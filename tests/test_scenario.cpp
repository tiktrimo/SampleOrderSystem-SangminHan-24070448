#include "TestRunner.h"
#include "src/model/Sample.h"
#include "src/model/Order.h"
#include "src/controller/SampleController.h"
#include "src/controller/OrderController.h"
#include "src/service/OrderService.h"
#include "src/service/ProductionService.h"
#include "src/service/MonitorService.h"
#include <cmath>

// ─── 헬퍼 ────────────────────────────────────────────────────────────────────

static Sample makeSample(const std::string& id, int stock, double yield = 0.9, double avg = 0.5) {
    Sample s; s.id=id; s.name="시료_"+id; s.stock=stock; s.yield=yield; s.avgProductionTimeMin=avg;
    return s;
}

// 승인 + 컨트롤러/서비스 동기화 헬퍼
static void doApprove(const std::string& orderId,
                      OrderController& oc, SampleController& sc, ProductionService& ps) {
    auto oOpt = oc.findById(orderId);
    auto sOpt = sc.findById(oOpt->sampleId);
    Order o = *oOpt; Sample s = *sOpt;
    int prevStock = s.stock;
    OrderService::approve(o, s);
    if (o.status == OrderStatus::PRODUCING) ps.enqueue(o, s, o.quantity - prevStock);
    oc.updateOrder(o);
    sc.updateStock(s.id, s.stock - prevStock);
}

// ─── Scenario 1: 재고 충분 — RESERVED → CONFIRMED → RELEASE ─────────────────

TEST(scenario_sufficient_stock_full_flow) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 100));

    std::string oid = oc.placeOrder("S-001", "삼성전자", 30);
    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::RESERVED);

    doApprove(oid, oc, sc, ps);
    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(sc.findById("S-001")->stock, 100); // 재고는 출고 시에만 차감
    ASSERT_EQ(ps.queueSize(), 0);

    Order o = *oc.findById(oid);
    OrderService::release(o);
    oc.updateOrder(o);
    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::RELEASE);
}

// ─── Scenario 2: 재고 부족 — RESERVED → PRODUCING → CONFIRMED → RELEASE ─────

TEST(scenario_insufficient_stock_production_full_flow) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 10));

    std::string oid = oc.placeOrder("S-001", "SK하이닉스", 50);
    doApprove(oid, oc, sc, ps);

    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::PRODUCING);
    ASSERT_EQ(sc.findById("S-001")->stock, 10); // 재고는 출고 시에만 차감
    ASSERT_EQ(ps.queueSize(), 1);

    // 생산 완료
    Order o = *oc.findById(oid);
    Sample s = *sc.findById("S-001");
    auto item = ps.complete(o, s);
    oc.updateOrder(o);
    sc.updateStock("S-001", item.actualProduction);

    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::CONFIRMED);
    ASSERT_GT(sc.findById("S-001")->stock, 0);
    ASSERT_EQ(ps.queueSize(), 0);

    // 출고
    Order o2 = *oc.findById(oid);
    OrderService::release(o2);
    oc.updateOrder(o2);
    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::RELEASE);
}

// ─── Scenario 3: 주문 거절 흐름 ──────────────────────────────────────────────

TEST(scenario_reject_flow) {
    SampleController sc; OrderController oc;
    sc.addSample(makeSample("S-001", 100));

    std::string oid = oc.placeOrder("S-001", "LG이노텍", 20);
    Order o = *oc.findById(oid);
    ASSERT_TRUE(OrderService::reject(o));
    oc.updateOrder(o);

    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::REJECTED);
    ASSERT_EQ(sc.findById("S-001")->stock, 100); // 재고 변화 없음

    // 거절된 주문은 재승인 불가
    Sample s = *sc.findById("S-001");
    ASSERT_FALSE(OrderService::approve(o, s));
}

// ─── Scenario 4: FIFO 생산 큐 — 3건 순서 보장 ───────────────────────────────

