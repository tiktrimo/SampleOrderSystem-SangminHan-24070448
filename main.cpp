#include <windows.h>
#include <iostream>
#include "src/model/Sample.h"
#include "src/model/Order.h"
#include "src/controller/SampleController.h"
#include "src/controller/OrderController.h"
#include "src/service/OrderService.h"
#include "src/service/ProductionService.h"
#include "src/service/MonitorService.h"
#include "src/repository/SampleRepository.h"
#include "src/repository/OrderRepository.h"
#include "src/view/ConsoleView.h"

static void menuSampleManage(SampleController& sc, SampleRepository& sr) {
    std::cout << "\n[1] 시료관리 - (1)등록 (2)조회 (3)검색 > ";
    int sub = 0; std::cin >> sub; std::cin.ignore();
    if (sub == 1) {
        Sample s;
        s.id = ConsoleView::promptString("시료 ID");
        s.name = ConsoleView::promptString("이름");
        s.avgProductionTimeMin = ConsoleView::promptDouble("평균 생산시간(min)");
        s.yield = ConsoleView::promptDouble("수율(0~1)");
        s.stock = ConsoleView::promptInt("초기 재고");
        sc.addSample(s);
        sr.save(s);
        std::cout << "등록 완료: " << s.id << "\n";
    } else if (sub == 2) {
        ConsoleView::showSamples(sc.getAll());
    } else if (sub == 3) {
        auto kw = ConsoleView::promptString("검색 키워드");
        ConsoleView::showSamples(sc.searchByName(kw));
    }
}

static void menuPlaceOrder(OrderController& oc, SampleController& sc,
                           OrderRepository& or_) {
    std::cout << "\n[ 주문 접수 ]\n";
    ConsoleView::showSamples(sc.getAll());
    auto sid = ConsoleView::promptString("시료 ID");
    auto cust = ConsoleView::promptString("고객명");
    int qty = ConsoleView::promptInt("수량");
    auto id = oc.placeOrder(sid, cust, qty);
    auto found = oc.findById(id);
    if (found) or_.save(*found);
    std::cout << "접수 완료: " << id << " [RESERVED]\n";
}

static void menuApproveReject(OrderController& oc, SampleController& sc,
                              OrderRepository& or_, SampleRepository& sr,
                              ProductionService& ps) {
    auto reserved = oc.getByStatus(OrderStatus::RESERVED);
    if (reserved.empty()) { std::cout << "\n대기 주문 없음\n"; return; }
    ConsoleView::showOrders(reserved);
    auto oid = ConsoleView::promptString("주문번호");
    std::cout << "(1)승인  (2)거절 > ";
    int ch = 0; std::cin >> ch; std::cin.ignore();

    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "주문 없음\n"; return; }
    Order o = *oFound;

    if (ch == 1) {
        auto sFound = sc.findById(o.sampleId);
        if (!sFound) { std::cout << "시료 없음\n"; return; }
        Sample s = *sFound;
        bool producing = (s.stock < o.quantity);
        OrderService::approve(o, s);
        if (producing) ps.enqueue(o, s);
        sc.updateStock(s.id, s.stock - sFound->stock);
        sr.update(s);
        // oc에 직접 반영
        oc.updateOrder(o);
        or_.update(o);
        std::cout << "승인 완료: " << orderStatusToString(o.status) << "\n";
    } else {
        OrderService::reject(o);
        oc.updateOrder(o);
        or_.update(o);
        std::cout << "거절 완료\n";
    }
}

static void menuProductionLine(OrderController& oc, SampleController& sc,
                               OrderRepository& or_, SampleRepository& sr,
                               ProductionService& ps) {
    ConsoleView::showProductionQueue(ps);
    if (!ps.hasNext()) return;
    std::cout << "\n현재 생산 완료 처리? (1=예) > ";
    int ch = 0; std::cin >> ch; std::cin.ignore();
    if (ch != 1) return;
    auto item = ps.peek();
    auto oFound = oc.findById(item.orderId);
    auto sFound = sc.findById(item.sampleId);
    if (!oFound || !sFound) { std::cout << "데이터 오류\n"; return; }
    Order o = *oFound; Sample s = *sFound;
    ps.complete(o, s);
    for (auto& ord : oc.orders_) {
        if (ord.orderId == o.orderId) { ord = o; break; }
    }
    sc.updateStock(s.id, item.actualProduction);
    or_.update(o);
    sr.update(s);
    std::cout << "생산 완료: " << o.orderId << " -> CONFIRMED\n";
}

static void menuRelease(OrderController& oc, OrderRepository& or_) {
    auto confirmed = oc.getByStatus(OrderStatus::CONFIRMED);
    if (confirmed.empty()) { std::cout << "\n출고 대기 주문 없음\n"; return; }
    ConsoleView::showOrders(confirmed);
    auto oid = ConsoleView::promptString("출고할 주문번호");
    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "주문 없음\n"; return; }
    Order o = *oFound;
    if (!OrderService::release(o)) { std::cout << "출고 불가 상태\n"; return; }
    for (auto& ord : oc.orders_) {
        if (ord.orderId == oid) { ord = o; break; }
    }
    or_.update(o);
    std::cout << "출고 완료: " << oid << " -> RELEASE\n";
}

int main() {
    SetConsoleOutputCP(65001);

    // data 디렉터리 생성
    CreateDirectoryA("data", nullptr);

    SampleRepository sr("data/samples.dat");
    OrderRepository  or_("data/orders.dat");

    SampleController sc;
    for (const auto& s : sr.findAll()) sc.addSample(s);

    OrderController oc;
    for (const auto& o : or_.findAll()) {
        oc.orders_.push_back(o);  // 파일에서 복원
    }

    ProductionService ps;
    for (const auto& o : oc.getByStatus(OrderStatus::PRODUCING)) {
        auto s = sc.findById(o.sampleId);
        if (s) ps.enqueue(o, *s);
    }

    int choice = -1;
    while (choice != 0) {
        auto snap = MonitorService::buildSnapshot(oc.getAll(), sc.getAll());
        int producing = static_cast<int>(oc.getByStatus(OrderStatus::PRODUCING).size());
        ConsoleView::showMainMenu(sc.getSampleCount(), sc.getTotalStock(),
                                  oc.getOrderCount(), producing);
        std::cin >> choice; std::cin.ignore();
        switch (choice) {
        case 1: menuSampleManage(sc, sr); break;
        case 2: menuPlaceOrder(oc, sc, or_); break;
        case 3: menuApproveReject(oc, sc, or_, sr, ps); break;
        case 4: ConsoleView::showMonitor(snap); break;
        case 5: menuProductionLine(oc, sc, or_, sr, ps); break;
        case 6: menuRelease(oc, or_); break;
        case 0: std::cout << "종료합니다.\n"; break;
        default: std::cout << "잘못된 입력\n";
        }
    }
    return 0;
}
