#include <catch2/catch.hpp>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "../src/collision_detector.h"

// Специализация StringMaker для красивого вывода GatheringEvent
namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << ","
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace collision_detector {

// Тестовый провайдер, реализующий интерфейс ItemGathererProvider
class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Gatherer> gatherers, std::vector<Item> items)
        : gatherers_(std::move(gatherers)), items_(std::move(items)) {}

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_.at(idx); }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_.at(idx); }

private:
    std::vector<Gatherer> gatherers_;
    std::vector<Item> items_;
};

// Вспомогательная функция для сравнения двух событий с учётом погрешности
bool EventsEqual(const GatheringEvent& lhs, const GatheringEvent& rhs, double eps = 1e-10) {
    return lhs.gatherer_id == rhs.gatherer_id &&
           lhs.item_id == rhs.item_id &&
           std::abs(lhs.sq_distance - rhs.sq_distance) <= eps &&
           std::abs(lhs.time - rhs.time) <= eps;
}

}  // namespace collision_detector

using namespace collision_detector;

TEST_CASE("FindGatherEvents: single gatherer, single item on path") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    Item i{{5, 0}, 0.2};
    TestProvider provider({g}, {i});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].gatherer_id == 0);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].time == Approx(0.5).margin(1e-10));
    CHECK(events[0].sq_distance == Approx(0.0).margin(1e-10));
}

TEST_CASE("FindGatherEvents: item too far from path") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    Item i{{5, 1}, 0.2};               // расстояние 1 > (0.6+0.2)/2 = 0.4
    TestProvider provider({g}, {i});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents: item projection outside segment") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};

    SECTION("left of start") {
        Item i{{-1, 0}, 0.2};
        TestProvider provider({g}, {i});
        CHECK(FindGatherEvents(provider).empty());
    }

    SECTION("right of end") {
        Item i{{11, 0}, 0.2};
        TestProvider provider({g}, {i});
        CHECK(FindGatherEvents(provider).empty());
    }
}

TEST_CASE("FindGatherEvents: item at start and end positions") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};

    SECTION("at start") {
        Item i{{0, 0}, 0.2};
        TestProvider provider({g}, {i});
        auto events = FindGatherEvents(provider);
        REQUIRE(events.size() == 1);
        CHECK(events[0].time == Approx(0.0).margin(1e-10));
    }

    SECTION("at end") {
        Item i{{10, 0}, 0.2};
        TestProvider provider({g}, {i});
        auto events = FindGatherEvents(provider);
        REQUIRE(events.size() == 1);
        CHECK(events[0].time == Approx(1.0).margin(1e-10));
    }
}

TEST_CASE("FindGatherEvents: multiple items on path") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    std::vector<Item> items = {
        {{2, 0}, 0.2},
        {{5, 0}, 0.2},
        {{8, 0}, 0.2}
    };
    TestProvider provider({g}, items);

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 3);
    CHECK(events[0].time == Approx(0.2).margin(1e-10));
    CHECK(events[1].time == Approx(0.5).margin(1e-10));
    CHECK(events[2].time == Approx(0.8).margin(1e-10));
    CHECK(events[0].item_id == 0);
    CHECK(events[1].item_id == 1);
    CHECK(events[2].item_id == 2);
}

TEST_CASE("FindGatherEvents: multiple gatherers and items") {
    Gatherer g1{{0, 0}, {10, 0}, 0.6};
    Gatherer g2{{0, 1}, {10, 1}, 0.6};
    Item i1{{5, 0}, 0.2};
    Item i2{{5, 1}, 0.2};
    Item i3{{5, 0.5}, 0.2};   // вне зоны сбора (расстояние 0.5 > 0.4)

    TestProvider provider({g1, g2}, {i1, i2, i3});
    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);

    // Ожидаемые события (порядок может быть любым из-за одинакового времени)
    std::vector<GatheringEvent> expected = {
        {0, 0, 0.0, 0.5},
        {1, 1, 0.0, 0.5}
    };

    // Сортируем для сравнения
    auto cmp = [](const GatheringEvent& a, const GatheringEvent& b) {
        if (a.time != b.time) return a.time < b.time;
        if (a.gatherer_id != b.gatherer_id) return a.gatherer_id < b.gatherer_id;
        return a.item_id < b.item_id;
    };
    std::sort(events.begin(), events.end(), cmp);
    std::sort(expected.begin(), expected.end(), cmp);

    for (size_t i = 0; i < events.size(); ++i) {
        CHECK(events[i].gatherer_id == expected[i].gatherer_id);
        CHECK(events[i].item_id == expected[i].item_id);
        CHECK(events[i].time == Approx(expected[i].time).margin(1e-10));
        CHECK(events[i].sq_distance == Approx(expected[i].sq_distance).margin(1e-10));
    }
}

