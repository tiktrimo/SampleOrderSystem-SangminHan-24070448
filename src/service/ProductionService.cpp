#include "ProductionService.h"
#include <cmath>

int ProductionService::calcActualProduction(int shortage, double yield) {
    return static_cast<int>(std::ceil(shortage / (yield * 0.9)));
}

double ProductionService::calcTotalTime(double avgTimeMin, int actualProduction) {
    return avgTimeMin * actualProduction;
}

void ProductionService::enqueue(const Order& order, const Sample& sample, int shortage) {
    int actual = calcActualProduction(shortage, sample.yield);
    double totalTime = calcTotalTime(sample.avgProductionTimeMin, actual);

    ProductionItem item;
    item.orderId = order.orderId;
    item.sampleId = sample.id;
    item.sampleName = sample.name;
    item.orderQuantity = order.quantity;
    item.shortage = shortage;
    item.actualProduction = actual;
    item.totalTimeMin = totalTime;
    item.startTime = time(nullptr);
    queue_.push(item);
}

bool ProductionService::hasNext() const {
    return !queue_.empty();
}

ProductionItem ProductionService::peek() const {
    return queue_.front();
}

ProductionItem ProductionService::complete(Order& order, Sample& sample) {
    ProductionItem item = queue_.front();
    queue_.pop();
    order.status = OrderStatus::CONFIRMED;
    sample.stock += item.actualProduction;
    return item;
}

int ProductionService::queueSize() const {
    return static_cast<int>(queue_.size());
}

std::vector<ProductionItem> ProductionService::getQueueItems() const {
    std::queue<ProductionItem> copy = queue_;
    std::vector<ProductionItem> items;
    while (!copy.empty()) {
        items.push_back(copy.front());
        copy.pop();
    }
    return items;
}
