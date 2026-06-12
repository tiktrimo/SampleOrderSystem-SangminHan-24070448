#include "ConsoleView.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <string>

// ── ANSI escape codes ─────────────────────────────────────────────────────────
namespace {
    constexpr const char* RST  = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM  = "\033[2m";
    constexpr const char* RED  = "\033[31m";
    constexpr const char* GRN  = "\033[32m";
    constexpr const char* YLW  = "\033[33m";
    constexpr const char* BLU  = "\033[34m";
    constexpr const char* CYN  = "\033[36m";
    constexpr const char* BCYN = "\033[1;36m";
    constexpr const char* BWHT = "\033[1;37m";
    constexpr const char* DWHT = "\033[2;37m";

    // UTF-8 문자 화면 너비 (한글=2, ASCII=1)
    int dw(const std::string& s) {
        int w = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if      (c < 0x80) { w += 1; i += 1; }
            else if (c < 0xE0) { w += 1; i += 2; }
            else if (c < 0xF0) { w += 2; i += 3; }
            else               { w += 2; i += 4; }
        }
        return w;
    }

    // ANSI 이스케이프 시퀀스를 제외한 실제 표시 너비
    int ansiDw(const std::string& s) {
        int w = 0;
        for (size_t i = 0; i < s.size(); ) {
            if (s[i] == '\033' && i + 1 < s.size() && s[i+1] == '[') {
                i += 2;
                while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E)) i++;
                if (i < s.size()) i++;
            } else {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if      (c < 0x80) { w += 1; i += 1; }
                else if (c < 0xE0) { w += 1; i += 2; }
                else if (c < 0xF0) { w += 2; i += 3; }
                else               { w += 2; i += 4; }
            }
        }
        return w;
    }

    // 화면 너비 기준 우측 패딩
    std::string rpad(const std::string& s, int width) {
        int pad = std::max(0, width - dw(s));
        return s + std::string(pad, ' ');
    }

    // ── 박스 드로잉 ──────────────────────────────────────────────────────────
    // BOX_W: │와 │ 사이의 표시 너비 (내부 콘텐츠 영역 포함)
    constexpr int BOX_W = 56;

    void hline(int n = 50) {
        std::cout << DWHT;
        for (int i = 0; i < n; i++) std::cout << "─";
        std::cout << RST << "\n";
    }

    void borderTop() {
        std::cout << DWHT << "╭";
        for (int i = 0; i < BOX_W; i++) std::cout << "─";
        std::cout << "╮" << RST << "\n";
    }

    void borderMid() {
        std::cout << DWHT << "├";
        for (int i = 0; i < BOX_W; i++) std::cout << "─";
        std::cout << "┤" << RST << "\n";
    }

    void borderBot() {
        std::cout << DWHT << "╰";
        for (int i = 0; i < BOX_W; i++) std::cout << "─";
        std::cout << "╯" << RST << "\n";
    }

    // 콘텐츠를 BOX_W 너비에 맞춰 패딩하고 우측 │ 추가
    void bar(const std::string& content = "") {
        int cw  = ansiDw(content);
        int pad = std::max(0, BOX_W - 2 - cw); // 2 = 앞 공백
        std::cout << DWHT << "│" << RST
                  << "  " << content << std::string(pad, ' ')
                  << DWHT << "│" << RST << "\n";
    }

    void sectionHeader(const std::string& title) {
        std::cout << "\n" << BCYN << "▸ " << BWHT << title << RST << "\n";
        hline(46);
    }

    // ── 상태 뱃지 ────────────────────────────────────────────────────────────

    const char* statusClr(OrderStatus s) {
        switch (s) {
        case OrderStatus::RESERVED:  return YLW;
        case OrderStatus::CONFIRMED: return GRN;
        case OrderStatus::PRODUCING: return CYN;
        case OrderStatus::REJECTED:  return RED;
        case OrderStatus::RELEASE:   return BLU;
        default: return RST;
        }
    }

    std::string statusBadge(OrderStatus s) {
        const char* icon;
        switch (s) {
        case OrderStatus::RESERVED:  icon = "● "; break;
        case OrderStatus::CONFIRMED: icon = "✓ "; break;
        case OrderStatus::PRODUCING: icon = "⚙ "; break;
        case OrderStatus::REJECTED:  icon = "✗ "; break;
        case OrderStatus::RELEASE:   icon = "↑ "; break;
        default: icon = "  "; break;
        }
        return std::string(statusClr(s)) + BOLD + icon + orderStatusToString(s) + RST;
    }

    std::string stockBadge(StockStatus s) {
        switch (s) {
        case StockStatus::DEPLETED: return std::string(RED) + BOLD + "● 고갈" + RST;
        case StockStatus::LOW:      return std::string(YLW) + BOLD + "▲ 부족" + RST;
        default:                    return std::string(GRN) + BOLD + "✓ 여유" + RST;
        }
    }
}

