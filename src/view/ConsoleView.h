#pragma once
#include <string>
#include <vector>
#include "src/model/Sample.h"
#include "src/model/Order.h"
#include "src/service/MonitorService.h"
#include "src/service/ProductionService.h"

class ConsoleView {
public:
    static void showMainMenu(int sampleCount, int totalStock,
                             int orderCount, int producingCount);
    static void showSamples(const std::vector<Sample>& samples);
    static void showOrders(const std::vector<Order>& orders);
    static void showSelectableOrderList(const std::vector<Order>& orders);
    static void showMonitor(const MonitorSnapshot& snap);
    static void showProductionQueue(ProductionService& ps);

    static std::string promptString(const std::string& label);
    static int promptInt(const std::string& label);
    static double promptDouble(const std::string& label);
};
