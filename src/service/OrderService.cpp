#include "OrderService.h"

bool OrderService::approve(Order& order, Sample& sample) {
    if (order.status != OrderStatus::RESERVED) return false;

    if (sample.stock >= order.quantity) {
        order.status = OrderStatus::CONFIRMED;
    } else {
        order.status = OrderStatus::PRODUCING;
    }
    return true;
}

bool OrderService::reject(Order& order) {
    if (order.status != OrderStatus::RESERVED) return false;
    order.status = OrderStatus::REJECTED;
    return true;
}

bool OrderService::release(Order& order) {
    if (order.status != OrderStatus::CONFIRMED) return false;
    order.status = OrderStatus::RELEASE;
    return true;
}
