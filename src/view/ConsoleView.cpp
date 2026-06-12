#include "ConsoleView.h"
#include <iostream>
#include <iomanip>
#include <ctime>

static const char* stockStatusLabel(StockStatus s) {
    if (s == StockStatus::DEPLETED) return "[고갈]";
    if (s == StockStatus::LOW)      return "[부족]";
    return "[여유]";
}

void ConsoleView::showMainMenu(int sampleCount, int totalStock,
                               int orderCount, int producingCount) {
    time_t now = time(nullptr);
    struct tm t {};
    localtime_s(&t, &now);
    char dtbuf[32];
    strftime(dtbuf, sizeof(dtbuf), "%Y-%m-%d %H:%M:%S", &t);

    std::cout << "\n========================================\n";
    std::cout << "   S-Semi 반도체 시료 생산주문관리 시스템\n";
    std::cout << "   " << dtbuf << "\n";
    std::cout << "========================================\n";
    std::cout << "  시료: " << sampleCount << "종  |  총재고: " << totalStock
              << "ea  |  주문: " << orderCount << "건  |  생산중: " << producingCount << "건\n";
    std::cout << "----------------------------------------\n";
    std::cout << "  [1] 시료 관리\n";
    std::cout << "  [2] 주문 접수\n";
    std::cout << "  [3] 주문 승인/거절\n";
    std::cout << "  [4] 모니터링\n";
    std::cout << "  [5] 생산라인\n";
    std::cout << "  [6] 출고 처리\n";
    std::cout << "  [0] 종료\n";
    std::cout << "========================================\n";
    std::cout << "선택: ";
}

void ConsoleView::showSamples(const std::vector<Sample>& samples) {
    std::cout << "\n[ 시료 목록 ]\n";
    std::cout << std::left << std::setw(8) << "ID"
              << std::setw(20) << "이름"
              << std::setw(10) << "생산시간"
              << std::setw(8) << "수율"
              << "재고\n";
    std::cout << std::string(60, '-') << "\n";
    for (const auto& s : samples) {
        std::cout << std::left << std::setw(8) << s.id
                  << std::setw(20) << s.name
                  << std::setw(10) << s.avgProductionTimeMin
                  << std::setw(8) << s.yield
                  << s.stock << "ea\n";
    }
}

void ConsoleView::showOrders(const std::vector<Order>& orders) {
    std::cout << "\n[ 주문 목록 ]\n";
    std::cout << std::left << std::setw(20) << "주문번호"
              << std::setw(10) << "시료ID"
              << std::setw(20) << "고객명"
              << std::setw(8) << "수량"
              << "상태\n";
    std::cout << std::string(70, '-') << "\n";
    for (const auto& o : orders) {
        std::cout << std::left << std::setw(20) << o.orderId
                  << std::setw(10) << o.sampleId
                  << std::setw(20) << o.customerName
                  << std::setw(8) << o.quantity
                  << orderStatusToString(o.status) << "\n";
    }
}

void ConsoleView::showMonitor(const MonitorSnapshot& snap) {
    std::cout << "\n[ 주문 현황 ]\n";
    for (const auto& [status, count] : snap.orderCounts) {
        std::cout << "  " << std::setw(12) << orderStatusToString(status)
                  << ": " << count << "건\n";
    }
    std::cout << "\n[ 재고 현황 ]\n";
    std::cout << std::left << std::setw(10) << "ID"
              << std::setw(20) << "이름"
              << std::setw(8) << "재고"
              << std::setw(10) << "대기"
              << "상태\n";
    std::cout << std::string(60, '-') << "\n";
    for (const auto& info : snap.stockInfos) {
        std::cout << std::left << std::setw(10) << info.sample.id
                  << std::setw(20) << info.sample.name
                  << std::setw(8) << info.sample.stock
                  << std::setw(10) << info.pendingQuantity
                  << stockStatusLabel(info.stockStatus) << "\n";
    }
}

void ConsoleView::showProductionQueue(ProductionService& ps) {
    std::cout << "\n[ 생산 큐 현황 ] 대기: " << ps.queueSize() << "건\n";
    if (!ps.hasNext()) {
        std::cout << "  (생산 중인 항목 없음)\n";
        return;
    }
    auto items = ps.getQueueItems();

    std::cout << "\n[ 현재 처리 중 ]\n";
    const auto& cur = items[0];
    std::cout << "  주문번호: " << cur.orderId << "\n";
    std::cout << "  시료명  : " << cur.sampleName << " (" << cur.sampleId << ")\n";
    std::cout << "  주문량  : " << cur.orderQuantity << "ea\n";
    std::cout << "  실생산량: " << cur.actualProduction << "ea\n";
    std::cout << "  생산시간: " << cur.totalTimeMin << "min\n";

    if (items.size() > 1) {
        std::cout << "\n[ 대기 중인 주문 (FIFO 순) ]\n";
        std::cout << std::left
                  << std::setw(6)  << "순서"
                  << std::setw(22) << "주문번호"
                  << std::setw(15) << "시료"
                  << std::setw(10) << "주문량"
                  << std::setw(10) << "실생산량"
                  << "예상시간\n";
        std::cout << std::string(72, '-') << "\n";
        for (int i = 1; i < (int)items.size(); i++) {
            const auto& it = items[i];
            std::cout << std::left
                      << std::setw(6)  << (i + 1)
                      << std::setw(22) << it.orderId
                      << std::setw(15) << it.sampleName
                      << std::setw(10) << (std::to_string(it.orderQuantity) + "ea")
                      << std::setw(10) << (std::to_string(it.actualProduction) + "ea")
                      << it.totalTimeMin << "min\n";
        }
    }
}

std::string ConsoleView::promptString(const std::string& label) {
    std::cout << label << ": ";
    std::string val;
    std::getline(std::cin >> std::ws, val);
    return val;
}

int ConsoleView::promptInt(const std::string& label) {
    std::cout << label << ": ";
    int val = 0;
    std::cin >> val;
    std::cin.ignore();
    return val;
}

double ConsoleView::promptDouble(const std::string& label) {
    std::cout << label << ": ";
    double val = 0.0;
    std::cin >> val;
    std::cin.ignore();
    return val;
}
