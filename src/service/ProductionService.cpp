#include "ProductionService.h"

// STUB: 미구현 상태 (RED)
int ProductionService::calcActualProduction(int, double) {
    return 0;
}

double ProductionService::calcTotalTime(double, int) {
    return 0.0;
}

void ProductionService::enqueue(const Order&, const Sample&) {}

bool ProductionService::hasNext() const {
    return false;
}

ProductionItem ProductionService::peek() const {
    return {};
}

ProductionItem ProductionService::complete(Order&, Sample&) {
    return {};
}

int ProductionService::queueSize() const {
    return 0;
}
