#include "TestRunner.h"
#include "src/repository/SampleRepository.h"
#include "src/repository/OrderRepository.h"
#include <cstdio>
#include <string>

static std::string tmpPath(const std::string& name) {
    char* t = nullptr; size_t sz = 0;
    _dupenv_s(&t, &sz, "TEMP");
    std::string dir = (t && sz > 0) ? t : ".";
    free(t);
    return dir + "\\" + name;
}

// ─── SampleRepository ────────────────────────────────────────────────────────

TEST(sr_save_and_findById) {
    auto p = tmpPath("sr_save.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="S-001"; s.name="Silicon Wafer"; s.avgProductionTimeMin=0.5; s.yield=0.92; s.stock=100;
    repo.save(s);
    auto found = repo.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, std::string("Silicon Wafer"));
    ASSERT_EQ(found->stock, 100);
    std::remove(p.c_str());
}

TEST(sr_save_multiple_findAll) {
    auto p = tmpPath("sr_all.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s1; s1.id="S-001"; s1.name="A"; s1.avgProductionTimeMin=0.5; s1.yield=0.9; s1.stock=50;
    Sample s2; s2.id="S-002"; s2.name="B"; s2.avgProductionTimeMin=1.0; s2.yield=0.8; s2.stock=30;
    Sample s3; s3.id="S-003"; s3.name="C"; s3.avgProductionTimeMin=0.3; s3.yield=0.75; s3.stock=0;
    repo.save(s1); repo.save(s2); repo.save(s3);
    ASSERT_EQ((int)repo.findAll().size(), 3);
    std::remove(p.c_str());
}

TEST(sr_update_stock_persists) {
    auto p = tmpPath("sr_update.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="S-001"; s.name="GaN"; s.avgProductionTimeMin=0.8; s.yield=0.85; s.stock=200;
    repo.save(s);
    s.stock = 150;
    repo.update(s);
    auto found = repo.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->stock, 150);
    std::remove(p.c_str());
}

TEST(sr_update_nonexistent_returns_false) {
    auto p = tmpPath("sr_upd_none.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="GHOST"; s.name="X"; s.avgProductionTimeMin=1.0; s.yield=0.9; s.stock=0;
    bool ok = repo.update(s);
    ASSERT_FALSE(ok);
    std::remove(p.c_str());
}

TEST(sr_remove_existing) {
    auto p = tmpPath("sr_remove.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="S-001"; s.name="SiC"; s.avgProductionTimeMin=1.2; s.yield=0.88; s.stock=80;
    repo.save(s);
    ASSERT_TRUE(repo.remove("S-001"));
    ASSERT_EQ((int)repo.findAll().size(), 0);
    std::remove(p.c_str());
}

TEST(sr_remove_nonexistent_returns_false) {
    auto p = tmpPath("sr_rem_none.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    ASSERT_FALSE(repo.remove("GHOST"));
    std::remove(p.c_str());
}

TEST(sr_duplicate_save_ignored) {
    auto p = tmpPath("sr_dup.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="S-001"; s.name="X"; s.avgProductionTimeMin=0.5; s.yield=0.9; s.stock=10;
    repo.save(s); repo.save(s); repo.save(s);
    ASSERT_EQ((int)repo.findAll().size(), 1);
    std::remove(p.c_str());
}

TEST(sr_exists_before_and_after_save) {
    auto p = tmpPath("sr_exists.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    ASSERT_FALSE(repo.exists("S-001"));
    Sample s; s.id="S-001"; s.name="X"; s.avgProductionTimeMin=0.5; s.yield=0.9; s.stock=10;
    repo.save(s);
    ASSERT_TRUE(repo.exists("S-001"));
    std::remove(p.c_str());
}

TEST(sr_persistence_reload_after_restart) {
    auto p = tmpPath("sr_reload.dat"); std::remove(p.c_str());
    {
        SampleRepository repo(p);
        Sample s; s.id="S-001"; s.name="Diamond"; s.avgProductionTimeMin=2.0; s.yield=0.7; s.stock=5;
        repo.save(s);
        s.stock = 3; repo.update(s);
    }
    // 새 인스턴스로 재로드 — 데이터 영속성 검증
    SampleRepository repo2(p);
    auto found = repo2.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->stock, 3);
    ASSERT_EQ(found->name, std::string("Diamond"));
    std::remove(p.c_str());
}

TEST(sr_yield_and_time_roundtrip) {
    auto p = tmpPath("sr_float.dat"); std::remove(p.c_str());
    SampleRepository repo(p);
    Sample s; s.id="S-001"; s.name="X"; s.avgProductionTimeMin=1.25; s.yield=0.875; s.stock=0;
    repo.save(s);
    auto found = repo.findById("S-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(std::abs(found->yield - 0.875) < 1e-6);
    ASSERT_TRUE(std::abs(found->avgProductionTimeMin - 1.25) < 1e-6);
    std::remove(p.c_str());
}

// ─── OrderRepository ─────────────────────────────────────────────────────────

TEST(or_save_and_findById) {
    auto p = tmpPath("or_save.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    Order o; o.orderId="ORD-001"; o.sampleId="S-001"; o.customerName="Samsung";
    o.quantity=50; o.status=OrderStatus::RESERVED;
    repo.save(o);
    auto found = repo.findById("ORD-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->customerName, std::string("Samsung"));
    ASSERT_EQ(found->quantity, 50);
    ASSERT_EQ(found->status, OrderStatus::RESERVED);
    std::remove(p.c_str());
}

TEST(or_update_status_persists) {
    auto p = tmpPath("or_update.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    Order o; o.orderId="ORD-001"; o.sampleId="S-001"; o.customerName="SK"; o.quantity=30; o.status=OrderStatus::RESERVED;
    repo.save(o);
    o.status = OrderStatus::CONFIRMED;
    repo.update(o);
    auto found = repo.findById("ORD-001");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->status, OrderStatus::CONFIRMED);
    std::remove(p.c_str());
}

TEST(or_update_nonexistent_returns_false) {
    auto p = tmpPath("or_upd_none.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    Order o; o.orderId="GHOST"; o.sampleId="S-001"; o.customerName="X"; o.quantity=1; o.status=OrderStatus::RESERVED;
    ASSERT_FALSE(repo.update(o));
    std::remove(p.c_str());
}

TEST(or_find_by_status_filters_correctly) {
    auto p = tmpPath("or_status.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    auto mk = [](const std::string& id, OrderStatus st) {
        Order o; o.orderId=id; o.sampleId="S-001"; o.customerName="c"; o.quantity=10; o.status=st;
        return o;
    };
    repo.save(mk("O1", OrderStatus::RESERVED));
    repo.save(mk("O2", OrderStatus::PRODUCING));
    repo.save(mk("O3", OrderStatus::RESERVED));
    repo.save(mk("O4", OrderStatus::CONFIRMED));
    repo.save(mk("O5", OrderStatus::REJECTED));
    ASSERT_EQ((int)repo.findByStatus(OrderStatus::RESERVED).size(), 2);
    ASSERT_EQ((int)repo.findByStatus(OrderStatus::PRODUCING).size(), 1);
    ASSERT_EQ((int)repo.findByStatus(OrderStatus::REJECTED).size(), 1);
    std::remove(p.c_str());
}

TEST(or_remove_existing) {
    auto p = tmpPath("or_remove.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    Order o; o.orderId="ORD-001"; o.sampleId="S-001"; o.customerName="LG"; o.quantity=20; o.status=OrderStatus::RESERVED;
    repo.save(o);
    ASSERT_TRUE(repo.remove("ORD-001"));
    ASSERT_EQ((int)repo.findAll().size(), 0);
    std::remove(p.c_str());
}

TEST(or_remove_nonexistent_returns_false) {
    auto p = tmpPath("or_rem_none.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    ASSERT_FALSE(repo.remove("GHOST"));
    std::remove(p.c_str());
}

TEST(or_persistence_all_statuses_reload) {
    auto p = tmpPath("or_reload.dat"); std::remove(p.c_str());
    {
        OrderRepository repo(p);
        auto mk = [](const std::string& id, OrderStatus st) {
            Order o; o.orderId=id; o.sampleId="S-001"; o.customerName="c"; o.quantity=10; o.status=st;
            return o;
        };
        repo.save(mk("O1", OrderStatus::RESERVED));
        repo.save(mk("O2", OrderStatus::PRODUCING));
        repo.save(mk("O3", OrderStatus::CONFIRMED));
        repo.save(mk("O4", OrderStatus::RELEASE));
        repo.save(mk("O5", OrderStatus::REJECTED));
    }
    OrderRepository repo2(p);
    ASSERT_EQ((int)repo2.findAll().size(), 5);
    ASSERT_EQ(repo2.findById("O3")->status, OrderStatus::CONFIRMED);
    ASSERT_EQ(repo2.findById("O5")->status, OrderStatus::REJECTED);
    std::remove(p.c_str());
}

TEST(or_exists_check) {
    auto p = tmpPath("or_exists.dat"); std::remove(p.c_str());
    OrderRepository repo(p);
    ASSERT_FALSE(repo.exists("ORD-001"));
    Order o; o.orderId="ORD-001"; o.sampleId="S-001"; o.customerName="c"; o.quantity=10; o.status=OrderStatus::RESERVED;
    repo.save(o);
    ASSERT_TRUE(repo.exists("ORD-001"));
    std::remove(p.c_str());
}
