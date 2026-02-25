#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <sstream>

using namespace collision_detector;
using namespace Catch::Matchers;

// Специализация StringMaker для печати GatheringEvent
namespace Catch {
template <>
struct StringMaker<GatheringEvent> {
    static std::string convert(GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << ","
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

// Вспомогательный класс для тестов, реализующий ItemGathererProvider
class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_.at(idx); }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_.at(idx); }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

// Вспомогательная функция для сравнения двух событий с учётом погрешности
bool EventsAlmostEqual(const GatheringEvent& a, const GatheringEvent& b, double eps = 1e-10) {
    return a.gatherer_id == b.gatherer_id &&
           a.item_id == b.item_id &&
           std::abs(a.sq_distance - b.sq_distance) < eps &&
           std::abs(a.time - b.time) < eps;
}

// Матчер для сравнения векторов событий с плавающей точкой
class GatheringEventMatcher : public Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
public:
    GatheringEventMatcher(std::vector<GatheringEvent> expected, double eps = 1e-10)
        : expected_(std::move(expected)), eps_(eps) {}

    bool match(const std::vector<GatheringEvent>& actual) const override {
        if (actual.size() != expected_.size()) return false;
        for (size_t i = 0; i < actual.size(); ++i) {
            if (!EventsAlmostEqual(actual[i], expected_[i], eps_)) return false;
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "Equals: [";
        for (const auto& e : expected_) {
            // Manually format each event to avoid requiring operator<<
            ss << "(" << e.gatherer_id << "," << e.item_id << ","
               << e.sq_distance << "," << e.time << ") ";
        }
        ss << "]";
        return ss.str();
    }

private:
    std::vector<GatheringEvent> expected_;
    double eps_;
};

// Тесты
TEST_CASE("FindGatherEvents handles empty provider", "[gather]") {
    TestProvider provider({}, {});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents detects a single collection event", "[gather]") {
    // Собиратель движется из (0,0) в (10,0), ширина 1
    Gatherer g{{0, 0}, {10, 0}, 1.0};
    // Предмет в центре пути на расстоянии 5, ширина 1 (радиус 0.5)
    Item i{{5, 0}, 1.0};
    TestProvider provider({i}, {g});

    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);

    // Ожидаем событие: gatherer_id=0, item_id=0
    // Расстояние от предмета до линии = 0, sq_distance = 0
    // Время: proj_ratio = (5*10)/(10*10) = 0.5 (т.к. u=(5,0), v=(10,0), u_dot_v=50, v_len2=100)
    GatheringEvent expected{0, 0, 0.0, 0.5};
    CHECK_THAT(events, GatheringEventMatcher({expected}));
}

TEST_CASE("FindGatherEvents does not detect when item is too far sideways", "[gather]") {
    // Собиратель движется горизонтально, предмет сбоку на расстоянии > суммы радиусов
    Gatherer g{{0, 0}, {10, 0}, 1.0}; // радиус 0.5
    Item i{{5, 2}, 1.0}; // расстояние по вертикали 2, сумма радиусов 1 -> не пересекается
    TestProvider provider({i}, {g});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents does not detect when item is before start", "[gather]") {
    Gatherer g{{0, 0}, {10, 0}, 1.0};
    Item i{{-5, 0}, 1.0}; // предмет слева от начала
    TestProvider provider({i}, {g});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents does not detect when item is after end", "[gather]") {
    Gatherer g{{0, 0}, {10, 0}, 1.0};
    Item i{{15, 0}, 1.0}; // предмет справа от конца
    TestProvider provider({i}, {g});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents handles multiple items and gatherers", "[gather]") {
    // Два собирателя и три предмета
    Gatherer g1{{0, 0}, {10, 0}, 1.0};
    Gatherer g2{{0, 10}, {0, 0}, 1.0}; // движется вниз
    Item i1{{5, 0}, 1.0};   // пересекается с g1 в момент 0.5
    Item i2{{0, 5}, 1.0};   // пересекается с g2 в момент 0.5 (g2 движется сверху вниз)
    Item i3{{5, 5}, 1.0};   // не пересекается ни с кем

    TestProvider provider({i1, i2, i3}, {g1, g2});
    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);
    // Ожидаем два события: (gatherer=0, item=0, time=0.5) и (gatherer=1, item=1, time=0.5)
    // Порядок может быть любым, так как время одинаково.
    std::vector<GatheringEvent> expected1{{0, 0, 0.0, 0.5}, {1, 1, 0.0, 0.5}};
    std::vector<GatheringEvent> expected2{{1, 1, 0.0, 0.5}, {0, 0, 0.0, 0.5}};

    bool matchOrder1 = std::is_permutation(events.begin(), events.end(),
                                           expected1.begin(), expected1.end(),
                                           [](const auto& a, const auto& b) {
                                               return a.gatherer_id == b.gatherer_id &&
                                                      a.item_id == b.item_id &&
                                                      std::abs(a.sq_distance - b.sq_distance) < 1e-10 &&
                                                      std::abs(a.time - b.time) < 1e-10;
                                           });
    bool matchOrder2 = std::is_permutation(events.begin(), events.end(),
                                           expected2.begin(), expected2.end(),
                                           [](const auto& a, const auto& b) {
                                               return a.gatherer_id == b.gatherer_id &&
                                                      a.item_id == b.item_id &&
                                                      std::abs(a.sq_distance - b.sq_distance) < 1e-10 &&
                                                      std::abs(a.time - b.time) < 1e-10;
                                           });
    CHECK((matchOrder1 || matchOrder2));
}

