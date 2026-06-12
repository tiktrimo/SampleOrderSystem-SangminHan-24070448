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
#include "src/view/Colors.h"

namespace {
    using namespace Clr;
}

// ── 콘솔 유틸 ────────────────────────────────────────────────────────────────
static void clsConsole() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi)) return;
    DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    COORD origin{0, 0};
    DWORD written;
    FillConsoleOutputCharacter(h, ' ', cells, origin, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cells, origin, &written);
    SetConsoleCursorPosition(h, origin);
}

static void setCursorVisible(bool visible) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci{1, visible ? TRUE : FALSE};
    SetConsoleCursorInfo(h, &cci);
}

// ── 생산 자동 완료 ────────────────────────────────────────────────────────────
// 메인 루프 상단에서 호출 — 시간이 지난 생산 항목을 즉시 CONFIRMED 처리
static void autoCompleteProduction(OrderController& oc, SampleController& sc,
                                   OrderRepository& or_, SampleRepository& sr,
                                   ProductionService& ps) {
    while (ps.hasNext()) {
        auto item  = ps.peek();
        double elap = difftime(time(nullptr), item.startTime);
        if (item.totalTimeMin > 0 && elap < item.totalTimeMin) break;

        auto oFound = oc.findById(item.orderId);
        auto sFound = sc.findById(item.sampleId);
        if (!oFound || !sFound) break;

        Order o = *oFound; Sample s = *sFound;
        auto done = ps.complete(o, s);
        for (auto& ord : oc.orders_)
            if (ord.orderId == o.orderId) { ord = o; break; }
        sc.updateStock(s.id, done.actualProduction);
        or_.update(o); sr.update(s);
    }
}

// ── 메뉴 함수 ─────────────────────────────────────────────────────────────────

static void menuSampleManage(SampleController& sc, SampleRepository& sr) {
    std::cout << "\n" << BCYN << "▸ " << BWHT << "시료 관리"  << RST
              << DWHT  << "  (1)등록  (2)조회  (3)검색  (0)뒤로" << RST << " " << CYN << "›" << RST << " ";
    int sub = 0; std::cin >> sub; std::cin.ignore();

    if (sub == 0) return;
    if (sub == 1) {
        std::cout << "\n" << BWHT << "  [ 시료 등록 ]" << RST << "\n";
        Sample s;
        s.id                  = ConsoleView::promptString("시료 ID");
        s.name                = ConsoleView::promptString("이름");
        s.avgProductionTimeMin = ConsoleView::promptDouble("평균 생산시간(min)");
        s.yield               = ConsoleView::promptDouble("수율(0~1)");
        s.stock               = ConsoleView::promptInt("초기 재고");
        sc.addSample(s);
        sr.save(s);
        std::cout << "  " << GRN << "✓ 등록 완료" << RST << "  " << BOLD << s.id << RST << "\n";
    } else if (sub == 2) {
        ConsoleView::showSamples(sc.getAll());
    } else if (sub == 3) {
        auto kw = ConsoleView::promptString("검색 키워드");
        ConsoleView::showSamples(sc.searchByName(kw));
    }
}