TEST(scenario_fifo_production_queue_three_orders) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 0)); // 재고 없음 → 모두 PRODUCING

    std::string o1 = oc.placeOrder("S-001", "고객A", 10);
    std::string o2 = oc.placeOrder("S-001", "고객B", 20);
    std::string o3 = oc.placeOrder("S-001", "고객C", 30);

    doApprove(o1, oc, sc, ps);
    doApprove(o2, oc, sc, ps);
    doApprove(o3, oc, sc, ps);

    ASSERT_EQ(ps.queueSize(), 3);
    ASSERT_EQ(ps.peek().orderId, o1); // FIFO: 첫 번째 주문이 앞

    // 순서대로 완료
    for (const auto& oid : {o1, o2, o3}) {
        Order o = *oc.findById(oid);
        Sample s = *sc.findById("S-001");
        auto item = ps.complete(o, s);
        oc.updateOrder(o);
        sc.updateStock("S-001", item.actualProduction);
        ASSERT_EQ(oc.findById(oid)->status, OrderStatus::CONFIRMED);
    }
    ASSERT_EQ(ps.queueSize(), 0);
}

// ─── Scenario 5: 연속 승인 — 재고 차감 없음, 출고 시 차감 ───────────────────

TEST(scenario_sequential_approval_no_stock_deduction) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 100));

    std::string o1 = oc.placeOrder("S-001", "A사", 60);
    std::string o2 = oc.placeOrder("S-001", "B사", 50);

    // 승인 시 재고 차감 없음
    doApprove(o1, oc, sc, ps);
    ASSERT_EQ(oc.findById(o1)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(sc.findById("S-001")->stock, 100); // 재고 유지

    doApprove(o2, oc, sc, ps);
    ASSERT_EQ(oc.findById(o2)->status, OrderStatus::CONFIRMED); // 100>=50 → CONFIRMED
    ASSERT_EQ(sc.findById("S-001")->stock, 100); // 여전히 차감 없음
    ASSERT_EQ(ps.queueSize(), 0);
}

// ─── Scenario 6: 다중 시료 모니터링 스냅샷 ───────────────────────────────────

TEST(scenario_multi_sample_monitor_snapshot) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 200));
    sc.addSample(makeSample("S-002", 0));   // 고갈 시료

    // S-001: 승인 2건 (CONFIRMED 1, PRODUCING 1)
    std::string oa = oc.placeOrder("S-001", "A사", 50);
    std::string ob = oc.placeOrder("S-001", "B사", 300); // 재고 초과 → PRODUCING
    doApprove(oa, oc, sc, ps);
    doApprove(ob, oc, sc, ps);

    // S-002: 거절 1건
    std::string oc_ = oc.placeOrder("S-002", "C사", 10);
    Order rej = *oc.findById(oc_);
    OrderService::reject(rej); oc.updateOrder(rej);

    auto snap = MonitorService::buildSnapshot(oc.getAll(), sc.getAll());

    // REJECTED 제외 집계
    ASSERT_TRUE(snap.orderCounts.find(OrderStatus::REJECTED) == snap.orderCounts.end()
             || snap.orderCounts[OrderStatus::REJECTED] == 0);
    ASSERT_EQ(snap.orderCounts[OrderStatus::CONFIRMED], 1);
    ASSERT_EQ(snap.orderCounts[OrderStatus::PRODUCING], 1);

    // S-002 고갈 확인
    ASSERT_EQ((int)snap.stockInfos.size(), 2);
    bool s2depleted = false;
    for (const auto& info : snap.stockInfos) {
        if (info.sample.id == "S-002")
            s2depleted = (info.stockStatus == StockStatus::DEPLETED);
    }
    ASSERT_TRUE(s2depleted);
}

// ─── Scenario 7: 생산 완료 공식 검증 — 실 생산량이 주문량 이상 ───────────────

TEST(scenario_production_output_covers_order_quantity) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 5, 0.8, 1.0)); // 소량 재고, 낮은 수율

    std::string oid = oc.placeOrder("S-001", "테스트고객", 100);
    doApprove(oid, oc, sc, ps);
    ASSERT_EQ(ps.queueSize(), 1);

    auto item = ps.peek();
    // stock=5, qty=100, 부족분=95, yield=0.8 -> ceil(95/(0.8*0.9)) = ceil(131.94) = 132
    ASSERT_EQ(item.actualProduction, 132);
    ASSERT_GT(item.actualProduction, item.orderQuantity); // 손실 고려해 주문량 초과
}

