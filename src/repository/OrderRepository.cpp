#include "OrderRepository.h"
#include <fstream>
#include <sstream>
#include <algorithm>

OrderRepository::OrderRepository(const std::string& filePath)
    : filePath_(filePath) {
    loadAll();
}

void OrderRepository::loadAll() {
    cache_.clear();
    std::ifstream ifs(filePath_);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) cache_.push_back(parseLine(line));
    }
}

void OrderRepository::saveAll() const {
    std::ofstream ofs(filePath_);
    for (const auto& o : cache_) ofs << toLine(o) << "\n";
}

Order OrderRepository::parseLine(const std::string& line) {
    std::istringstream ss(line);
    Order o;
    std::string tok;
    std::getline(ss, o.orderId, '|');
    std::getline(ss, o.sampleId, '|');
    std::getline(ss, o.customerName, '|');
    std::getline(ss, tok, '|'); o.quantity = std::stoi(tok);
    std::getline(ss, tok, '|'); o.status = stringToOrderStatus(tok);
    if (std::getline(ss, tok, '|')) {
        try { o.shortage = std::stoi(tok); } catch (...) {}
    }
    return o;
}

std::string OrderRepository::toLine(const Order& o) {
    return o.orderId + "|" + o.sampleId + "|" + o.customerName + "|" +
        std::to_string(o.quantity) + "|" + orderStatusToString(o.status) + "|" +
        std::to_string(o.shortage);
}

void OrderRepository::save(const Order& item) {
    if (exists(item.orderId)) return;
    cache_.push_back(item);
    saveAll();
}

bool OrderRepository::update(const Order& item) {
    for (auto& o : cache_) {
        if (o.orderId == item.orderId) {
            o = item;
            saveAll();
            return true;
        }
    }
    return false;
}

bool OrderRepository::remove(const std::string& id) {
    auto it = std::remove_if(cache_.begin(), cache_.end(),
        [&id](const Order& o) { return o.orderId == id; });
    if (it == cache_.end()) return false;
    cache_.erase(it, cache_.end());
    saveAll();
    return true;
}

std::optional<Order> OrderRepository::findById(const std::string& id) const {
    for (const auto& o : cache_) {
        if (o.orderId == id) return o;
    }
    return std::nullopt;
}

std::vector<Order> OrderRepository::findAll() const {
    return cache_;
}

bool OrderRepository::exists(const std::string& id) const {
    for (const auto& o : cache_) {
        if (o.orderId == id) return true;
    }
    return false;
}

std::vector<Order> OrderRepository::findByStatus(OrderStatus status) const {
    std::vector<Order> result;
    for (const auto& o : cache_) {
        if (o.status == status) result.push_back(o);
    }
    return result;
}
