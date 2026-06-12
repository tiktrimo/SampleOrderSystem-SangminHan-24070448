#pragma once
#include "src/model/Order.h"
#include "src/model/Sample.h"
#include <ctime>
#include <queue>
#include <string>
#include <vector>

struct ProductionItem {
    std::string orderId;
    std::string sampleId;
    std::string sampleName;
    int orderQuantity = 0;
    int actualProduction = 0;
    double totalTimeMin = 0.0;
    std::time_t startTime = 0; // enqueue 시각 (Unix timestamp)
};

class ProductionService {
public:
    // 실 생산량 = ceil(부족분 / (수율 * 0.9))
    static int calcActualProduction(int shortage, double yield);
    // 총 생산 시간 = 평균 생산시간 * 실 생산량
    static double calcTotalTime(double avgTimeMin, int actualProduction);

    // shortage = 주문량 - 승인 시 기존 재고 (부족분)
    void enqueue(const Order& order, const Sample& sample, int shortage);
    bool hasNext() const;
    ProductionItem peek() const;
    // 완료 처리: 큐에서 꺼내고 order 상태를 CONFIRMED로, sample 재고 증가
    ProductionItem complete(Order& order, Sample& sample);

    int queueSize() const;
    std::vector<ProductionItem> getQueueItems() const;

private:
    std::queue<ProductionItem> queue_;
};
