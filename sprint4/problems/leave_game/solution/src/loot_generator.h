#pragma once
#include <chrono>
#include <functional>

namespace loot_gen {

class LootGenerator {
public:
    using RandomGenerator = std::function<double()>;
    using TimeInterval = std::chrono::milliseconds;

    LootGenerator(TimeInterval base_interval, double probability,
                  RandomGenerator random_gen = DefaultGenerator)
        : base_interval_{base_interval}
        , probability_{probability}
        , random_generator_{std::move(random_gen)} {
    }

    LootGenerator(LootGenerator&& other) noexcept
        : base_interval_(other.base_interval_)
        , probability_(other.probability_)
        , time_without_loot_(other.time_without_loot_)
        , random_generator_(std::move(other.random_generator_)) {
    }

    LootGenerator& operator=(LootGenerator&& other) noexcept {
        if (this != &other) {
            base_interval_ = other.base_interval_;
            probability_ = other.probability_;
            time_without_loot_ = other.time_without_loot_;
            random_generator_ = std::move(other.random_generator_);
        }
        return *this;
    }

    LootGenerator(const LootGenerator&) = delete;
    LootGenerator& operator=(const LootGenerator&) = delete;

    unsigned Generate(TimeInterval time_delta, unsigned loot_count, unsigned looter_count);

private:
    static double DefaultGenerator() noexcept {
        return 1.0;
    };
    TimeInterval base_interval_;
    double probability_;
    TimeInterval time_without_loot_{};
    RandomGenerator random_generator_;
};

}  // namespace loot_gen
