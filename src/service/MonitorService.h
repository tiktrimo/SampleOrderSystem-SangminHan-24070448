#pragma once
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include <map>
#include <vector>
#include <string>

enum class StockStatus { SUFFICIENT, LOW, DEPLETED };

struct SampleStockInfo {
    Sample sample;
    int pendingQuantity = 0;
    StockStatus stockStatus = StockStatus::SUFFICIENT;
};

struct MonitorSnapshot {
    std::map<OrderStatus, int> orderCounts;
    std::vector<SampleStockInfo> stockInfos;
};

class MonitorService {
public:
    // REJECTED 제외, 재고 상태 분류
    static MonitorSnapshot buildSnapshot(const std::vector<Order>& orders,
                                         const std::vector<Sample>& samples);
    static StockStatus classifyStock(int stock, int pendingQty);
};