TEST_CASE("FindGatherEvents orders events by time", "[gather]") {
    // Один собиратель, несколько предметов на разных расстояниях
    Gatherer g{{0, 0}, {10, 0}, 1.0};
    Item i1{{2, 0}, 1.0}; // time = 0.2
    Item i2{{8, 0}, 1.0}; // time = 0.8
    Item i3{{5, 0}, 1.0}; // time = 0.5

    TestProvider provider({i1, i2, i3}, {g});
    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 3);
    // Проверяем, что времена упорядочены по возрастанию
    CHECK(events[0].time == 0.2);
    CHECK(events[1].time == 0.5);
    CHECK(events[2].time == 0.8);
}

TEST_CASE("FindGatherEvents respects zero movement (no events)", "[gather]") {
    Gatherer g{{0, 0}, {0, 0}, 1.0}; // собиратель не двигается
    Item i{{5, 0}, 1.0};
    TestProvider provider({i}, {g});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents boundary cases: item exactly at edge of collection radius", "[gather]") {
    // Собиратель движется горизонтально, предмет на расстоянии, равном сумме радиусов (граница)
    Gatherer g{{0, 0}, {10, 0}, 1.0}; // радиус 0.5
    // Радиус предмета тоже 0.5, сумма = 1.0
    // Предмет на (5, 1) – расстояние до линии = 1.0, должно быть пересечение (<=1)
    Item i1{{5, 1}, 1.0};
    TestProvider provider1({i1}, {g});
    auto events1 = FindGatherEvents(provider1);
    CHECK(events1.size() == 1);

    // Предмет на (5, 1.0001) – расстояние чуть больше 1, не должно пересекаться
    Item i2{{5, 1.0001}, 1.0};
    TestProvider provider2({i2}, {g});
    auto events2 = FindGatherEvents(provider2);
    CHECK(events2.empty());
}

TEST_CASE("FindGatherEvents handles item exactly at start or end point", "[gather]") {
    Gatherer g{{0, 0}, {10, 0}, 1.0};
    // Предмет в начальной точке (должен быть пойман, так как proj_ratio=0, расстояние 0)
    Item i_start{{0, 0}, 1.0};
    TestProvider provider_start({i_start}, {g});
    auto events_start = FindGatherEvents(provider_start);
    REQUIRE(events_start.size() == 1);
    CHECK(events_start[0].time == 0.0);

    // Предмет в конечной точке (proj_ratio=1)
    Item i_end{{10, 0}, 1.0};
    TestProvider provider_end({i_end}, {g});
    auto events_end = FindGatherEvents(provider_end);
    REQUIRE(events_end.size() == 1);
    CHECK(events_end[0].time == 1.0);
}

TEST_CASE("FindGatherEvents calculates sq_distance correctly", "[gather]") {
    // Собиратель движется по диагонали, предмет смещён
    Gatherer g{{0, 0}, {10, 10}, 1.0}; // вектор (10,10)
    Item i{{5, 6}, 1.0}; // расстояние до прямой?
    // Вектор u = (5,6), v = (10,10)
    // u_dot_v = 5*10 + 6*10 = 110, v_len2 = 200, proj_ratio = 110/200 = 0.55
    // sq_distance = u_len2 - (u_dot_v)^2/v_len2 = (25+36)=61 - 110*110/200 =61 - 12100/200 =61 - 60.5 = 0.5
    // Ожидаем sq_distance = 0.5
    TestProvider provider({i}, {g});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == 0.5);
    CHECK(events[0].time == 0.55);
}