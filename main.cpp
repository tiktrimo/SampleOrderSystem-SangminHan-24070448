#define NOMINMAX
#include <windows.h>
#include <conio.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <ctime>
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

// ── 인라인 색상 상수 ─────────────────────────────────────────────────────────
namespace {
    constexpr const char* C_RST  = "\033[0m";
    constexpr const char* C_BOLD = "\033[1m";
    constexpr const char* C_DIM  = "\033[2;37m";
    constexpr const char* C_RED  = "\033[31m";
    constexpr const char* C_GRN  = "\033[32m";
    constexpr const char* C_YLW  = "\033[33m";
    constexpr const char* C_BLU  = "\033[34m";
    constexpr const char* C_CYN  = "\033[36m";
    constexpr const char* C_BWHT = "\033[1;37m";
    constexpr const char* C_BCYN = "\033[1;36m";
}

// ── 메뉴 함수 ─────────────────────────────────────────────────────────────────

static void menuSampleManage(SampleController& sc, SampleRepository& sr) {
    std::cout << "\n" << C_BCYN << "▸ " << C_BWHT << "시료 관리"  << C_RST
              << C_DIM  << "  (1)등록  (2)조회  (3)검색  (0)뒤로" << C_RST << " " << C_CYN << "›" << C_RST << " ";
    int sub = 0; std::cin >> sub; std::cin.ignore();

    if (sub == 0) return;
    if (sub == 1) {
        std::cout << "\n" << C_BWHT << "  [ 시료 등록 ]" << C_RST << "\n";
        Sample s;
        s.id                  = ConsoleView::promptString("시료 ID");
        s.name                = ConsoleView::promptString("이름");
        s.avgProductionTimeMin = ConsoleView::promptDouble("평균 생산시간(min)");
        s.yield               = ConsoleView::promptDouble("수율(0~1)");
        s.stock               = ConsoleView::promptInt("초기 재고");
        sc.addSample(s);
        sr.save(s);
        std::cout << "  " << C_GRN << "✓ 등록 완료" << C_RST << "  " << C_BOLD << s.id << C_RST << "\n";
    } else if (sub == 2) {
        ConsoleView::showSamples(sc.getAll());
    } else if (sub == 3) {
        auto kw = ConsoleView::promptString("검색 키워드");
        ConsoleView::showSamples(sc.searchByName(kw));
    }
}

static void menuPlaceOrder(OrderController& oc, SampleController& sc,
                           OrderRepository& or_) {
    std::cout << "\n" << C_BCYN << "▸ " << C_BWHT << "주문 접수" << C_RST
              << C_DIM << "  (0 입력 시 뒤로)" << C_RST << "\n";
    ConsoleView::showSamples(sc.getAll());

    auto sid  = ConsoleView::promptString("시료 ID  (0=뒤로)");
    if (sid == "0") return;
    auto cust = ConsoleView::promptString("고객명");
    int  qty  = ConsoleView::promptInt("수량");

    auto sFound = sc.findById(sid);
    if (!sFound) {
        std::cout << "  " << C_RED << "✗ 시료 없음: " << C_BOLD << sid << C_RST << "\n";
        return;
    }
    std::cout << "\n" << C_BWHT << "  [ 입력 확인 ]" << C_RST << "\n";
    std::cout << "  " << C_DIM << "시료" << C_RST << "  " << sFound->name
              << " " << C_DIM << "(" << sid << ")" << C_RST << "\n";
    std::cout << "  " << C_DIM << "고객" << C_RST << "  " << cust << "\n";
    std::cout << "  " << C_DIM << "수량" << C_RST << "  " << C_BOLD << qty << "ea" << C_RST << "\n";
    std::cout << "\n  " << C_BWHT << "[Y]" << C_RST << " 예약 접수  "
              << C_DIM  << "[N]" << C_RST << " 취소 " << C_CYN << "›" << C_RST << " ";
    char yn = 0; std::cin >> yn; std::cin.ignore();
    if (yn != 'Y' && yn != 'y') {
        std::cout << "  " << C_DIM << "취소됨" << C_RST << "\n";
        return;
    }

    auto id = oc.placeOrder(sid, cust, qty);
    auto found = oc.findById(id);
    if (found) or_.save(*found);
    std::cout << "  " << C_GRN << "✓ 접수 완료" << C_RST << "  "
              << C_BOLD << id << C_RST << "  "
              << C_YLW << "● RESERVED" << C_RST << "\n";
}

