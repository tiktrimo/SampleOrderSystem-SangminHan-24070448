#include "Order.h"
#include <stdexcept>

std::string orderStatusToString(OrderStatus status) {
    switch (status) {
    case OrderStatus::RESERVED:  return "RESERVED";
    case OrderStatus::REJECTED:  return "REJECTED";
    case OrderStatus::PRODUCING: return "PRODUCING";
    case OrderStatus::CONFIRMED: return "CONFIRMED";
    case OrderStatus::RELEASE:   return "RELEASE";
    default: return "UNKNOWN";
    }
}

OrderStatus stringToOrderStatus(const std::string& str) {
    if (str == "RESERVED")  return OrderStatus::RESERVED;
    if (str == "REJECTED")  return OrderStatus::REJECTED;
    if (str == "PRODUCING") return OrderStatus::PRODUCING;
    if (str == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (str == "RELEASE")   return OrderStatus::RELEASE;
    throw std::invalid_argument("알 수 없는 OrderStatus: " + str);
}
