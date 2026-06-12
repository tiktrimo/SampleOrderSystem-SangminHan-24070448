#pragma once
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include <string>

class OrderService {
public:
    // approve: CONFIRMED(재고충분) 또는 PRODUCING(재고부족)
    // sample의 stock을 직접 수정하며, ordersOut/samplesOut 벡터를 업데이트
    static bool approve(Order& order, Sample& sample);

    // reject: RESERVED -> REJECTED
    static bool reject(Order& order);

    // release: CONFIRMED -> RELEASE
    static bool release(Order& order);
};