static void menuApproveReject(OrderController& oc, SampleController& sc,
                              OrderRepository& or_, SampleRepository& sr,
                              ProductionService& ps) {
    auto reserved = oc.getByStatus(OrderStatus::RESERVED);
    if (reserved.empty()) {
        std::cout << "\n  " << C_DIM << "대기 주문 없음" << C_RST << "\n";
        return;
    }

    std::cout << "\n" << C_BCYN << "▸ " << C_BWHT << "승인 대기 목록" << C_RST
              << C_DIM << "  RESERVED " << reserved.size() << "건" << C_RST << "\n";
    // 헤더
    std::cout << C_DIM
              << std::left << std::setw(6)  << "번호"
              << std::setw(22) << "주문번호"
              << std::setw(12) << "시료ID"
              << std::setw(18) << "고객명"
              << "수량" << C_RST << "\n";
    std::cout << C_DIM;
    for (int i = 0; i < 64; i++) std::cout << "─";
    std::cout << C_RST << "\n";
    for (int i = 0; i < (int)reserved.size(); i++) {
        const auto& o = reserved[i];
        std::cout << C_BWHT << std::left << std::setw(6) << ("[" + std::to_string(i+1) + "]") << C_RST
                  << std::setw(22) << o.orderId
                  << std::setw(12) << o.sampleId
                  << std::setw(18) << o.customerName
                  << o.quantity << "ea\n";
    }

    int idx = ConsoleView::promptInt("승인/거절할 번호  (0=뒤로)");
    if (idx == 0) return;
    if (idx < 1 || idx > (int)reserved.size()) {
        std::cout << "  " << C_RED << "✗ 잘못된 번호" << C_RST << "\n";
        return;
    }
    std::string oid = reserved[idx - 1].orderId;

    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "  " << C_RED << "✗ 주문 없음" << C_RST << "\n"; return; }
    Order o = *oFound;

    auto sFound = sc.findById(o.sampleId);
    if (!sFound) { std::cout << "  " << C_RED << "✗ 시료 없음" << C_RST << "\n"; return; }
    Sample s = *sFound;
    const int origStock = s.stock; // approve 전 재고 저장 (부족분 계산용)

    std::cout << "\n  " << C_DIM << "재고 확인 중..." << C_RST << "\n";
    std::cout << "  " << C_DIM << "시료    " << C_RST << "  " << s.name
              << " " << C_DIM << "(" << s.id << ")" << C_RST << "\n";
    std::cout << "  " << C_DIM << "현재 재고" << C_RST << "  " << C_BOLD << s.stock << "ea" << C_RST << "\n";
    std::cout << "  " << C_DIM << "주문 수량" << C_RST << "  " << C_BOLD << o.quantity << "ea" << C_RST << "\n";

    char yn = 0;
    if (s.stock >= o.quantity) {
        std::cout << "\n  " << C_GRN << "재고 충분" << C_RST
                  << "  " << C_BWHT << "[Y]" << C_RST << " 예약 처리  "
                  << C_DIM  << "[N]" << C_RST << " 취소 " << C_CYN << "›" << C_RST << " ";
        std::cin >> yn; std::cin.ignore();
        if (yn != 'Y' && yn != 'y') {
            std::cout << "  " << C_DIM << "취소됨" << C_RST << "\n";
            return;
        }
    } else {
        int shortage = o.quantity - origStock;
        int actualProd = ProductionService::calcActualProduction(shortage, s.yield);
        double totalTime = ProductionService::calcTotalTime(s.avgProductionTimeMin, actualProd);
        std::cout << "  " << C_DIM << "부족분  " << C_RST << "  " << C_YLW << shortage << "ea" << C_RST << "\n";
        std::cout << "\n  " << C_YLW << "▲ 재고 부족" << C_RST
                  << "  부족분 " << C_BOLD << shortage << "ea" << C_RST
                  << " 승인 시 생산 등록"
                  << C_DIM << "  (실생산량 " << actualProd << "ea / " << totalTime << "min)" << C_RST << "\n";
        std::cout << "  " << C_BWHT << "[Y]" << C_RST << " 승인  "
                  << C_RED  << "[N]" << C_RST << " 주문 거절 " << C_CYN << "›" << C_RST << " ";
        std::cin >> yn; std::cin.ignore();
        if (yn != 'Y' && yn != 'y') {
            OrderService::reject(o);
            oc.updateOrder(o);
            or_.update(o);
            std::cout << "  " << C_RED << "✗ 거절 완료" << C_RST << "\n";
            return;
        }
    }

    bool producing = (origStock < o.quantity);
    int shortage = producing ? (o.quantity - origStock) : 0;
    OrderService::approve(o, s);
    if (producing) ps.enqueue(o, s, shortage);
    sc.updateStock(s.id, s.stock - sFound->stock);
    sr.update(s);
    oc.updateOrder(o);
    or_.update(o);

    const char* statusClr = (o.status == OrderStatus::CONFIRMED) ? C_GRN : C_CYN;
    const char* statusIcon = (o.status == OrderStatus::CONFIRMED) ? "✓" : "⚙";
    std::cout << "  " << statusClr << statusIcon << " 승인 완료" << C_RST << "  "
              << statusClr << C_BOLD << orderStatusToString(o.status) << C_RST << "\n";
}

