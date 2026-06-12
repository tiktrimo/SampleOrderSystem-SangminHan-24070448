// ============================================================================
// test_integration.cpp
// 전체 시스템 통합 테스트 — 파일 영속성까지 포함한 엔드-투-엔드 시나리오
// ============================================================================
#include "TestRunner.h"
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include "src/controller/OrderController.h"
#include "src/controller/SampleController.h"
#include "src/repository/OrderRepository.h"
#include "src/repository/SampleRepository.h"
#include "src/service/OrderService.h"
#include "src/service/ProductionService.h"
#include "src/service/MonitorService.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <thread>
#include <chrono>

// ─── 공통 헬퍼 ───────────────────────────────────────────────────────────────

static std::string tmpFile(const std::string& name) {
    char* t = nullptr; size_t sz = 0;
    _dupenv_s(&t, &sz, "TEMP");
    std::string dir = (t && sz > 0) ? t : ".";
    free(t);
    return dir + "\\intg_" + name;
}

struct TestEnv {
    std::string sPath, oPath;
    SampleRepository sr;
    OrderRepository  or_;
    SampleController sc;
    OrderController  oc;
    ProductionService ps;

    explicit TestEnv(const std::string& tag)
        : sPath(tmpFile(tag + "_s.dat"))
        , oPath(tmpFile(tag + "_o.dat"))
        , sr(sPath), or_(oPath)
    {
        std::remove(sPath.c_str());
        std::remove(oPath.c_str());
        sr = SampleRepository(sPath);
        or_ = OrderRepository(oPath);
    }

    ~TestEnv() {
        std::remove(sPath.c_str());
        std::remove(oPath.c_str());
    }

    void addSample(const std::string& id, const std::string& name,
                   int stock, double yield = 0.9, double avgMin = 0.5) {
        Sample s; s.id = id; s.name = name;
        s.stock = stock; s.yield = yield; s.avgProductionTimeMin = avgMin;
        sc.addSample(s);
        sr.save(s);
    }

    // 승인: approve 후 컨트롤러·리포지터리 동기화
    void approve(const std::string& orderId) {
        auto oOpt = oc.findById(orderId);
        auto sOpt = sc.findById(oOpt->sampleId);
        Order o = *oOpt; Sample s = *sOpt;
        int prevStock = s.stock;
        bool producing = (s.stock < o.quantity);
        int shortage   = producing ? (o.quantity - s.stock) : 0;
        o.shortage = shortage;
        OrderService::approve(o, s);
        if (producing) ps.enqueue(o, s, shortage);
        oc.updateOrder(o);
        or_.update(o);
    }

    // 생산 완료: 완료 후 컨트롤러·리포지터리 동기화
    void completeProduction(const std::string& orderId) {
        auto oOpt = oc.findById(orderId);
        auto sOpt = sc.findById(oOpt->sampleId);
        Order o = *oOpt; Sample s = *sOpt;
        auto done = ps.complete(o, s);
        oc.updateOrder(o);
        sc.updateStock(s.id, done.actualProduction);
        auto sUpdated = sc.findById(s.id);
        or_.update(o);
        sr.update(*sUpdated);
    }

    // 출고: release 후 재고 차감 및 저장
    void release(const std::string& orderId) {
        auto oOpt = oc.findById(orderId);
        Order o = *oOpt;
        OrderService::release(o);
        oc.updateOrder(o);
        or_.update(o);
        sc.updateStock(o.sampleId, -o.quantity);
        auto sUpdated = sc.findById(o.sampleId);
        sr.update(*sUpdated);
    }
};

// ─── 1. 재고 충분 전체 흐름: 파일 영속성 포함 ────────────────────────────────

