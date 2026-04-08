/*
 * test_list_utility.cc - Unit tests for list_utility.hh
 * Tests count caching and other list operations
 */

#include <catch2/catch_test_macros.hpp>
#include "src/core/list_utility.hh"

// Test node for SList (single linked)
struct SNode {
    int value;
    SNode *next{nullptr};
    
    explicit SNode(int v) : value(v) {}
};

// Test node for DList (double linked)
struct DNode {
    int value;
    DNode *fore{nullptr};
    DNode *next{nullptr};
    
    explicit DNode(int v) : value(v) {}
};

TEST_CASE("SList count caching", "[list_utility][slist]") {
    SList<SNode> list;
    
    SECTION("Empty list has count 0") {
        REQUIRE(list.Count() == 0);
        REQUIRE(list.IsEmpty() == true);
    }
    
    SECTION("AddToHead increments count") {
        list.AddToHead(new SNode(1));
        REQUIRE(list.Count() == 1);
        
        list.AddToHead(new SNode(2));
        REQUIRE(list.Count() == 2);
        
        list.AddToHead(new SNode(3));
        REQUIRE(list.Count() == 3);
    }
    
    SECTION("AddToTail increments count") {
        list.AddToTail(new SNode(1));
        REQUIRE(list.Count() == 1);
        
        list.AddToTail(new SNode(2));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("AddAfterNode increments count") {
        auto* first = new SNode(1);
        list.AddToHead(first);
        
        list.AddAfterNode(first, new SNode(2));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("Remove decrements count") {
        auto* n1 = new SNode(1);
        auto* n2 = new SNode(2);
        auto* n3 = new SNode(3);
        
        list.AddToTail(n1);
        list.AddToTail(n2);
        list.AddToTail(n3);
        REQUIRE(list.Count() == 3);
        
        list.Remove(n2);
        delete n2;
        REQUIRE(list.Count() == 2);
        
        list.Remove(n1);
        delete n1;
        REQUIRE(list.Count() == 1);
        
        list.Remove(n3);
        delete n3;
        REQUIRE(list.Count() == 0);
    }
    
    SECTION("RemoveAndDelete decrements count") {
        list.AddToTail(new SNode(1));
        list.AddToTail(new SNode(2));
        REQUIRE(list.Count() == 2);
        
        list.RemoveAndDelete(list.Head());
        REQUIRE(list.Count() == 1);
    }
    
    SECTION("Purge resets count to 0") {
        list.AddToTail(new SNode(1));
        list.AddToTail(new SNode(2));
        list.AddToTail(new SNode(3));
        REQUIRE(list.Count() == 3);
        
        list.Purge();
        REQUIRE(list.Count() == 0);
        REQUIRE(list.IsEmpty() == true);
    }
    
    SECTION("Move constructor transfers count") {
        list.AddToTail(new SNode(1));
        list.AddToTail(new SNode(2));
        REQUIRE(list.Count() == 2);
        
        SList<SNode> moved(std::move(list));
        REQUIRE(moved.Count() == 2);
        REQUIRE(list.Count() == 0);  // NOLINT - intentionally checking moved-from state
    }
}

TEST_CASE("DList count caching", "[list_utility][dlist]") {
    DList<DNode> list;
    
    SECTION("Empty list has count 0") {
        REQUIRE(list.Count() == 0);
        REQUIRE(list.IsEmpty() == true);
    }
    
    SECTION("AddToHead increments count") {
        list.AddToHead(new DNode(1));
        REQUIRE(list.Count() == 1);
        
        list.AddToHead(new DNode(2));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("AddToTail increments count") {
        list.AddToTail(new DNode(1));
        REQUIRE(list.Count() == 1);
        
        list.AddToTail(new DNode(2));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("AddAfterNode increments count") {
        auto* first = new DNode(1);
        list.AddToHead(first);
        
        list.AddAfterNode(first, new DNode(2));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("AddBeforeNode increments count") {
        auto* last = new DNode(2);
        list.AddToTail(last);
        
        list.AddBeforeNode(last, new DNode(1));
        REQUIRE(list.Count() == 2);
    }
    
    SECTION("Remove decrements count") {
        auto* n1 = new DNode(1);
        auto* n2 = new DNode(2);
        auto* n3 = new DNode(3);
        
        list.AddToTail(n1);
        list.AddToTail(n2);
        list.AddToTail(n3);
        REQUIRE(list.Count() == 3);
        
        list.Remove(n2);
        delete n2;
        REQUIRE(list.Count() == 2);
        
        // Verify list integrity
        REQUIRE(n1->next == n3);
        REQUIRE(n3->fore == n1);
    }
    
    SECTION("RemoveAndDelete decrements count") {
        list.AddToTail(new DNode(1));
        list.AddToTail(new DNode(2));
        REQUIRE(list.Count() == 2);
        
        list.RemoveAndDelete(list.Head());
        REQUIRE(list.Count() == 1);
    }
    
    SECTION("Purge resets count to 0") {
        list.AddToTail(new DNode(1));
        list.AddToTail(new DNode(2));
        list.AddToTail(new DNode(3));
        REQUIRE(list.Count() == 3);
        
        list.Purge();
        REQUIRE(list.Count() == 0);
        REQUIRE(list.IsEmpty() == true);
    }
    
    SECTION("Sort preserves count") {
        list.AddToTail(new DNode(3));
        list.AddToTail(new DNode(1));
        list.AddToTail(new DNode(2));
        REQUIRE(list.Count() == 3);
        
        list.Sort([](DNode* a, DNode* b) { return a->value - b->value; });
        REQUIRE(list.Count() == 3);
        
        // Verify sorted order
        REQUIRE(list.Head()->value == 1);
        REQUIRE(list.Tail()->value == 3);
    }
    
    SECTION("Move constructor transfers count") {
        list.AddToTail(new DNode(1));
        list.AddToTail(new DNode(2));
        REQUIRE(list.Count() == 2);
        
        DList<DNode> moved(std::move(list));
        REQUIRE(moved.Count() == 2);
        REQUIRE(list.Count() == 0);  // NOLINT - intentionally checking moved-from state
    }
}

TEST_CASE("Count caching performance benefit", "[list_utility][performance]") {
    DList<DNode> list;
    
    // Add 1000 items
    for (int i = 0; i < 1000; ++i) {
        list.AddToTail(new DNode(i));
    }
    
    // Count() should now be O(1) - calling it many times should be fast
    int total = 0;
    for (int i = 0; i < 10000; ++i) {
        total += list.Count();  // Would be O(n*10000) without caching
    }
    
    REQUIRE(total == 10000 * 1000);
    REQUIRE(list.Count() == 1000);
}