// ── showMainMenu ──────────────────────────────────────────────────────────────
void ConsoleView::showMainMenu(int sampleCount, int totalStock,
                               int orderCount, int producingCount) {
    time_t now = time(nullptr);
    struct tm t{};
    localtime_s(&t, &now);
    char dtbuf[32];
    strftime(dtbuf, sizeof(dtbuf), "%Y-%m-%d %H:%M:%S", &t);

    std::cout << "\n";
    borderTop();
    bar(std::string(BCYN) + "S-Semi" + RST + "  반도체 시료 생산주문관리 시스템");
    bar(std::string(DWHT) + dtbuf + RST);
    borderMid();

    // 현황 한 줄 — bar()로 전달해 우측 │ 정렬
    {
        std::string s;
        s += std::string(BWHT) + "시료 "   + RST + std::to_string(sampleCount) + "종";
        s += std::string(DWHT) + "  ·  "   + RST;
        s += std::string(BWHT) + "총재고 " + RST + std::to_string(totalStock)  + "ea";
        s += std::string(DWHT) + "  ·  "   + RST;
        s += std::string(BWHT) + "주문 "   + RST + std::to_string(orderCount)  + "건";
        s += std::string(DWHT) + "  ·  "   + RST;
        s += (producingCount > 0 ? std::string(CYN) : std::string(DWHT));
        s += "생산중 " + std::to_string(producingCount) + "건" + RST;
        bar(s);
    }

    borderMid();

    auto menuItem = [](const char* n, const char* label) {
        std::string content = std::string(BWHT) + "[" + n + "]" + RST + "  " + label;
        bar(content);
    };
    menuItem("1", "시료 관리");
    menuItem("2", "주문 접수");
    menuItem("3", "주문 승인/거절");
    menuItem("4", "모니터링");
    menuItem("5", "생산라인");
    menuItem("6", "출고 처리");
    bar();
    menuItem("0", "종료");
    borderBot();
    std::cout << BWHT << "선택 " << RST << CYN << "›" << RST << " ";
}

// ── showSamples ───────────────────────────────────────────────────────────────
void ConsoleView::showSamples(const std::vector<Sample>& samples) {
    sectionHeader("시료 목록");
    std::cout << DWHT
              << rpad("ID", 10)
              << rpad("시료명", 22)
              << rpad("생산시간", 12)
              << rpad("수율", 8)
              << "재고" << RST << "\n";
    hline(58);
    if (samples.empty()) {
        std::cout << DWHT << "  (등록된 시료 없음)" << RST << "\n";
        return;
    }
    for (const auto& s : samples) {
        const char* stk = s.stock == 0 ? RED : (s.stock < 20 ? YLW : GRN);
        std::cout << rpad(s.id,   10)
                  << rpad(s.name, 22)
                  << rpad(std::to_string(s.avgProductionTimeMin) + "min", 12)
                  << rpad(std::to_string(s.yield), 8)
                  << stk << BOLD << s.stock << "ea" << RST << "\n";
    }
}

// ── showOrders ────────────────────────────────────────────────────────────────
void ConsoleView::showOrders(const std::vector<Order>& orders) {
    sectionHeader("주문 목록");
    std::cout << DWHT
              << rpad("번호", 6)
              << rpad("주문번호", 22)
              << rpad("시료ID", 10)
              << rpad("고객명", 18)
              << rpad("수량", 8)
              << "상태" << RST << "\n";
    hline(76);
    if (orders.empty()) {
        std::cout << DWHT << "  (주문 없음)" << RST << "\n";
        return;
    }
    for (int i = 0; i < (int)orders.size(); i++) {
        const auto& o = orders[i];
        std::cout << rpad("[" + std::to_string(i + 1) + "]", 6)
                  << rpad(o.orderId,      22)
                  << rpad(o.sampleId,     10)
                  << rpad(o.customerName, 18)
                  << rpad(std::to_string(o.quantity) + "ea", 8)
                  << statusBadge(o.status) << "\n";
    }
}