static void menuProductionLine(OrderController& oc, SampleController& sc,
                               OrderRepository& or_, SampleRepository& sr,
                               ProductionService& ps) {
    // 입력 버퍼 비우기
    while (_kbhit()) _getch();

    if (!ps.hasNext()) {
        std::cout << "\n  " << C_DIM << "(생산 중인 항목 없음)" << C_RST << "\n";
        std::cout << "  " << C_DIM << "[아무 키] 뒤로" << C_RST;
        _getch();
        return;
    }

    while (true) {
        std::cout << "\033[2J\033[H" << std::flush;

        if (!ps.hasNext()) {
            std::cout << "\n  " << C_GRN << "✓ 모든 생산 완료" << C_RST << "\n\n";
            std::cout << "  " << C_DIM << "[아무 키] 뒤로" << C_RST;
            _getch();
            break;
        }

        auto item    = ps.peek();
        time_t now   = time(nullptr);
        double elap  = difftime(now, item.startTime);
        double total = item.totalTimeMin; // 시뮬레이션: 1min → 1sec
        double prog  = std::min(1.0, total > 0 ? elap / total : 1.0);
        int    pct   = static_cast<int>(prog * 100);
        int    rem   = static_cast<int>(std::max(0.0, total - elap));

        // ── 헤더
        std::cout << "\n" << C_BCYN << "▸ " << C_BWHT << "생산라인" << C_RST
                  << C_DIM << "  실시간 모니터링" << C_RST << "\n\n";

        // ── 현재 처리 중 박스
        std::cout << C_CYN << "⚙ " << C_BWHT << "현재 처리 중" << C_RST << "\n";
        std::cout << C_DIM; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << C_RST << "\n";
        std::cout << "  " << C_DIM << "주문번호  " << C_RST << C_CYN << C_BOLD << item.orderId  << C_RST << "\n";
        std::cout << "  " << C_DIM << "시료      " << C_RST << item.sampleName
                  << " " << C_DIM << "(" << item.sampleId << ")" << C_RST << "\n";
        std::cout << "  " << C_DIM << "주문량    " << C_RST << item.orderQuantity << "ea"
                  << C_DIM << "  실생산량 " << C_RST << C_GRN << C_BOLD << item.actualProduction << "ea" << C_RST << "\n\n";

        // ── 진행률 바 (30칸)
        int filled = static_cast<int>(prog * 30);
        std::string barStr;
        for (int i = 0; i < 30; i++) barStr += (i < filled ? "█" : "░");

        std::cout << "  " << C_DIM << "진행  " << C_RST
                  << (pct >= 100 ? C_GRN : C_CYN) << barStr << C_RST
                  << "  " << C_BWHT << pct << "%" << C_RST;
        if (pct < 100)
            std::cout << C_DIM << "  남은시간 " << rem << "초" << C_RST;
        else
            std::cout << C_GRN << "  완료 처리 중..." << C_RST;
        std::cout << "\n";
        std::cout << C_DIM; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << C_RST << "\n";

        // ── 대기 큐
        auto items = ps.getQueueItems();
        if ((int)items.size() > 1) {
            std::cout << "\n" << C_BWHT << "대기 중인 주문" << C_RST
                      << C_DIM << "  (FIFO)  " << (items.size() - 1) << "건" << C_RST << "\n";
            std::cout << C_DIM; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << C_RST << "\n";
            std::cout << C_DIM << std::left
                      << std::setw(5)  << "순서"
                      << std::setw(24) << "주문번호"
                      << std::setw(10) << "주문량"
                      << "실생산량" << C_RST << "\n";
            for (int i = 1; i < (int)items.size(); i++) {
                const auto& it = items[i];
                std::cout << std::left
                          << std::setw(5)  << i
                          << std::setw(24) << it.orderId
                          << std::setw(10) << (std::to_string(it.orderQuantity) + "ea")
                          << it.actualProduction << "ea\n";
            }
        }

        std::cout << "\n  " << C_DIM << "[0] 뒤로" << C_RST << "\n";
        std::cout << std::flush;

        // ── 자동 완료
        if (prog >= 1.0) {
            auto oFound = oc.findById(item.orderId);
            auto sFound = sc.findById(item.sampleId);
            if (oFound && sFound) {
                Order o = *oFound; Sample s = *sFound;
                auto done = ps.complete(o, s);
                for (auto& ord : oc.orders_)
                    if (ord.orderId == o.orderId) { ord = o; break; }
                sc.updateStock(s.id, done.actualProduction);
                or_.update(o); sr.update(s);
                std::cout << "\n  " << C_GRN << "✓ 생산 완료  "
                          << C_BOLD << item.orderId << C_RST << "  " << C_GRN << "→ CONFIRMED" << C_RST << "\n";
                std::cout << std::flush;
                Sleep(1500);
            }
            continue;
        }

        // ── 200ms 간격으로 키 체크, 총 1초 대기
        for (int i = 0; i < 5; i++) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == '0') return;
            }
            Sleep(200);
        }
    }
}