TEST(intg_sufficient_stock_full_lifecycle_with_persistence) {
    TestEnv env("intg01");
    env.addSample("S-001", "GaN Wafer", 100, 0.9, 0.5);

    std::string oid = env.oc.placeOrder("S-001", "삼성전자", 30);
    env.or_.save(*env.oc.findById(oid));

    // RESERVED 상태 파일 확인
    {
        OrderRepository reload(env.oPath);
        auto found = reload.findById(oid);
        ASSERT_TRUE(found.has_value());
        ASSERT_EQ(found->status, OrderStatus::RESERVED);
    }

    env.approve(oid);
    ASSERT_EQ(env.oc.findById(oid)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100); // 승인 시 재고 불변

    // CONFIRMED 상태 파일 확인
    {
        OrderRepository reload(env.oPath);
        ASSERT_EQ(reload.findById(oid)->status, OrderStatus::CONFIRMED);
    }

    env.release(oid);
    ASSERT_EQ(env.oc.findById(oid)->status, OrderStatus::RELEASE);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 70); // 100 - 30

    // RELEASE 상태 + 재고 파일 확인
    {
        OrderRepository oreload(env.oPath);
        SampleRepository sreload(env.sPath);
        ASSERT_EQ(oreload.findById(oid)->status, OrderStatus::RELEASE);
        ASSERT_EQ(sreload.findById("S-001")->stock, 70);
    }
}

// ─── 2. 재고 부족 전체 흐름: 생산 완료 후 재고 공식 검증 ─────────────────────

TEST(intg_insufficient_stock_production_stock_formula) {
    TestEnv env("intg02");
    // stock=20, qty=100, 부족분=80, yield=0.9
    // actualProduction = ceil(80 / (0.9 * 0.9)) = ceil(98.77) = 99
    env.addSample("S-001", "SiC", 20, 0.9, 0.1);

    std::string oid = env.oc.placeOrder("S-001", "SK하이닉스", 100);
    env.or_.save(*env.oc.findById(oid));

    env.approve(oid);
    ASSERT_EQ(env.oc.findById(oid)->status, OrderStatus::PRODUCING);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 20); // 승인 시 재고 불변
    ASSERT_EQ(env.ps.queueSize(), 1);

    auto item = env.ps.peek();
    int expected = (int)std::ceil(80.0 / (0.9 * 0.9)); // = 99
    ASSERT_EQ(item.actualProduction, expected);
    ASSERT_EQ(item.shortage, 80);

    env.completeProduction(oid);
    ASSERT_EQ(env.oc.findById(oid)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 20 + expected); // 생산 완료 후 재고 증가

    env.release(oid);
    // 출고: stock - orderQuantity (not actualProduction)
    ASSERT_EQ(env.sc.findById("S-001")->stock, 20 + expected - 100);
    ASSERT_GT(env.sc.findById("S-001")->stock, 0); // 잉여 재고 남아야 함

    // 파일 최종 상태 검증
    {
        SampleRepository sreload(env.sPath);
        ASSERT_EQ(sreload.findById("S-001")->stock, 20 + expected - 100);
    }
}

// ─── 3. 출고량은 주문량 기준 (실 생산량이 아님) ──────────────────────────────

TEST(intg_release_deducts_order_qty_not_actual_production) {
    TestEnv env("intg03");
    // stock=5, qty=50, 부족분=45, yield=0.8
    // actualProduction = ceil(45 / (0.8 * 0.9)) = ceil(62.5) = 63
    env.addSample("S-001", "GaAs", 5, 0.8, 0.1);

    std::string oid = env.oc.placeOrder("S-001", "인텔코리아", 50);
    env.or_.save(*env.oc.findById(oid));
    env.approve(oid);

    auto item = env.ps.peek();
    ASSERT_EQ(item.actualProduction, (int)std::ceil(45.0 / (0.8 * 0.9)));

    env.completeProduction(oid);
    int stockAfterProd = env.sc.findById("S-001")->stock;
    ASSERT_EQ(stockAfterProd, 5 + item.actualProduction);

    env.release(oid);
    // 출고 시 order.quantity(50)만 차감 — actualProduction(63)이 아님
    ASSERT_EQ(env.sc.findById("S-001")->stock, stockAfterProd - 50);
    ASSERT_NE(env.sc.findById("S-001")->stock, stockAfterProd - item.actualProduction);
}

// ─── 4. 재시작 복원: PRODUCING 주문이 파일에서 올바르게 복원됨 ───────────────

