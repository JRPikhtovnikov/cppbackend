#include <catch2/catch_test_macros.hpp>
#include "../src/model.h"

using namespace model;

TEST_CASE("Player bag functionality", "[player]") {
    Player player(1, "TestPlayer", 0, {0.0, 0.0});
    
    SECTION("Player starts with empty bag and zero score") {
        CHECK(player.GetBag().empty());
        CHECK(player.GetScore() == 0);
        CHECK(player.GetBagCapacity() == 0);
    }
    
    SECTION("Setting bag capacity works") {
        player.SetBagCapacity(3);
        CHECK(player.GetBagCapacity() == 3);
    }
    
    SECTION("Adding loot to bag") {
        player.SetBagCapacity(2);
        
        CHECK(player.AddLoot(1, 0, 10) == true); 
        CHECK(player.GetBag().size() == 1);
        CHECK(player.GetBag()[0].id == 1);
        CHECK(player.GetBag()[0].type == 0);
        CHECK(player.GetBag()[0].value == 10);
        
        CHECK(player.AddLoot(2, 1, 20) == true);
        CHECK(player.GetBag().size() == 2);
        
        CHECK(player.AddLoot(3, 2, 30) == false);
        CHECK(player.GetBag().size() == 2);
    }
    
    SECTION("Clearing bag") {
        player.SetBagCapacity(3);
        player.AddLoot(1, 0, 10);
        player.AddLoot(2, 1, 20);
        CHECK(player.GetBag().size() == 2);
        
        player.ClearBag();
        CHECK(player.GetBag().empty());
    }
    
    SECTION("Score accumulation") {
        CHECK(player.GetScore() == 0);
        
        player.AddScore(50);
        CHECK(player.GetScore() == 50);
        
        player.AddScore(25);
        CHECK(player.GetScore() == 75);
    }
}