// ── showMonitor ───────────────────────────────────────────────────────────────
void ConsoleView::showMonitor(const MonitorSnapshot& snap) {
    sectionHeader("주문 현황");
    if (snap.orderCounts.empty()) {
        std::cout << DWHT << "  (집계된 주문 없음)" << RST << "\n";
    } else {
        for (const auto& [status, count] : snap.orderCounts) {
            std::cout << "  " << statusBadge(status)
                      << DWHT << "  " << RST << count << "건\n";
        }
    }

    sectionHeader("재고 현황");
    std::cout << DWHT
              << rpad("ID", 10)
              << rpad("시료명", 22)
              << rpad("재고", 8)
              << rpad("대기", 8)
              << "상태" << RST << "\n";
    hline(58);
    if (snap.stockInfos.empty()) {
        std::cout << DWHT << "  (등록된 시료 없음)" << RST << "\n";
        return;
    }
    for (const auto& info : snap.stockInfos) {
        std::cout << rpad(info.sample.id,   10)
                  << rpad(info.sample.name, 22)
                  << rpad(std::to_string(info.sample.stock)      + "ea", 8)
                  << rpad(std::to_string(info.pendingQuantity)   + "ea", 8)
                  << stockBadge(info.stockStatus) << "\n";
    }
}

// ── showProductionQueue ───────────────────────────────────────────────────────
void ConsoleView::showProductionQueue(ProductionService& ps) {
    sectionHeader("생산 큐 현황");
    std::cout << DWHT << "  대기 " << RST
              << (ps.queueSize() > 0 ? std::string(CYN) + BOLD : std::string(DWHT))
              << ps.queueSize() << "건" << RST << "\n";

    if (!ps.hasNext()) {
        std::cout << "\n" << DWHT << "  (생산 중인 항목 없음)" << RST << "\n";
        return;
    }

    auto items = ps.getQueueItems();
    const auto& cur = items[0];

    std::cout << "\n  " << BOLD << BWHT << "[ 현재 처리 중 ]" << RST << "\n";
    hline(40);
    std::cout << "  " << rpad(std::string(DWHT) + "주문번호" + RST, 8 + 9)
              << CYN << BOLD << cur.orderId << RST << "\n";
    std::cout << "  " << rpad(std::string(DWHT) + "시료명  " + RST, 8 + 9)
              << cur.sampleName << " " << DWHT << "(" << cur.sampleId << ")" << RST << "\n";
    std::cout << "  " << rpad(std::string(DWHT) + "주문량  " + RST, 8 + 9)
              << cur.orderQuantity << "ea\n";
    std::cout << "  " << rpad(std::string(DWHT) + "실생산량" + RST, 8 + 9)
              << GRN << BOLD << cur.actualProduction << "ea" << RST << "\n";
    std::cout << "  " << rpad(std::string(DWHT) + "생산시간" + RST, 8 + 9)
              << cur.totalTimeMin << "min\n";

    if (items.size() > 1) {
        std::cout << "\n  " << BOLD << BWHT << "[ 대기 중인 주문 — FIFO 순 ]" << RST << "\n";
        hline(72);
        std::cout << DWHT
                  << rpad("순서", 6)
                  << rpad("주문번호", 22)
                  << rpad("시료", 16)
                  << rpad("주문량", 10)
                  << rpad("실생산량", 10)
                  << "예상시간" << RST << "\n";
        hline(72);
        for (int i = 1; i < (int)items.size(); i++) {
            const auto& it = items[i];
            std::cout << rpad(std::to_string(i + 1), 6)
                      << rpad(it.orderId,   22)
                      << rpad(it.sampleName, 16)
                      << rpad(std::to_string(it.orderQuantity)    + "ea", 10)
                      << rpad(std::to_string(it.actualProduction) + "ea", 10)
                      << it.totalTimeMin << "min\n";
        }
    }
}

// ── prompts ───────────────────────────────────────────────────────────────────
std::string ConsoleView::promptString(const std::string& label) {
    std::cout << "  " << DWHT << label << RST << " " << CYN << "›" << RST << " ";
    std::string val;
    std::getline(std::cin >> std::ws, val);
    return val;
}

int ConsoleView::promptInt(const std::string& label) {
    std::cout << "  " << DWHT << label << RST << " " << CYN << "›" << RST << " ";
    int val = 0;
    std::cin >> val;
    std::cin.ignore();
    return val;
}

double ConsoleView::promptDouble(const std::string& label) {
    std::cout << "  " << DWHT << label << RST << " " << CYN << "›" << RST << " ";
    double val = 0.0;
    std::cin >> val;
    std::cin.ignore();
    return val;
}