TEST(intg_restart_restores_producing_orders_from_file) {
    TestEnv env("intg04");
    // stock=10, qty=100 → PRODUCING
    env.addSample("S-001", "InP", 10, 0.85, 0.2);

    std::string oid = env.oc.placeOrder("S-001", "퀄컴코리아", 100);
    env.or_.save(*env.oc.findById(oid));
    env.approve(oid);
    ASSERT_EQ(env.oc.findById(oid)->shortage, 90); // shortage 필드 검증

    // 재시작 시뮬레이션: 새 컨트롤러를 파일에서 로드
    SampleController sc2; OrderController oc2; ProductionService ps2;
    for (const auto& s : env.sr.findAll()) sc2.addSample(s);
    for (const auto& o : env.or_.findAll()) {
        // OrderController에 직접 로드 (placeOrder 우회)
        oc2.orders_.push_back(o);
    }
    oc2.syncSequence();

    // PRODUCING 주문 큐 복원
    for (const auto& o : oc2.getByStatus(OrderStatus::PRODUCING)) {
        auto s = sc2.findById(o.sampleId);
        int restoreShortage = o.shortage > 0 ? o.shortage : o.quantity;
        ASSERT_TRUE(s.has_value());
        ps2.enqueue(o, *s, restoreShortage);
    }

    // 복원 후 검증
    ASSERT_EQ(ps2.queueSize(), 1);
    auto item = ps2.peek();
    ASSERT_EQ(item.orderId, oid);
    ASSERT_EQ(item.shortage, 90);
    ASSERT_EQ(item.actualProduction, (int)std::ceil(90.0 / (0.85 * 0.9)));
}

// ─── 5. shortage 필드 파일 왕복 직렬화 검증 ──────────────────────────────────

TEST(intg_shortage_survives_file_round_trip) {
    TestEnv env("intg05");
    env.addSample("S-001", "GaN", 15, 0.9, 0.3);

    std::string oid = env.oc.placeOrder("S-001", "마이크론코리아", 80);
    env.or_.save(*env.oc.findById(oid));
    env.approve(oid);
    // shortage = 80 - 15 = 65

    // 파일에서 다시 읽어 shortage 검증
    OrderRepository reload(env.oPath);
    auto found = reload.findById(oid);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, OrderStatus::PRODUCING);
    ASSERT_EQ(found->shortage, 65);
}

// ─── 6. 다중 주문, 동일 시료: 승인 순서별 상태 + 재고 검증 ───────────────────

TEST(intg_multi_order_same_sample_stock_consistency) {
    TestEnv env("intg06");
    env.addSample("S-001", "SiC", 100, 0.9, 0.1);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 40);
    std::string o2 = env.oc.placeOrder("S-001", "B사", 70); // 승인 당시 재고 = 100
    std::string o3 = env.oc.placeOrder("S-001", "C사", 30);
    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));
    env.or_.save(*env.oc.findById(o3));

    // 승인 시 재고 차감 없음 → 모두 CONFIRMED
    env.approve(o1); // stock=100 >= qty=40 → CONFIRMED
    ASSERT_EQ(env.oc.findById(o1)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100);

    env.approve(o2); // stock=100 >= qty=70 → CONFIRMED (승인 기준 재고 차감 없음)
    ASSERT_EQ(env.oc.findById(o2)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100);

    env.approve(o3); // stock=100 >= qty=30 → CONFIRMED
    ASSERT_EQ(env.oc.findById(o3)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100);
    ASSERT_EQ(env.ps.queueSize(), 0);

    // 출고 순서대로 재고 차감: 40 → 60 → 30
    env.release(o1);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 60);
    env.release(o2);
    ASSERT_EQ(env.sc.findById("S-001")->stock, -10); // 재고 음수 허용 (비즈니스 규칙 범위 외)
    env.release(o3);
    ASSERT_EQ(env.sc.findById("S-001")->stock, -40);
}

// ─── 7. FIFO 생산 큐: 3건 순서 보장 + 각 완료 후 재고 누적 검증 ──────────────

