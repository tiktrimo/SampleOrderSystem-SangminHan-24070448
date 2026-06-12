#pragma once
#include <string>
#include <vector>
#include <optional>
#include "src/model/Sample.h"

class SampleController {
public:
    void addSample(const Sample& sample);
    std::vector<Sample> getAll() const;
    std::optional<Sample> findById(const std::string& id) const;
    std::vector<Sample> searchByName(const std::string& keyword) const;
    bool updateStock(const std::string& id, int delta);
    int getSampleCount() const;
    int getTotalStock() const;

private:
    std::vector<Sample> samples_;
    bool exists(const std::string& id) const;
};