static void menuRelease(OrderController& oc, OrderRepository& or_) {
    auto confirmed = oc.getByStatus(OrderStatus::CONFIRMED);
    if (confirmed.empty()) {
        std::cout << "\n  " << C_DIM << "출고 대기 주문 없음" << C_RST << "\n";
        return;
    }

    std::cout << "\n" << C_BCYN << "▸ " << C_BWHT << "출고 대기 목록" << C_RST
              << C_DIM << "  CONFIRMED " << confirmed.size() << "건" << C_RST << "\n";
    std::cout << C_DIM
              << std::left << std::setw(6)  << "번호"
              << std::setw(22) << "주문번호"
              << std::setw(12) << "시료ID"
              << std::setw(18) << "고객명"
              << "수량" << C_RST << "\n";
    std::cout << C_DIM;
    for (int i = 0; i < 64; i++) std::cout << "─";
    std::cout << C_RST << "\n";
    for (int i = 0; i < (int)confirmed.size(); i++) {
        const auto& o = confirmed[i];
        std::cout << C_BWHT << std::left << std::setw(6) << ("[" + std::to_string(i+1) + "]") << C_RST
                  << std::setw(22) << o.orderId
                  << std::setw(12) << o.sampleId
                  << std::setw(18) << o.customerName
                  << o.quantity << "ea\n";
    }

    int idx = ConsoleView::promptInt("출고할 번호  (0=뒤로)");
    if (idx == 0) return;
    if (idx < 1 || idx > (int)confirmed.size()) {
        std::cout << "  " << C_RED << "✗ 잘못된 번호" << C_RST << "\n";
        return;
    }
    std::string oid = confirmed[idx - 1].orderId;

    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "  " << C_RED << "✗ 주문 없음" << C_RST << "\n"; return; }
    Order o = *oFound;
    if (!OrderService::release(o)) {
        std::cout << "  " << C_RED << "✗ 출고 불가 상태" << C_RST << "\n";
        return;
    }
    for (auto& ord : oc.orders_) {
        if (ord.orderId == oid) { ord = o; break; }
    }
    or_.update(o);
    std::cout << "  " << C_BLU << "↑ 출고 완료" << C_RST << "  "
              << C_BOLD << oid << C_RST << "  "
              << C_BLU << "→ RELEASE" << C_RST << "\n";
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    SetConsoleOutputCP(65001);

    // ANSI 이스케이프 코드 활성화 (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    CreateDirectoryA("data", nullptr);

    SampleRepository sr("data/samples.dat");
    OrderRepository  or_("data/orders.dat");

    SampleController sc;
    for (const auto& s : sr.findAll()) sc.addSample(s);

    OrderController oc;
    for (const auto& o : or_.findAll()) oc.orders_.push_back(o);

    ProductionService ps;
    for (const auto& o : oc.getByStatus(OrderStatus::PRODUCING)) {
        auto s = sc.findById(o.sampleId);
        if (s) ps.enqueue(o, *s, o.quantity); // 복원 시 재고=0, 부족분=전량
    }

    int choice = -1;
    while (choice != 0) {
        auto snap     = MonitorService::buildSnapshot(oc.getAll(), sc.getAll());
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
        case 0: std::cout << "\n  " << C_DIM << "종료합니다." << C_RST << "\n"; break;
        default: std::cout << "  " << C_RED << "✗ 잘못된 입력" << C_RST << "\n";
        }
    }
    return 0;
}