static void menuPlaceOrder(OrderController& oc, SampleController& sc,
                           OrderRepository& or_) {
    std::cout << "\n" << BCYN << "▸ " << BWHT << "주문 접수" << RST
              << DWHT << "  (0 입력 시 뒤로)" << RST << "\n";
    ConsoleView::showSamples(sc.getAll());

    auto sid  = ConsoleView::promptString("시료 ID  (0=뒤로)");
    if (sid == "0") return;
    auto cust = ConsoleView::promptString("고객명");
    int  qty  = ConsoleView::promptInt("수량");

    auto sFound = sc.findById(sid);
    if (!sFound) {
        std::cout << "  " << RED << "✗ 시료 없음: " << BOLD << sid << RST << "\n";
        return;
    }
    std::cout << "\n" << BWHT << "  [ 입력 확인 ]" << RST << "\n";
    std::cout << "  " << DWHT << "시료" << RST << "  " << sFound->name
              << " " << DWHT << "(" << sid << ")" << RST << "\n";
    std::cout << "  " << DWHT << "고객" << RST << "  " << cust << "\n";
    std::cout << "  " << DWHT << "수량" << RST << "  " << BOLD << qty << "ea" << RST << "\n";
    std::cout << "\n  " << BWHT << "[Y]" << RST << " 예약 접수  "
              << DWHT  << "[N]" << RST << " 취소 " << CYN << "›" << RST << " ";
    char yn = 0; std::cin >> yn; std::cin.ignore();
    if (yn != 'Y' && yn != 'y') {
        std::cout << "  " << DWHT << "취소됨" << RST << "\n";
        return;
    }

    auto id = oc.placeOrder(sid, cust, qty);
    auto found = oc.findById(id);
    if (found) or_.save(*found);
    std::cout << "  " << GRN << "✓ 접수 완료" << RST << "  "
              << BOLD << id << RST << "  "
              << YLW << "● RESERVED" << RST << "\n";
}

static void menuApproveReject(OrderController& oc, SampleController& sc,
                              OrderRepository& or_, SampleRepository& sr,
                              ProductionService& ps) {
    auto reserved = oc.getByStatus(OrderStatus::RESERVED);
    if (reserved.empty()) {
        std::cout << "\n  " << DWHT << "대기 주문 없음" << RST << "\n";
        return;
    }

    std::cout << "\n" << BCYN << "▸ " << BWHT << "승인 대기 목록" << RST
              << DWHT << "  RESERVED " << reserved.size() << "건" << RST << "\n";
    // 헤더
    std::cout << DWHT
              << std::left << std::setw(6)  << "번호"
              << std::setw(22) << "주문번호"
              << std::setw(12) << "시료ID"
              << std::setw(18) << "고객명"
              << "수량" << RST << "\n";
    std::cout << DWHT;
    for (int i = 0; i < 64; i++) std::cout << "─";
    std::cout << RST << "\n";
    for (int i = 0; i < (int)reserved.size(); i++) {
        const auto& o = reserved[i];
        std::cout << BWHT << std::left << std::setw(6) << ("[" + std::to_string(i+1) + "]") << RST
                  << std::setw(22) << o.orderId
                  << std::setw(12) << o.sampleId
                  << std::setw(18) << o.customerName
                  << o.quantity << "ea\n";
    }

    int idx = ConsoleView::promptInt("승인/거절할 번호  (0=뒤로)");
    if (idx == 0) return;
    if (idx < 1 || idx > (int)reserved.size()) {
        std::cout << "  " << RED << "✗ 잘못된 번호" << RST << "\n";
        return;
    }
    std::string oid = reserved[idx - 1].orderId;

    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "  " << RED << "✗ 주문 없음" << RST << "\n"; return; }
    Order o = *oFound;

    auto sFound = sc.findById(o.sampleId);
    if (!sFound) { std::cout << "  " << RED << "✗ 시료 없음" << RST << "\n"; return; }
    Sample s = *sFound;
    const int origStock = s.stock; // approve 전 재고 저장 (부족분 계산용)

    std::cout << "\n  " << DWHT << "재고 확인 중..." << RST << "\n";
    std::cout << "  " << DWHT << "시료    " << RST << "  " << s.name
              << " " << DWHT << "(" << s.id << ")" << RST << "\n";
    std::cout << "  " << DWHT << "현재 재고" << RST << "  " << BOLD << s.stock << "ea" << RST << "\n";
    std::cout << "  " << DWHT << "주문 수량" << RST << "  " << BOLD << o.quantity << "ea" << RST << "\n";

    char yn = 0;
    if (s.stock >= o.quantity) {
        std::cout << "\n  " << GRN << "재고 충분" << RST
                  << "  " << BWHT << "[Y]" << RST << " 예약 처리  "
                  << DWHT  << "[N]" << RST << " 취소 " << CYN << "›" << RST << " ";
        std::cin >> yn; std::cin.ignore();
        if (yn != 'Y' && yn != 'y') {
            std::cout << "  " << DWHT << "취소됨" << RST << "\n";
            return;
        }
    } else {
        int shortage = o.quantity - origStock;
        int actualProd = ProductionService::calcActualProduction(shortage, s.yield);
        double totalTime = ProductionService::calcTotalTime(s.avgProductionTimeMin, actualProd);
        std::cout << "  " << DWHT << "부족분  " << RST << "  " << YLW << shortage << "ea" << RST << "\n";
        std::cout << "\n  " << YLW << "▲ 재고 부족" << RST
                  << "  부족분 " << BOLD << shortage << "ea" << RST
                  << " 승인 시 생산 등록"
                  << DWHT << "  (실생산량 " << actualProd << "ea / " << totalTime << "sec)" << RST << "\n";
        std::cout << "  " << BWHT << "[Y]" << RST << " 승인  "
                  << RED  << "[N]" << RST << " 주문 거절 " << CYN << "›" << RST << " ";
        std::cin >> yn; std::cin.ignore();
        if (yn != 'Y' && yn != 'y') {
            OrderService::reject(o);
            oc.updateOrder(o);
            or_.update(o);
            std::cout << "  " << RED << "✗ 거절 완료" << RST << "\n";
            return;
        }
    }

    bool producing = (origStock < o.quantity);
    int shortage = producing ? (o.quantity - origStock) : 0;
    o.shortage = shortage;
    OrderService::approve(o, s);
    if (producing) ps.enqueue(o, s, shortage);
    oc.updateOrder(o);
    or_.update(o);

    const char* statusClr = (o.status == OrderStatus::CONFIRMED) ? GRN : CYN;
    const char* statusIcon = (o.status == OrderStatus::CONFIRMED) ? "✓" : "⚙";
    std::cout << "  " << statusClr << statusIcon << " 승인 완료" << RST << "  "
              << statusClr << BOLD << orderStatusToString(o.status) << RST << "\n";
}