TEST(intg_fifo_queue_three_orders_stock_accumulation) {
    TestEnv env("intg07");
    env.addSample("S-001", "GaAs", 0, 0.9, 0.1); // 재고 0 → 모두 PRODUCING

    std::string o1 = env.oc.placeOrder("S-001", "A사", 10);
    std::string o2 = env.oc.placeOrder("S-001", "B사", 20);
    std::string o3 = env.oc.placeOrder("S-001", "C사", 30);
    for (auto& id : {o1, o2, o3}) env.or_.save(*env.oc.findById(id));

    env.approve(o1); env.approve(o2); env.approve(o3);
    ASSERT_EQ(env.ps.queueSize(), 3);
    ASSERT_EQ(env.ps.peek().orderId, o1); // FIFO 순서

    // o1 완료
    int act1 = env.ps.peek().actualProduction;
    env.completeProduction(o1);
    ASSERT_EQ(env.oc.findById(o1)->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(env.sc.findById("S-001")->stock, act1);
    ASSERT_EQ(env.ps.queueSize(), 2);
    ASSERT_EQ(env.ps.peek().orderId, o2); // o2가 다음 순서

    // o2 완료
    int act2 = env.ps.peek().actualProduction;
    env.completeProduction(o2);
    ASSERT_EQ(env.sc.findById("S-001")->stock, act1 + act2);

    // o3 완료
    int act3 = env.ps.peek().actualProduction;
    env.completeProduction(o3);
    ASSERT_EQ(env.sc.findById("S-001")->stock, act1 + act2 + act3);
    ASSERT_EQ(env.ps.queueSize(), 0);
}

// ─── 8. 거절 흐름: 파일 영속성 + 모니터 제외 검증 ───────────────────────────

TEST(intg_reject_persisted_and_excluded_from_monitor) {
    TestEnv env("intg08");
    env.addSample("S-001", "Si Wafer", 100, 0.9, 0.5);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 20);
    std::string o2 = env.oc.placeOrder("S-001", "B사", 30);
    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));

    // o1 거절
    Order rej = *env.oc.findById(o1);
    ASSERT_TRUE(OrderService::reject(rej));
    env.oc.updateOrder(rej);
    env.or_.update(rej);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100); // 재고 불변

    // 파일에 거절 상태 저장 검증
    {
        OrderRepository reload(env.oPath);
        ASSERT_EQ(reload.findById(o1)->status, OrderStatus::REJECTED);
    }

    // 거절된 주문은 모니터링에서 제외
    auto snap = MonitorService::buildSnapshot(env.oc.getAll(), env.sc.getAll());
    ASSERT_TRUE(snap.orderCounts.find(OrderStatus::REJECTED) == snap.orderCounts.end()
             || snap.orderCounts[OrderStatus::REJECTED] == 0);
    ASSERT_EQ(snap.orderCounts[OrderStatus::RESERVED], 1); // o2만 카운트

    // o2의 pending으로만 재고 상태 분류
    ASSERT_EQ(snap.stockInfos[0].pendingQuantity, 30);
    ASSERT_EQ(snap.stockInfos[0].stockStatus, StockStatus::SUFFICIENT); // 100 >= 30
}

// ─── 9. 모니터 스냅샷 정확성: 생산 중 + CONFIRMED 혼재 ────────────────────────

TEST(intg_monitor_snapshot_mixed_states_accuracy) {
    TestEnv env("intg09");
    env.addSample("S-001", "InGaAs", 50, 0.9, 0.1);
    env.addSample("S-002", "GaN", 0, 0.85, 0.2);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 30); // 충분 → CONFIRMED
    std::string o2 = env.oc.placeOrder("S-001", "B사", 80); // 부족 → PRODUCING
    std::string o3 = env.oc.placeOrder("S-002", "C사", 20); // 재고 0 → PRODUCING

    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));
    env.or_.save(*env.oc.findById(o3));

    env.approve(o1); env.approve(o2); env.approve(o3);

    auto snap = MonitorService::buildSnapshot(env.oc.getAll(), env.sc.getAll());

    ASSERT_EQ(snap.orderCounts[OrderStatus::CONFIRMED], 1);
    ASSERT_EQ(snap.orderCounts[OrderStatus::PRODUCING], 2);

    // S-001: stock=50, pending = PRODUCING(80) → LOW
    // S-002: stock=0 → DEPLETED
    bool s1Low = false, s2Depleted = false;
    for (const auto& info : snap.stockInfos) {
        if (info.sample.id == "S-001") s1Low      = (info.stockStatus == StockStatus::LOW);
        if (info.sample.id == "S-002") s2Depleted = (info.stockStatus == StockStatus::DEPLETED);
    }
    ASSERT_TRUE(s1Low);
    ASSERT_TRUE(s2Depleted);
}

