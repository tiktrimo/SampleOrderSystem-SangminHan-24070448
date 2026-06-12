#include "SampleRepository.h"
#include <fstream>
#include <sstream>
#include <algorithm>

SampleRepository::SampleRepository(const std::string& filePath)
    : filePath_(filePath) {
    loadAll();
}

void SampleRepository::loadAll() {
    cache_.clear();
    std::ifstream ifs(filePath_);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) cache_.push_back(parseLine(line));
    }
}

void SampleRepository::saveAll() const {
    std::ofstream ofs(filePath_);
    for (const auto& s : cache_) ofs << toLine(s) << "\n";
}

Sample SampleRepository::parseLine(const std::string& line) {
    std::istringstream ss(line);
    Sample s;
    std::string tok;
    std::getline(ss, s.id, '|');
    std::getline(ss, s.name, '|');
    std::getline(ss, tok, '|'); s.avgProductionTimeMin = std::stod(tok);
    std::getline(ss, tok, '|'); s.yield = std::stod(tok);
    std::getline(ss, tok, '|'); s.stock = std::stoi(tok);
    return s;
}

std::string SampleRepository::toLine(const Sample& s) {
    return s.id + "|" + s.name + "|" +
        std::to_string(s.avgProductionTimeMin) + "|" +
        std::to_string(s.yield) + "|" +
        std::to_string(s.stock);
}

void SampleRepository::save(const Sample& item) {
    if (exists(item.id)) return;
    cache_.push_back(item);
    saveAll();
}

bool SampleRepository::update(const Sample& item) {
    for (auto& s : cache_) {
        if (s.id == item.id) {
            s = item;
            saveAll();
            return true;
        }
    }
    return false;
}

bool SampleRepository::remove(const std::string& id) {
    auto it = std::remove_if(cache_.begin(), cache_.end(),
        [&id](const Sample& s) { return s.id == id; });
    if (it == cache_.end()) return false;
    cache_.erase(it, cache_.end());
    saveAll();
    return true;
}

std::optional<Sample> SampleRepository::findById(const std::string& id) const {
    for (const auto& s : cache_) {
        if (s.id == id) return s;
    }
    return std::nullopt;
}

std::vector<Sample> SampleRepository::findAll() const {
    return cache_;
}

bool SampleRepository::exists(const std::string& id) const {
    for (const auto& s : cache_) {
        if (s.id == id) return true;
    }
    return false;
}
