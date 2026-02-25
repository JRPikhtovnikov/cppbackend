#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace collision_detector {

// Вспомогательный класс для тестов, реализующий интерфейс ItemGathererProvider
class TestProvider : public ItemGathererProvider {
public:
    void AddItem(geom::Point2D pos, double width) {
        items_.push_back({pos, width});
    }
    void AddGatherer(geom::Point2D start, geom::Point2D end, double width) {
        gatherers_.push_back({start, end, width});
    }

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

// Проверка близости двух чисел с плавающей точкой
inline bool DoubleEqual(double a, double b, double eps = 1e-10) {
    return std::abs(a - b) < eps;
}

// Проверка, что событие имеет ожидаемые значения
void CheckEvent(const GatheringEvent& ev, size_t gatherer_id, size_t item_id,
                double sq_distance, double time) {
    REQUIRE(ev.gatherer_id == gatherer_id);
    REQUIRE(ev.item_id == item_id);
    REQUIRE(DoubleEqual(ev.sq_distance, sq_distance));
    REQUIRE(DoubleEqual(ev.time, time));
}

// Проверка, что вектор событий содержит все ожидаемые события (порядок не важен)
void CheckEventsUnordered(const std::vector<GatheringEvent>& actual,
                          const std::vector<GatheringEvent>& expected) {
    REQUIRE(actual.size() == expected.size());

    // Копируем, чтобы можно было удалять найденные элементы
    std::vector<GatheringEvent> remaining = expected;

    for (const auto& ev : actual) {
        auto it = std::find_if(remaining.begin(), remaining.end(),
            [&ev](const GatheringEvent& e) {
                return e.gatherer_id == ev.gatherer_id &&
                       e.item_id == ev.item_id &&
                       DoubleEqual(e.sq_distance, ev.sq_distance) &&
                       DoubleEqual(e.time, ev.time);
            });
        if (it == remaining.end()) {
            FAIL("Unexpected event: (" << ev.gatherer_id << "," << ev.item_id
                 << "," << ev.sq_distance << "," << ev.time << ")");
        }
        remaining.erase(it);
    }
    REQUIRE(remaining.empty());
}

// Проверка, что события идут в хронологическом порядке
void CheckOrder(const std::vector<GatheringEvent>& events) {
    for (size_t i = 1; i < events.size(); ++i) {
        REQUIRE(events[i-1].time <= events[i].time + 1e-10);
    }
}

}  // namespace collision_detector

using namespace collision_detector;

TEST_CASE("FindGatherEvents detects a simple collision") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {10,0}, 1.0);   // gatherer 0
    provider.AddItem({5,0}, 1.0);               // item 0

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CheckEvent(events[0], 0, 0, 0.0, 0.5);
    CheckOrder(events);
}

TEST_CASE("FindGatherEvents ignores item far from trajectory") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({5,3}, 0.5);   // distance = 3 > 1+0.5 = 1.5

    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents ignores item behind start") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({-1,0}, 1.0);   // proj_ratio < 0

    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents ignores item beyond end") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({11,0}, 1.0);   // proj_ratio > 1

    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents detects items at start and end") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({0,0}, 0.5);    // at start
    provider.AddItem({10,0}, 0.5);   // at end

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);

    // События могут идти в любом порядке (одновременных нет, т.к. времена 0 и 1)
    // Проверим, что оба присутствуют с правильными временами
    std::vector<GatheringEvent> expected = {
        {0, 0, 0.0, 0.0},
        {1, 0, 0.0, 1.0}
    };
    CheckEventsUnordered(events, expected);
    CheckOrder(events);
}

TEST_CASE("FindGatherEvents handles multiple gatherers and items") {
    TestProvider provider;
    // gatherer0: (0,0)->(10,0), width=1
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    // gatherer1: (0,2)->(10,2), width=1
    provider.AddGatherer({0,2}, {10,2}, 1.0);
    // item0 at (5,0) width=1
    provider.AddItem({5,0}, 1.0);
    // item1 at (5,2) width=1
    provider.AddItem({5,2}, 1.0);

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);

    // Оба события происходят в момент времени 0.5
    std::vector<GatheringEvent> expected = {
        {0, 0, 0.0, 0.5},
        {1, 1, 0.0, 0.5}
    };
    CheckEventsUnordered(events, expected);
    CheckOrder(events);   // времена равны, порядок может быть любым
}

TEST_CASE("FindGatherEvents respects exact distance threshold") {
    TestProvider provider;
    // gatherer radius = 1, item radius = 0.5, sum = 1.5
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({5, 1.5}, 0.5);   // distance = 1.5 -> should be collected

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CheckEvent(events[0], 0, 0, 1.5*1.5, 0.5);
}

TEST_CASE("FindGatherEvents ignores non-moving gatherer") {
    TestProvider provider;
    provider.AddGatherer({0,0}, {0,0}, 1.0);   // no movement
    provider.AddItem({0,0}, 0.5);

    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents computes correct squared distance") {
    TestProvider provider;
    // gatherer moves along x-axis, item at (5,2) with large width to ensure collection
    provider.AddGatherer({0,0}, {10,0}, 5.0);   // large width, will collect
    provider.AddItem({5,2}, 1.0);

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    // sq_distance should be 2^2 = 4
    CheckEvent(events[0], 0, 0, 4.0, 0.5);
}

TEST_CASE("FindGatherEvents returns events in chronological order") {
    TestProvider provider;
    // Один собиратель, несколько предметов вдоль пути с разными проекциями
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddItem({2,0}, 0.5);   // time 0.2
    provider.AddItem({5,0}, 0.5);   // time 0.5
    provider.AddItem({8,0}, 0.5);   // time 0.8

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    // Проверим, что времена идут по возрастанию
    for (size_t i = 1; i < events.size(); ++i) {
        REQUIRE(events[i-1].time <= events[i].time + 1e-10);
    }
    // Также проверим, что это именно те предметы
    CheckEvent(events[0], 0, 0, 0.0, 0.2);
    CheckEvent(events[1], 0, 1, 0.0, 0.5);
    CheckEvent(events[2], 0, 2, 0.0, 0.8);
}

TEST_CASE("FindGatherEvents collects same item by multiple gatherers") {
    TestProvider provider;
    // Два собирателя проходят через одну точку
    provider.AddGatherer({0,0}, {10,0}, 1.0);
    provider.AddGatherer({0,0}, {10,0}, 1.0);   // второй такой же
    provider.AddItem({5,0}, 0.5);

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    std::vector<GatheringEvent> expected = {
        {0, 0, 0.0, 0.5},
        {1, 0, 0.0, 0.5}
    };
    CheckEventsUnordered(events, expected);
}

TEST_CASE("FindGatherEvents no events when no gatherers or items") {
    TestProvider provider;
    auto events = FindGatherEvents(provider);
    REQUIRE(events.empty());

    provider.AddGatherer({0,0}, {10,0}, 1.0);
    events = FindGatherEvents(provider);
    REQUIRE(events.empty());

    provider = TestProvider{};
    provider.AddItem({5,0}, 0.5);
    events = FindGatherEvents(provider);
    REQUIRE(events.empty());
}