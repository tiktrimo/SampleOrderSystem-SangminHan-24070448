#include "MonitorService.h"

StockStatus MonitorService::classifyStock(int stock, int pendingQty) {
    if (stock == 0) return StockStatus::DEPLETED;
    if (stock < pendingQty) return StockStatus::LOW;
    return StockStatus::SUFFICIENT;
}

MonitorSnapshot MonitorService::buildSnapshot(const std::vector<Order>& orders,
                                              const std::vector<Sample>& samples) {
    MonitorSnapshot snap;

    // 상태별 주문건수 (REJECTED 제외)
    for (const auto& o : orders) {
        if (o.status == OrderStatus::REJECTED) continue;
        snap.orderCounts[o.status]++;
    }

    // 시료별 pending 수량 = RESERVED + PRODUCING 주문의 quantity 합산
    std::map<std::string, int> pendingBySample;
    for (const auto& o : orders) {
        if (o.status == OrderStatus::RESERVED || o.status == OrderStatus::PRODUCING) {
            pendingBySample[o.sampleId] += o.quantity;
        }
    }

    for (const auto& s : samples) {
        int pending = 0;
        auto it = pendingBySample.find(s.id);
        if (it != pendingBySample.end()) pending = it->second;

        SampleStockInfo info;
        info.sample = s;
        info.pendingQuantity = pending;
        info.stockStatus = classifyStock(s.stock, pending);
        snap.stockInfos.push_back(info);
    }

    return snap;
}