// ─── 10. 연속 생산 완료 후 재고 파일 일관성 ─────────────────────────────────

TEST(intg_sequential_production_complete_file_consistency) {
    TestEnv env("intg10");
    env.addSample("S-001", "SiGe", 5, 0.9, 0.1);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 50); // 부족분 45
    std::string o2 = env.oc.placeOrder("S-001", "B사", 20);
    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));

    env.approve(o1);
    // o1 완료 후 재고 증가 → o2 승인 시 재고 충분 여부 결정
    int act1 = env.ps.peek().actualProduction;
    env.completeProduction(o1);
    int stockAfterO1 = env.sc.findById("S-001")->stock;
    ASSERT_EQ(stockAfterO1, 5 + act1);

    env.approve(o2);
    // 재고 = 5 + act1 (>= 20이면 CONFIRMED, < 20이면 PRODUCING)
    if (stockAfterO1 >= 20) {
        ASSERT_EQ(env.oc.findById(o2)->status, OrderStatus::CONFIRMED);
        ASSERT_EQ(env.ps.queueSize(), 0);
    } else {
        ASSERT_EQ(env.oc.findById(o2)->status, OrderStatus::PRODUCING);
        ASSERT_EQ(env.ps.queueSize(), 1);
    }

    // 파일 재고와 인메모리 재고 일치
    SampleRepository sreload(env.sPath);
    ASSERT_EQ(sreload.findById("S-001")->stock, env.sc.findById("S-001")->stock);
}

// ─── 11. 주문번호 시퀀스 동기화: 재시작 후 중복 없음 ────────────────────────

TEST(intg_order_sequence_no_duplicate_after_restart) {
    TestEnv env("intg11");
    env.addSample("S-001", "AlGaN", 100, 0.9, 0.5);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 10);
    std::string o2 = env.oc.placeOrder("S-001", "B사", 10);
    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));

    // 재시작: 새 OrderController
    OrderController oc2;
    for (const auto& o : env.or_.findAll()) oc2.orders_.push_back(o);
    oc2.syncSequence();

    // 재시작 후 새 주문 번호가 기존과 겹치지 않아야 함
    std::string o3 = oc2.placeOrder("S-001", "C사", 10);
    ASSERT_NE(o3, o1);
    ASSERT_NE(o3, o2);
    // 시퀀스 번호가 기존 최대값보다 커야 함
    ASSERT_GT(o3, o2); // 문자열 비교 — 같은 날짜라면 ORD-DATE-NNNN 형식으로 순서 보장
}

// ─── 12. 전체 재고 계산: getTotalStock은 모든 시료 합산 ──────────────────────

TEST(intg_total_stock_sums_all_samples) {
    TestEnv env("intg12");
    env.addSample("S-001", "GaN",  100, 0.9, 0.5);
    env.addSample("S-002", "SiC",   50, 0.8, 0.3);
    env.addSample("S-003", "InP",    0, 0.7, 1.0);

    ASSERT_EQ(env.sc.getTotalStock(), 150);

    // S-001에서 출고 30ea
    env.sc.updateStock("S-001", -30);
    ASSERT_EQ(env.sc.getTotalStock(), 120);

    // S-002 생산 완료로 +25ea
    env.sc.updateStock("S-002", 25);
    ASSERT_EQ(env.sc.getTotalStock(), 145);
}

// ─── 13. 생산 공식 경계값: 부족분 정확히 1ea ─────────────────────────────────