// ─── Scenario 8: OrderController::getAll 전체 목록 ───────────────────────────

TEST(scenario_order_controller_get_all) {
    OrderController oc;
    oc.placeOrder("S-001", "A", 10);
    oc.placeOrder("S-002", "B", 20);
    oc.placeOrder("S-003", "C", 30);

    auto all = oc.getAll();
    ASSERT_EQ((int)all.size(), 3);

    // 상태가 모두 RESERVED인지 확인
    for (const auto& o : all) {
        ASSERT_EQ(o.status, OrderStatus::RESERVED);
    }
}

// ─── Scenario 9: updateOrder 반영 검증 ───────────────────────────────────────

TEST(scenario_update_order_reflects_in_get_all) {
    OrderController oc;
    std::string oid = oc.placeOrder("S-001", "A사", 50);

    Order o = *oc.findById(oid);
    o.status = OrderStatus::CONFIRMED;
    oc.updateOrder(o);

    // getAll에 변경 반영 확인
    for (const auto& ord : oc.getAll()) {
        if (ord.orderId == oid)
            ASSERT_EQ(ord.status, OrderStatus::CONFIRMED);
    }
    // getByStatus로도 확인
    ASSERT_EQ((int)oc.getByStatus(OrderStatus::CONFIRMED).size(), 1);
    ASSERT_EQ((int)oc.getByStatus(OrderStatus::RESERVED).size(), 0);
}

// ─── Scenario 10: 주문 거절 후 재고 보존 + 모니터 제외 검증 ─────────────────

TEST(scenario_rejected_orders_excluded_from_monitor) {
    SampleController sc; OrderController oc;
    sc.addSample(makeSample("S-001", 50));

    std::string o1 = oc.placeOrder("S-001", "A사", 10);
    std::string o2 = oc.placeOrder("S-001", "B사", 20);
    std::string o3 = oc.placeOrder("S-001", "C사", 30);

    // o1 거절, o2 RESERVED 유지, o3 거절
    for (const auto& oid : {o1, o3}) {
        Order o = *oc.findById(oid);
        OrderService::reject(o); oc.updateOrder(o);
    }

    auto snap = MonitorService::buildSnapshot(oc.getAll(), sc.getAll());
    ASSERT_EQ(snap.orderCounts[OrderStatus::RESERVED], 1);
    ASSERT_TRUE(snap.orderCounts.find(OrderStatus::REJECTED) == snap.orderCounts.end()
             || snap.orderCounts[OrderStatus::REJECTED] == 0);

    // 거절 주문의 pending은 집계 제외 → B사(20ea)만 pending
    ASSERT_EQ(snap.stockInfos[0].pendingQuantity, 20);
    ASSERT_EQ(snap.stockInfos[0].stockStatus, StockStatus::SUFFICIENT); // 50>=20
}

// ─── Scenario 11: stringToOrderStatus 모든 값 + 예외 ────────────────────────

TEST(scenario_string_to_status_all_values) {
    ASSERT_EQ(stringToOrderStatus("RESERVED"),  OrderStatus::RESERVED);
    ASSERT_EQ(stringToOrderStatus("REJECTED"),  OrderStatus::REJECTED);
    ASSERT_EQ(stringToOrderStatus("PRODUCING"), OrderStatus::PRODUCING);
    ASSERT_EQ(stringToOrderStatus("CONFIRMED"), OrderStatus::CONFIRMED);
    ASSERT_EQ(stringToOrderStatus("RELEASE"),   OrderStatus::RELEASE);
}

TEST(scenario_string_to_status_invalid_throws) {
    bool threw = false;
    try { stringToOrderStatus("INVALID"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

// ─── Scenario 12: 재고 경계값 — 정확히 맞는 수량 ────────────────────────────

TEST(scenario_exact_stock_match_becomes_confirmed) {
    SampleController sc; OrderController oc; ProductionService ps;
    sc.addSample(makeSample("S-001", 50));

    std::string oid = oc.placeOrder("S-001", "A사", 50); // 정확히 일치
    doApprove(oid, oc, sc, ps);

    ASSERT_EQ(oc.findById(oid)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(sc.findById("S-001")->stock, 50); // 재고는 출고 시에만 차감
    ASSERT_EQ(ps.queueSize(), 0);
}