static void menuProductionLine(OrderController& oc, SampleController& sc,
                               OrderRepository& or_, SampleRepository& sr,
                               ProductionService& ps) {
    // 입력 버퍼 비우기
    while (_kbhit()) _getch();

    if (!ps.hasNext()) {
        std::cout << "\n  " << DWHT << "(생산 중인 항목 없음)" << RST << "\n";
        std::cout << "  " << DWHT << "[아무 키] 뒤로" << RST;
        _getch();
        return;
    }

    setCursorVisible(false);
    while (true) {
        clsConsole();

        if (!ps.hasNext()) {
            std::cout << "\n  " << GRN << "✓ 모든 생산 완료" << RST << "\n\n";
            std::cout << "  " << DWHT << "[아무 키] 뒤로" << RST;
            _getch();
            setCursorVisible(true);
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
        std::cout << "\n" << BCYN << "▸ " << BWHT << "생산라인" << RST
                  << DWHT << "  실시간 모니터링" << RST << "\n\n";

        // ── 현재 처리 중 박스
        std::cout << CYN << "⚙ " << BWHT << "현재 처리 중" << RST << "\n";
        std::cout << DWHT; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << RST << "\n";
        std::cout << "  " << DWHT << "주문번호  " << RST << CYN << BOLD << item.orderId  << RST << "\n";
        std::cout << "  " << DWHT << "시료      " << RST << item.sampleName
                  << " " << DWHT << "(" << item.sampleId << ")" << RST << "\n";
        int curProd = static_cast<int>(prog * item.actualProduction);
        std::cout << "  " << DWHT << "주문량    " << RST << item.orderQuantity << "ea"
                  << DWHT << "  목표생산량 " << RST << GRN << BOLD << item.actualProduction << "ea" << RST << "\n";
        std::cout << "  " << DWHT << "현재생산량" << RST
                  << "  " << CYN << BOLD << curProd << "ea" << RST
                  << DWHT << " / " << item.actualProduction << "ea" << RST << "\n\n";

        // ── 진행률 바 (30칸)
        int filled = static_cast<int>(prog * 30);
        std::string barStr;
        for (int i = 0; i < 30; i++) barStr += (i < filled ? "█" : "░");

        std::cout << "  " << DWHT << "진행  " << RST
                  << (pct >= 100 ? GRN : CYN) << barStr << RST
                  << "  " << BWHT << pct << "%" << RST;
        if (pct < 100)
            std::cout << DWHT << "  남은시간 " << rem << "초" << RST;
        else
            std::cout << GRN << "  완료 처리 중..." << RST;
        std::cout << "\n";
        std::cout << DWHT; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << RST << "\n";

        // ── 대기 큐
        auto items = ps.getQueueItems();
        if ((int)items.size() > 1) {
            std::cout << "\n" << BWHT << "대기 중인 주문" << RST
                      << DWHT << "  (FIFO)  " << (items.size() - 1) << "건" << RST << "\n";
            std::cout << DWHT; for (int i = 0; i < 54; i++) std::cout << "─"; std::cout << RST << "\n";
            std::cout << DWHT << std::left
                      << std::setw(5)  << "순서"
                      << std::setw(24) << "주문번호"
                      << std::setw(10) << "주문량"
                      << "실생산량" << RST << "\n";
            for (int i = 1; i < (int)items.size(); i++) {
                const auto& it = items[i];
                std::cout << std::left
                          << std::setw(5)  << i
                          << std::setw(24) << it.orderId
                          << std::setw(10) << (std::to_string(it.orderQuantity) + "ea")
                          << it.actualProduction << "ea\n";
            }
        }

        std::cout << "\n  " << DWHT << "[0] 뒤로" << RST << "\n";
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
                std::cout << "\n  " << GRN << "✓ 생산 완료  "
                          << BOLD << item.orderId << RST << "  " << GRN << "→ CONFIRMED" << RST << "\n";
                std::cout << std::flush;
                Sleep(1500);
            }
            continue;
        }

        // ── 200ms 간격으로 키 체크, 총 1초 대기
        for (int i = 0; i < 5; i++) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == '0') { setCursorVisible(true); return; }
            }
            Sleep(200);
        }
    }
}

