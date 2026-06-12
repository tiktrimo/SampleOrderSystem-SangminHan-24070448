#include "TestRunner.h"
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include "src/service/MonitorService.h"

static Order makeOrder(const std::string& oid, const std::string& sid,
                       int qty, OrderStatus st) {
    Order o; o.orderId = oid; o.sampleId = sid;
    o.customerName = "c"; o.quantity = qty; o.status = st;
    return o;
}

static Sample makeSample(const std::string& id, int stock) {
    Sample s; s.id = id; s.name = "시료"; s.stock = stock;
    s.avgProductionTimeMin = 1.0; s.yield = 0.9;
    return s;
}

TEST(monitor_order_counts_exclude_rejected) {
    std::vector<Order> orders = {
        makeOrder("O1", "S-001", 10, OrderStatus::RESERVED),
        makeOrder("O2", "S-001", 20, OrderStatus::PRODUCING),
        makeOrder("O3", "S-001", 30, OrderStatus::REJECTED),
        makeOrder("O4", "S-001", 40, OrderStatus::CONFIRMED),
        makeOrder("O5", "S-001", 50, OrderStatus::RELEASE)
    };
    std::vector<Sample> samples = { makeSample("S-001", 100) };
    auto snap = MonitorService::buildSnapshot(orders, samples);
    ASSERT_TRUE(snap.orderCounts.find(OrderStatus::REJECTED) == snap.orderCounts.end()
             || snap.orderCounts.at(OrderStatus::REJECTED) == 0);
    ASSERT_EQ(snap.orderCounts.at(OrderStatus::RESERVED), 1);
    ASSERT_EQ(snap.orderCounts.at(OrderStatus::PRODUCING), 1);
    ASSERT_EQ(snap.orderCounts.at(OrderStatus::CONFIRMED), 1);
    ASSERT_EQ(snap.orderCounts.at(OrderStatus::RELEASE), 1);
}

TEST(monitor_stock_status_depleted) {
    ASSERT_EQ(MonitorService::classifyStock(0, 0), StockStatus::DEPLETED);
}

TEST(monitor_stock_status_low) {
    ASSERT_EQ(MonitorService::classifyStock(30, 50), StockStatus::LOW);
}

TEST(monitor_stock_status_sufficient) {
    ASSERT_EQ(MonitorService::classifyStock(100, 50), StockStatus::SUFFICIENT);
}

TEST(monitor_stock_status_exact_sufficient) {
    ASSERT_EQ(MonitorService::classifyStock(50, 50), StockStatus::SUFFICIENT);
}

TEST(monitor_snapshot_stock_infos_count) {
    std::vector<Sample> samples = { makeSample("S-001", 100), makeSample("S-002", 0) };
    std::vector<Order> orders;
    auto snap = MonitorService::buildSnapshot(orders, samples);
    ASSERT_EQ((int)snap.stockInfos.size(), 2);
}

TEST(monitor_snapshot_pending_qty_accumulates) {
    std::vector<Order> orders = {
        makeOrder("O1", "S-001", 30, OrderStatus::RESERVED),
        makeOrder("O2", "S-001", 20, OrderStatus::PRODUCING),
        makeOrder("O3", "S-001", 50, OrderStatus::CONFIRMED)
    };
    std::vector<Sample> samples = { makeSample("S-001", 100) };
    auto snap = MonitorService::buildSnapshot(orders, samples);
    ASSERT_EQ((int)snap.stockInfos.size(), 1);  // 가드: 비어있으면 예외로 중단
    ASSERT_EQ(snap.stockInfos[0].pendingQuantity, 50);
}
