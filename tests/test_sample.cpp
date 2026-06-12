#include "TestRunner.h"
#include "src/model/Sample.h"
#include "src/controller/SampleController.h"

TEST(sample_model_fields) {
    Sample s;
    s.id = "S-001";
    s.name = "실리콘 웨이퍼";
    s.avgProductionTimeMin = 0.5;
    s.yield = 0.92;
    s.stock = 100;
    ASSERT_EQ(s.id, std::string("S-001"));
    ASSERT_EQ(s.name, std::string("실리콘 웨이퍼"));
    ASSERT_EQ(s.stock, 100);
}

TEST(sample_controller_add_and_find) {
    SampleController ctrl;
    Sample s;
    s.id = "S-001"; s.name = "실리콘 웨이퍼"; s.stock = 100;
    ctrl.addSample(s);
    auto found = ctrl.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, std::string("실리콘 웨이퍼"));
    ASSERT_EQ(found->stock, 100);
}

TEST(sample_controller_find_not_found) {
    SampleController ctrl;
    auto found = ctrl.findById("MISSING");
    ASSERT_FALSE(found.has_value());
}

TEST(sample_controller_get_all) {
    SampleController ctrl;
    Sample s1; s1.id = "S-001"; s1.name = "시료1"; s1.stock = 100;
    Sample s2; s2.id = "S-002"; s2.name = "시료2"; s2.stock = 50;
    ctrl.addSample(s1);
    ctrl.addSample(s2);
    ASSERT_EQ(ctrl.getSampleCount(), 2);
    ASSERT_EQ((int)ctrl.getAll().size(), 2);
}

TEST(sample_controller_search_by_name) {
    SampleController ctrl;
    Sample s1; s1.id = "S-001"; s1.name = "실리콘 웨이퍼"; s1.stock = 100;
    Sample s2; s2.id = "S-002"; s2.name = "GaN 기판"; s2.stock = 50;
    ctrl.addSample(s1);
    ctrl.addSample(s2);
    auto results = ctrl.searchByName("실리콘");
    ASSERT_EQ((int)results.size(), 1);
    ASSERT_EQ(results[0].id, std::string("S-001"));
}

TEST(sample_controller_update_stock) {
    SampleController ctrl;
    Sample s; s.id = "S-001"; s.name = "시료"; s.stock = 100;
    ctrl.addSample(s);
    bool ok = ctrl.updateStock("S-001", -30);
    ASSERT_TRUE(ok);
    auto found = ctrl.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->stock, 70);
}

TEST(sample_controller_duplicate_id_ignored) {
    SampleController ctrl;
    Sample s; s.id = "S-001"; s.name = "시료1"; s.stock = 100;
    ctrl.addSample(s);
    Sample dup; dup.id = "S-001"; dup.name = "중복"; dup.stock = 50;
    ctrl.addSample(dup);
    ASSERT_EQ(ctrl.getSampleCount(), 1);
}

TEST(sample_controller_total_stock) {
    SampleController ctrl;
    Sample s1; s1.id = "S-001"; s1.name = "시료1"; s1.stock = 100;
    Sample s2; s2.id = "S-002"; s2.name = "시료2"; s2.stock = 50;
    ctrl.addSample(s1);
    ctrl.addSample(s2);
    ASSERT_EQ(ctrl.getTotalStock(), 150);
}