static void menuRelease(OrderController& oc, SampleController& sc,
                        OrderRepository& or_, SampleRepository& sr) {
    auto confirmed = oc.getByStatus(OrderStatus::CONFIRMED);
    if (confirmed.empty()) {
        std::cout << "\n  " << DWHT << "출고 대기 주문 없음" << RST << "\n";
        return;
    }

    std::cout << "\n" << BCYN << "▸ " << BWHT << "출고 대기 목록" << RST
              << DWHT << "  CONFIRMED " << confirmed.size() << "건" << RST << "\n";
    std::cout << DWHT
              << std::left << std::setw(6)  << "번호"
              << std::setw(22) << "주문번호"
              << std::setw(12) << "시료ID"
              << std::setw(18) << "고객명"
              << "수량" << RST << "\n";
    std::cout << DWHT;
    for (int i = 0; i < 64; i++) std::cout << "─";
    std::cout << RST << "\n";
    for (int i = 0; i < (int)confirmed.size(); i++) {
        const auto& o = confirmed[i];
        std::cout << BWHT << std::left << std::setw(6) << ("[" + std::to_string(i+1) + "]") << RST
                  << std::setw(22) << o.orderId
                  << std::setw(12) << o.sampleId
                  << std::setw(18) << o.customerName
                  << o.quantity << "ea\n";
    }

    int idx = ConsoleView::promptInt("출고할 번호  (0=뒤로)");
    if (idx == 0) return;
    if (idx < 1 || idx > (int)confirmed.size()) {
        std::cout << "  " << RED << "✗ 잘못된 번호" << RST << "\n";
        return;
    }
    std::string oid = confirmed[idx - 1].orderId;

    auto oFound = oc.findById(oid);
    if (!oFound) { std::cout << "  " << RED << "✗ 주문 없음" << RST << "\n"; return; }
    Order o = *oFound;
    if (!OrderService::release(o)) {
        std::cout << "  " << RED << "✗ 출고 불가 상태" << RST << "\n";
        return;
    }
    // 출고 시 재고 차감 (order.quantity 만큼)
    auto sFound = sc.findById(o.sampleId);
    if (sFound) {
        sc.updateStock(o.sampleId, -o.quantity);
        auto sUpdated = sc.findById(o.sampleId);
        if (sUpdated) sr.update(*sUpdated);
    }
    for (auto& ord : oc.orders_) {
        if (ord.orderId == oid) { ord = o; break; }
    }
    or_.update(o);
    std::cout << "  " << BLU << "↑ 출고 완료" << RST << "  "
              << BOLD << oid << RST << "  "
              << BLU << "→ RELEASE" << RST << "\n";
    if (sFound) {
        auto sNow = sc.findById(o.sampleId);
        int stock = sNow ? sNow->stock : 0;
        std::cout << "  " << DWHT << "재고" << RST << "  "
                  << BWHT << sFound->stock << RST
                  << DWHT << " → " << RST
                  << (stock >= 0 ? GRN : RED) << BOLD << stock << "ea" << RST << "\n";
    }
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
    oc.syncSequence(); // 재시작 후 중복 주문번호 방지

    ProductionService ps;
    for (const auto& o : oc.getByStatus(OrderStatus::PRODUCING)) {
        auto s = sc.findById(o.sampleId);
        int restoreShortage = o.shortage > 0 ? o.shortage : o.quantity;
        if (s) ps.enqueue(o, *s, restoreShortage);
    }

    int choice = -1;
    while (choice != 0) {
        autoCompleteProduction(oc, sc, or_, sr, ps);
        int producing = ps.queueSize();
        int displayStock = sc.getTotalStock();
        if (ps.hasNext()) {
            auto item = ps.peek();
            double elap = difftime(time(nullptr), item.startTime);
            double prog = std::min(1.0, item.totalTimeMin > 0 ? elap / item.totalTimeMin : 1.0);
            displayStock += static_cast<int>(prog * item.actualProduction);
        }
        ConsoleView::showMainMenu(sc.getSampleCount(), displayStock,
                                  oc.getOrderCount(), producing);
        std::cin >> choice; std::cin.ignore();
        switch (choice) {
        case 1: menuSampleManage(sc, sr); break;
        case 2: menuPlaceOrder(oc, sc, or_); break;
        case 3: menuApproveReject(oc, sc, or_, sr, ps); break;
        case 4: {
            autoCompleteProduction(oc, sc, or_, sr, ps);
            auto snap = MonitorService::buildSnapshot(oc.getAll(), sc.getAll());
            // 생산 진행 중인 항목의 현재 생산량을 재고에 반영 (표시 전용)
            if (ps.hasNext()) {
                auto item = ps.peek();
                double elap = difftime(time(nullptr), item.startTime);
                double prog = std::min(1.0, item.totalTimeMin > 0 ? elap / item.totalTimeMin : 1.0);
                int curProd = static_cast<int>(prog * item.actualProduction);
                for (auto& info : snap.stockInfos)
                    if (info.sample.id == item.sampleId) { info.sample.stock += curProd; break; }
            }
            ConsoleView::showMonitor(snap);
            break;
        }
        case 5: menuProductionLine(oc, sc, or_, sr, ps); break;
        case 6: menuRelease(oc, sc, or_, sr); break;
        case 0: std::cout << "\n  " << DWHT << "종료합니다." << RST << "\n"; break;
        default: std::cout << "  " << RED << "✗ 잘못된 입력" << RST << "\n";
        }
    }
    return 0;
}
