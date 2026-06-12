#include "TestRunner.h"
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include "src/service/ProductionService.h"
#include <cmath>

static Order makeProducingOrder(const std::string& id, const std::string& sid, int qty) {
    Order o;
    o.orderId = id;
    o.sampleId = sid;
    o.customerName = "테스트";
    o.quantity = qty;
    o.status = OrderStatus::PRODUCING;
    return o;
}

static Sample makeSample(const std::string& id, int stock, double yield, double avgTime) {
    Sample s;
    s.id = id; s.name = "시료"; s.stock = stock; s.yield = yield;
    s.avgProductionTimeMin = avgTime;
    return s;
}

// 공식: ceil(부족분 / (수율 * 0.9))
TEST(production_calc_formula_basic) {
    // shortage=150, yield=0.9 -> ceil(150/(0.9*0.9)) = ceil(185.18) = 186
    int result = ProductionService::calcActualProduction(150, 0.9);
    ASSERT_EQ(result, 186);
}

TEST(production_calc_formula_exact) {
    // shortage=100, yield=1.0 -> ceil(100/(1.0*0.9)) = ceil(111.11) = 112
    int result = ProductionService::calcActualProduction(100, 1.0);
    ASSERT_EQ(result, 112);
}

TEST(production_calc_formula_small) {
    // shortage=1, yield=0.5 -> ceil(1/(0.5*0.9)) = ceil(2.22) = 3
    int result = ProductionService::calcActualProduction(1, 0.5);
    ASSERT_EQ(result, 3);
}

TEST(production_calc_total_time) {
    // avgTime=0.5min, actual=200 -> 100.0min
    double t = ProductionService::calcTotalTime(0.5, 200);
    ASSERT_EQ(t, 100.0);
}

TEST(production_enqueue_increases_size) {
    ProductionService ps;
    Order o = makeProducingOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o, s);
    ASSERT_EQ(ps.queueSize(), 1);
}

TEST(production_enqueue_calculates_actual_production) {
    // shortage=200(qty), stock=0, yield=0.9 -> ceil(200/(0.9*0.9)) = ceil(246.91) = 247
    ProductionService ps;
    Order o = makeProducingOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o, s);
    ASSERT_TRUE(ps.hasNext());
    auto item = ps.peek();
    ASSERT_EQ(item.actualProduction, 247);
}

TEST(production_fifo_order) {
    ProductionService ps;
    Order o1 = makeProducingOrder("ORD-001", "S-001", 100);
    Order o2 = makeProducingOrder("ORD-002", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o1, s);
    ps.enqueue(o2, s);
    auto item = ps.peek();
    ASSERT_EQ(item.orderId, std::string("ORD-001"));
}

TEST(production_complete_changes_status_to_confirmed) {
    ProductionService ps;
    Order o = makeProducingOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o, s);
    ps.complete(o, s);
    ASSERT_EQ(o.status, OrderStatus::CONFIRMED);
}

TEST(production_complete_increases_stock) {
    ProductionService ps;
    Order o = makeProducingOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o, s);
    int before = s.stock;
    ps.complete(o, s);
    ASSERT_GT(s.stock, before);
}

TEST(production_complete_decreases_queue_size) {
    ProductionService ps;
    Order o = makeProducingOrder("ORD-001", "S-001", 200);
    Sample s = makeSample("S-001", 0, 0.9, 0.5);
    ps.enqueue(o, s);
    ps.complete(o, s);
    ASSERT_EQ(ps.queueSize(), 0);
}
