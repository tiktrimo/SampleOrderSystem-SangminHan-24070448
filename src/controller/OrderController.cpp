#include "OrderController.h"

// STUB: 미구현 상태 (RED)
std::string OrderController::placeOrder(const std::string&, const std::string&, int) {
    return "";
}

std::vector<Order> OrderController::getAll() const {
    return {};
}

std::vector<Order> OrderController::getByStatus(OrderStatus) const {
    return {};
}

std::optional<Order> OrderController::findById(const std::string&) const {
    return std::nullopt;
}

int OrderController::getOrderCount() const {
    return 0;
}

std::string OrderController::generateOrderId() {
    return "";
}
