#include "TestRunner.h"
#include "src/model/Order.h"
#include "src/controller/OrderController.h"
#include <string>

TEST(order_status_to_string) {
    ASSERT_EQ(orderStatusToString(OrderStatus::RESERVED),  std::string("RESERVED"));
    ASSERT_EQ(orderStatusToString(OrderStatus::REJECTED),  std::string("REJECTED"));
    ASSERT_EQ(orderStatusToString(OrderStatus::PRODUCING), std::string("PRODUCING"));
    ASSERT_EQ(orderStatusToString(OrderStatus::CONFIRMED), std::string("CONFIRMED"));
    ASSERT_EQ(orderStatusToString(OrderStatus::RELEASE),   std::string("RELEASE"));
}

TEST(order_string_to_status) {
    ASSERT_EQ(stringToOrderStatus("RESERVED"),  OrderStatus::RESERVED);
    ASSERT_EQ(stringToOrderStatus("PRODUCING"), OrderStatus::PRODUCING);
    ASSERT_EQ(stringToOrderStatus("RELEASE"),   OrderStatus::RELEASE);
}

TEST(order_controller_place_order_returns_id) {
    OrderController ctrl;
    std::string id = ctrl.placeOrder("S-001", "삼성전자", 100);
    ASSERT_FALSE(id.empty());
}

TEST(order_controller_place_order_starts_with_ORD) {
    OrderController ctrl;
    std::string id = ctrl.placeOrder("S-001", "SK하이닉스", 50);
    ASSERT_TRUE(id.rfind("ORD-", 0) == 0);
}

TEST(order_controller_place_order_status_reserved) {
    OrderController ctrl;
    std::string id = ctrl.placeOrder("S-001", "LG이노텍", 30);
    auto found = ctrl.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, OrderStatus::RESERVED);
}

TEST(order_controller_place_order_fields_correct) {
    OrderController ctrl;
    std::string id = ctrl.placeOrder("S-002", "삼성전자 파운드리", 200);
    auto found = ctrl.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->sampleId, std::string("S-002"));
    ASSERT_EQ(found->customerName, std::string("삼성전자 파운드리"));
    ASSERT_EQ(found->quantity, 200);
}

TEST(order_controller_get_by_status) {
    OrderController ctrl;
    ctrl.placeOrder("S-001", "고객A", 10);
    ctrl.placeOrder("S-002", "고객B", 20);
    auto reserved = ctrl.getByStatus(OrderStatus::RESERVED);
    ASSERT_EQ((int)reserved.size(), 2);
}

TEST(order_controller_order_count) {
    OrderController ctrl;
    ctrl.placeOrder("S-001", "고객A", 10);
    ctrl.placeOrder("S-001", "고객B", 20);
    ctrl.placeOrder("S-001", "고객C", 30);
    ASSERT_EQ(ctrl.getOrderCount(), 3);
}

TEST(order_controller_sequential_ids_different) {
    OrderController ctrl;
    std::string id1 = ctrl.placeOrder("S-001", "고객A", 10);
    std::string id2 = ctrl.placeOrder("S-001", "고객B", 20);
    ASSERT_NE(id1, id2);
}
