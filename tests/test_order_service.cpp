#include "TestRunner.h"
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include "src/service/OrderService.h"

static Order makeReservedOrder(const std::string& id, const std::string& sampleId, int qty) {
    Order o;
    o.orderId = id;
    o.sampleId = sampleId;
    o.customerName = "테스트고객";
    o.quantity = qty;
    o.status = OrderStatus::RESERVED;
    return o;
}

static Sample makeSample(const std::string& id, int stock, double yield = 0.9) {
    Sample s;
    s.id = id;
    s.name = "테스트시료";
    s.avgProductionTimeMin = 1.0;
    s.yield = yield;
    s.stock = stock;
    return s;
}

TEST(approve_sufficient_stock_becomes_confirmed) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    Sample s = makeSample("S-001", 100);
    bool ok = OrderService::approve(o, s);
    ASSERT_TRUE(ok);
    ASSERT_EQ(o.status, OrderStatus::CONFIRMED);
}

TEST(approve_sufficient_stock_does_not_change_stock) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    Sample s = makeSample("S-001", 100);
    OrderService::approve(o, s);
    ASSERT_EQ(s.stock, 100); // 재고는 출고 시에만 차감
}

TEST(approve_exact_stock_becomes_confirmed) {
    Order o = makeReservedOrder("ORD-001", "S-001", 100);
    Sample s = makeSample("S-001", 100);
    bool ok = OrderService::approve(o, s);
    ASSERT_TRUE(ok);
    ASSERT_EQ(o.status, OrderStatus::CONFIRMED);
    ASSERT_EQ(s.stock, 100); // 재고는 출고 시에만 차감
}

TEST(approve_insufficient_stock_becomes_producing) {
    Order o = makeReservedOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 50);
    bool ok = OrderService::approve(o, s);
    ASSERT_TRUE(ok);
    ASSERT_EQ(o.status, OrderStatus::PRODUCING);
}

TEST(approve_insufficient_stock_does_not_change_stock) {
    Order o = makeReservedOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 50);
    OrderService::approve(o, s);
    ASSERT_EQ(s.stock, 50); // 재고는 출고 시에만 차감
}

TEST(approve_only_reserved_orders) {
    Order o = makeReservedOrder("ORD-001", "S-001", 10);
    o.status = OrderStatus::CONFIRMED;
    Sample s = makeSample("S-001", 100);
    bool ok = OrderService::approve(o, s);
    ASSERT_FALSE(ok);
}

TEST(reject_reserved_becomes_rejected) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    bool ok = OrderService::reject(o);
    ASSERT_TRUE(ok);
    ASSERT_EQ(o.status, OrderStatus::REJECTED);
}

TEST(reject_non_reserved_fails) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    o.status = OrderStatus::CONFIRMED;
    bool ok = OrderService::reject(o);
    ASSERT_FALSE(ok);
}

TEST(release_confirmed_becomes_release) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    o.status = OrderStatus::CONFIRMED;
    bool ok = OrderService::release(o);
    ASSERT_TRUE(ok);
    ASSERT_EQ(o.status, OrderStatus::RELEASE);
}

TEST(release_non_confirmed_fails) {
    Order o = makeReservedOrder("ORD-001", "S-001", 50);
    bool ok = OrderService::release(o);
    ASSERT_FALSE(ok);
}
