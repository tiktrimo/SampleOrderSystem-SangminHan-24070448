#include "SampleController.h"
#include <algorithm>

void SampleController::addSample(const Sample& sample) {
    if (exists(sample.id)) return;
    samples_.push_back(sample);
}

std::vector<Sample> SampleController::getAll() const {
    return samples_;
}

std::optional<Sample> SampleController::findById(const std::string& id) const {
    for (const auto& s : samples_) {
        if (s.id == id) return s;
    }
    return std::nullopt;
}

std::vector<Sample> SampleController::searchByName(const std::string& keyword) const {
    std::vector<Sample> result;
    for (const auto& s : samples_) {
        if (s.name.find(keyword) != std::string::npos) {
            result.push_back(s);
        }
    }
    return result;
}

bool SampleController::updateStock(const std::string& id, int delta) {
    for (auto& s : samples_) {
        if (s.id == id) {
            s.stock += delta;
            return true;
        }
    }
    return false;
}

int SampleController::getSampleCount() const {
    return static_cast<int>(samples_.size());
}

int SampleController::getTotalStock() const {
    int total = 0;
    for (const auto& s : samples_) total += s.stock;
    return total;
}

bool SampleController::exists(const std::string& id) const {
    for (const auto& s : samples_) {
        if (s.id == id) return true;
    }
    return false;
}
