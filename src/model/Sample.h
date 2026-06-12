#pragma once
#include <string>

struct Sample {
    std::string id;
    std::string name;
    double avgProductionTimeMin = 0.0;
    double yield = 0.0;
    int stock = 0;
};