TEST(intg_production_formula_shortage_of_one) {
    TestEnv env("intg13");
    // stock=99, qty=100, 부족분=1, yield=0.9
    // actualProduction = ceil(1 / (0.9 * 0.9)) = ceil(1.234) = 2
    env.addSample("S-001", "GaAs", 99, 0.9, 0.1);

    std::string oid = env.oc.placeOrder("S-001", "TSMC코리아", 100);
    env.or_.save(*env.oc.findById(oid));
    env.approve(oid);

    ASSERT_EQ(env.oc.findById(oid)->status, OrderStatus::PRODUCING);
    ASSERT_EQ(env.ps.peek().shortage, 1);
    ASSERT_EQ(env.ps.peek().actualProduction, (int)std::ceil(1.0 / (0.9 * 0.9)));
}

// ─── 14. 생산 완료 → 출고 후 잉여 재고 남음 검증 ────────────────────────────

TEST(intg_surplus_stock_after_production_and_release) {
    TestEnv env("intg14");
    // stock=0, qty=10, yield=0.5
    // actualProduction = ceil(10 / (0.5 * 0.9)) = ceil(22.22) = 23
    // 출고 후 재고 = 0 + 23 - 10 = 13
    env.addSample("S-001", "Diamond", 0, 0.5, 0.1);

    std::string oid = env.oc.placeOrder("S-001", "보쉬코리아", 10);
    env.or_.save(*env.oc.findById(oid));
    env.approve(oid);

    int actual = env.ps.peek().actualProduction;
    ASSERT_EQ(actual, (int)std::ceil(10.0 / (0.5 * 0.9)));

    env.completeProduction(oid);
    ASSERT_EQ(env.sc.findById("S-001")->stock, actual); // 생산 완료 후 재고

    env.release(oid);
    ASSERT_EQ(env.sc.findById("S-001")->stock, actual - 10); // 잉여 재고
    ASSERT_GT(env.sc.findById("S-001")->stock, 0);

    // 파일 검증
    SampleRepository sreload(env.sPath);
    ASSERT_EQ(sreload.findById("S-001")->stock, actual - 10);
}

// ─── 15. 거절된 주문은 재승인 불가 + 재고 불변 ───────────────────────────────

TEST(intg_rejected_order_cannot_be_reapproved) {
    TestEnv env("intg15");
    env.addSample("S-001", "CdTe", 100, 0.9, 0.5);

    std::string oid = env.oc.placeOrder("S-001", "독일고객", 50);
    env.or_.save(*env.oc.findById(oid));

    Order o = *env.oc.findById(oid);
    OrderService::reject(o);
    env.oc.updateOrder(o);
    env.or_.update(o);

    // 거절 후 재승인 시도
    Sample s = *env.sc.findById("S-001");
    bool reapproved = OrderService::approve(o, s);
    ASSERT_FALSE(reapproved);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 100); // 재고 불변
    ASSERT_EQ(env.ps.queueSize(), 0);
}

// ─── 16. 다중 시료 독립적 생산 큐 동시 진행 ─────────────────────────────────

TEST(intg_multi_sample_independent_production) {
    TestEnv env("intg16");
    env.addSample("S-001", "GaN",  5, 0.9, 0.1);
    env.addSample("S-002", "SiC", 10, 0.8, 0.1);

    std::string o1 = env.oc.placeOrder("S-001", "A사", 50); // 부족분 45
    std::string o2 = env.oc.placeOrder("S-002", "B사", 40); // 부족분 30
    env.or_.save(*env.oc.findById(o1));
    env.or_.save(*env.oc.findById(o2));

    env.approve(o1); env.approve(o2);
    ASSERT_EQ(env.ps.queueSize(), 2);

    // FIFO: o1이 먼저 완료
    int act1 = env.ps.peek().actualProduction;
    env.completeProduction(o1);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 5 + act1);
    ASSERT_EQ(env.sc.findById("S-002")->stock, 10); // S-002 재고 불변

    int act2 = env.ps.peek().actualProduction;
    env.completeProduction(o2);
    ASSERT_EQ(env.sc.findById("S-002")->stock, 10 + act2);
    ASSERT_EQ(env.sc.findById("S-001")->stock, 5 + act1); // S-001 재고 불변
}
