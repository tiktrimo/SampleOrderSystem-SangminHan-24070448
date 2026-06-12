#pragma once
#include "IRepository.h"
#include "src/model/Sample.h"
#include <string>
#include <vector>
#include <optional>

class SampleRepository : public IRepository<Sample> {
public:
    explicit SampleRepository(const std::string& filePath);

    void save(const Sample& item) override;
    bool update(const Sample& item) override;
    bool remove(const std::string& id) override;
    std::optional<Sample> findById(const std::string& id) const override;
    std::vector<Sample> findAll() const override;
    bool exists(const std::string& id) const override;

private:
    std::string filePath_;
    std::vector<Sample> cache_;

    void loadAll();
    void saveAll() const;
    static Sample parseLine(const std::string& line);
    static std::string toLine(const Sample& s);
};
