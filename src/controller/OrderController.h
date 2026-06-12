#pragma once
#include <string>
#include <vector>
#include <optional>
#include "src/model/Order.h"

class OrderController {
public:
    std::string placeOrder(const std::string& sampleId,
                           const std::string& customerName,
                           int quantity);
    std::vector<Order> getAll() const;
    std::vector<Order> getByStatus(OrderStatus status) const;
    std::optional<Order> findById(const std::string& orderId) const;
    bool updateOrder(const Order& order);
    int getOrderCount() const;
    void syncSequence(); // 파일에서 로드 후 orderSeq_ 동기화 (중복 ID 방지)

    std::vector<Order> orders_;

private:
    int orderSeq_ = 0;
    std::string generateOrderId();
};
