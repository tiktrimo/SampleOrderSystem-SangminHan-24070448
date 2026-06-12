#include "SampleController.h"
#include <algorithm>
#include <iterator>
#include <numeric>

void SampleController::addSample(const Sample& sample) {
    if (exists(sample.id)) return;
    samples_.push_back(sample);
}

std::vector<Sample> SampleController::getAll() const {
    return samples_;
}

std::optional<Sample> SampleController::findById(const std::string& id) const {
    auto it = std::find_if(samples_.begin(), samples_.end(),
        [&](const Sample& s) { return s.id == id; });
    return it != samples_.end() ? std::optional<Sample>(*it) : std::nullopt;
}

std::vector<Sample> SampleController::searchByName(const std::string& keyword) const {
    std::vector<Sample> result;
    std::copy_if(samples_.begin(), samples_.end(), std::back_inserter(result),
        [&](const Sample& s) { return s.name.find(keyword) != std::string::npos; });
    return result;
}

bool SampleController::updateStock(const std::string& id, int delta) {
    auto it = std::find_if(samples_.begin(), samples_.end(),
        [&](const Sample& s) { return s.id == id; });
    if (it == samples_.end()) return false;
    it->stock += delta;
    return true;
}

int SampleController::getSampleCount() const {
    return static_cast<int>(samples_.size());
}

int SampleController::getTotalStock() const {
    return std::accumulate(samples_.begin(), samples_.end(), 0,
        [](int sum, const Sample& s) { return sum + s.stock; });
}

bool SampleController::exists(const std::string& id) const {
    return std::any_of(samples_.begin(), samples_.end(),
        [&](const Sample& s) { return s.id == id; });
}
