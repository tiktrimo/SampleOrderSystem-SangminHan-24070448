#include "SampleController.h"

// STUB: 미구현 상태 (RED) — 테스트가 실패해야 정상입니다
void SampleController::addSample(const Sample&) {}

std::vector<Sample> SampleController::getAll() const {
    return {};
}

std::optional<Sample> SampleController::findById(const std::string&) const {
    return std::nullopt;
}

std::vector<Sample> SampleController::searchByName(const std::string&) const {
    return {};
}

bool SampleController::updateStock(const std::string&, int) {
    return false;
}

int SampleController::getSampleCount() const {
    return 0;
}

int SampleController::getTotalStock() const {
    return 0;
}
