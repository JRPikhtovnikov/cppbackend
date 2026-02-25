#include "loot_generator.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace loot_gen {

unsigned LootGenerator::Generate(TimeInterval time_delta, unsigned loot_count,
                                 unsigned looter_count) {
    
    time_without_loot_ += time_delta;
    const unsigned loot_shortage = loot_count > looter_count ? 0u : looter_count - loot_count;
    if (base_interval_.count() == 0) {
        return 0;
    }
    
    const double ratio = std::chrono::duration<double>{time_without_loot_} / base_interval_;
    
    if (!random_generator_) {
        return 0;
    }
    
    double rand_val;
    try {
        rand_val = random_generator_();
    } catch (const std::exception& e) {
        return 0;
    } catch (...) {
        return 0;
    }
    
    const double probability = std::clamp((1.0 - std::pow(1.0 - probability_, ratio)) * rand_val, 0.0, 1.0);    
    const unsigned generated_loot = static_cast<unsigned>(std::round(loot_shortage * probability));
    
    if (generated_loot > 0) {
        time_without_loot_ = {};
    }
    
    return generated_loot;
}

} // namespace loot_gen