TEST_CASE("FindGatherEvents: zero movement") {
    Gatherer g{{0, 0}, {0, 0}, 0.6};
    Item i{{0, 0}, 0.2};
    TestProvider provider({g}, {i});

    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("FindGatherEvents: item at collection radius boundary") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    double collect_radius = (g.width + 0.2) / 2.0; // 0.4
    Item i{{5, collect_radius}, 0.2};             // точно на границе

    TestProvider provider({g}, {i});
    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].time == Approx(0.5).margin(1e-10));
    CHECK(events[0].sq_distance == Approx(collect_radius * collect_radius).margin(1e-10));
}

TEST_CASE("FindGatherEvents: no false positives near boundary") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    double collect_radius = (g.width + 0.2) / 2.0; // 0.4
    const double eps = 1e-5;

    SECTION("slightly beyond radius") {
        Item i{{5, collect_radius + eps}, 0.2};
        TestProvider provider({g}, {i});
        CHECK(FindGatherEvents(provider).empty());
    }

    SECTION("projection slightly before start") {
        Item i{{-eps, 0}, 0.2};
        TestProvider provider({g}, {i});
        CHECK(FindGatherEvents(provider).empty());
    }

    SECTION("projection slightly after end") {
        Item i{{10 + eps, 0}, 0.2};
        TestProvider provider({g}, {i});
        CHECK(FindGatherEvents(provider).empty());
    }
}

TEST_CASE("FindGatherEvents: item with large width") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};   // радиус 0.3
    Item i{{5, 1.0}, 1.6};              // радиус 0.8, сумма радиусов 1.1 > 1.0

    TestProvider provider({g}, {i});
    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].time == Approx(0.5).margin(1e-10));
    CHECK(events[0].sq_distance == Approx(1.0 * 1.0).margin(1e-10));
}

TEST_CASE("FindGatherEvents: diagonal movement") {
    Gatherer g{{0, 0}, {10, 10}, 0.6};

    SECTION("item exactly on line") {
        Item i{{5, 5}, 0.2};
        TestProvider provider({g}, {i});
        auto events = FindGatherEvents(provider);
        REQUIRE(events.size() == 1);
        CHECK(events[0].time == Approx(0.5).margin(1e-10));
        CHECK(events[0].sq_distance == Approx(0.0).margin(1e-10));
    }

    SECTION("item at maximum allowed distance") {
        // перпендикулярное смещение на 0.4 (радиус сбора)
        double shift = 0.4 / std::sqrt(2.0); // 0.4/√2 ≈ 0.2828427
        Item i{{5 + shift, 5 - shift}, 0.2};
        TestProvider provider({g}, {i});
        auto events = FindGatherEvents(provider);
        REQUIRE(events.size() == 1);
        CHECK(events[0].time == Approx(0.5).margin(1e-6));
        CHECK(events[0].sq_distance == Approx(0.4 * 0.4).margin(1e-6));
    }
}

TEST_CASE("FindGatherEvents: events in chronological order") {
    Gatherer g{{0, 0}, {10, 0}, 0.6};
    std::vector<Item> items = {
        {{9, 0}, 0.2},   // время 0.9
        {{1, 0}, 0.2},   // время 0.1
        {{5, 0}, 0.2}    // время 0.5
    };
    TestProvider provider({g}, items);

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 3);
    CHECK(events[0].time == Approx(0.1).margin(1e-10));
    CHECK(events[1].time == Approx(0.5).margin(1e-10));
    CHECK(events[2].time == Approx(0.9).margin(1e-10));
}