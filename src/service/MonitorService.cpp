#include "MonitorService.h"

// STUB: 미구현 상태 (RED)
MonitorSnapshot MonitorService::buildSnapshot(const std::vector<Order>&,
                                              const std::vector<Sample>&) {
    return {};
}

StockStatus MonitorService::classifyStock(int, int) {
    return StockStatus::SUFFICIENT;
}
