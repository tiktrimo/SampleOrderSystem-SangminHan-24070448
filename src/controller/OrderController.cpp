#include "OrderController.h"
#include <algorithm>
#include <iterator>
#include <ctime>
#include <sstream>
#include <iomanip>

void OrderController::loadOrders(const std::vector<Order>& orders) {
    orders_ = orders;
}

std::string OrderController::placeOrder(const std::string& sampleId,
                                        const std::string& customerName,
                                        int quantity) {
    Order o;
    o.orderId = generateOrderId();
    o.sampleId = sampleId;
    o.customerName = customerName;
    o.quantity = quantity;
    o.status = OrderStatus::RESERVED;
    orders_.push_back(o);
    return o.orderId;
}

std::vector<Order> OrderController::getAll() const {
    return orders_;
}

std::vector<Order> OrderController::getByStatus(OrderStatus status) const {
    std::vector<Order> result;
    std::copy_if(orders_.begin(), orders_.end(), std::back_inserter(result),
        [&](const Order& o) { return o.status == status; });
    return result;
}

std::optional<Order> OrderController::findById(const std::string& orderId) const {
    auto it = std::find_if(orders_.begin(), orders_.end(),
        [&](const Order& o) { return o.orderId == orderId; });
    return it != orders_.end() ? std::optional<Order>(*it) : std::nullopt;
}

bool OrderController::updateOrder(const Order& order) {
    auto it = std::find_if(orders_.begin(), orders_.end(),
        [&](const Order& o) { return o.orderId == order.orderId; });
    if (it == orders_.end()) return false;
    *it = order;
    return true;
}

int OrderController::getOrderCount() const {
    return static_cast<int>(std::count_if(orders_.begin(), orders_.end(),
        [](const Order& o) {
            return o.status != OrderStatus::REJECTED && o.status != OrderStatus::RELEASE;
        }));
}

void OrderController::syncSequence() {
    for (const auto& o : orders_) {
        auto pos = o.orderId.rfind('-');
        if (pos != std::string::npos) {
            try {
                int seq = std::stoi(o.orderId.substr(pos + 1));
                if (seq > orderSeq_) orderSeq_ = seq;
            } catch (...) {}
        }
    }
}

std::string OrderController::generateOrderId() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << "ORD-" << std::put_time(&tm, "%Y%m%d") << "-"
        << std::setfill('0') << std::setw(4) << (++orderSeq_);
    return oss.str();
}
