#pragma once
#include "IRepository.h"
#include "src/model/Order.h"
#include <string>
#include <vector>
#include <optional>

class OrderRepository : public IRepository<Order> {
public:
    explicit OrderRepository(const std::string& filePath);

    void save(const Order& item) override;
    bool update(const Order& item) override;
    bool remove(const std::string& id) override;
    std::optional<Order> findById(const std::string& id) const override;
    std::vector<Order> findAll() const override;
    bool exists(const std::string& id) const override;

    std::vector<Order> findByStatus(OrderStatus status) const;

private:
    std::string filePath_;
    std::vector<Order> cache_;

    void loadAll();
    void saveAll() const;
    static Order parseLine(const std::string& line);
    static std::string toLine(const Order& o);
};
