#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN // Only define in one cpp file

#include "doctest.h"
#include "BoxSet.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN // Only define in one cpp file

#include "doctest.h"
#include "BoxSet.hpp"
#include <array>

TEST_CASE("BoxSet Initialization and Empty Check") {
    BoxSet<int, 8> box;
    CHECK(box.is_empty());
    CHECK(box.size() == 0);
    CHECK(box.capacity() == 8);

    std::array<int, 8> initial_data = {1,2,3,4,5,6,7,8};
    BoxSet<int, 8> box2{initial_data};
    CHECK(!box2.is_empty());  // Check is not empty now
    CHECK(box2.size() == 8); // Size should be 8
    CHECK(box2.capacity() == 8);
    CHECK(box2.is_full()); // Should also be full

}
TEST_CASE("BoxSet Add and Size Check") {
    BoxSet<int, 8> box;
    
    // Add some elements
    box.add(10);
    CHECK(!box.is_empty());
    CHECK(!box.is_full());
    CHECK(box.size() == 1);

    box.add(20);
    CHECK(box.size() == 2);

    box.add(30);
    CHECK(box.size() == 3);
}

TEST_CASE("BoxSet IsFull Check") {
    BoxSet<int, 8> box;
    
    // Check that it is not full at the beginning
    CHECK(!box.is_full());

    // Fill up
    box.add(10);
    box.add(20);
    box.add(30);
    box.add(40);
    box.add(50);
    box.add(60);
    box.add(70);
    box.add(80);
    
    // Check is full
    CHECK(box.is_full());
}

TEST_CASE("BoxSet Add and Remove Check") {
  
    BoxSet<int, 8> box;
    //add some items
    box.add(10);
    box.add(20);
    box.add(30);
    
    //check the correct size
    CHECK(box.size() == 3);
    
    // Remove an item
    int removed_item = box.remove(1);
    
    CHECK(removed_item == 20);
    CHECK(box.size() == 2);

    int removed_item2 = box.remove(0);
    CHECK(removed_item2 == 10);
    CHECK(box.size() == 1);

    int removed_item3 = box.remove(2);
    CHECK(removed_item3 == 30);
    CHECK(box.size() == 0);
}

TEST_CASE("BoxSet Add and IsUsed Check")
{
  BoxSet<int, 8> box; // Using N=8 now
    
    // Add some elements
    box.add(10);
    CHECK(box.is_used(0));
    CHECK(!box.is_used(1));
    CHECK(!box.is_used(2));
    CHECK(!box.is_used(3));
     CHECK(!box.is_used(4));
     CHECK(!box.is_used(5));
    CHECK(!box.is_used(6));
    CHECK(!box.is_used(7));

    box.add(20);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(!box.is_used(2));
    CHECK(!box.is_used(3));
      CHECK(!box.is_used(4));
     CHECK(!box.is_used(5));
    CHECK(!box.is_used(6));
     CHECK(!box.is_used(7));

    box.add(30);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(!box.is_used(3));
      CHECK(!box.is_used(4));
     CHECK(!box.is_used(5));
    CHECK(!box.is_used(6));
    CHECK(!box.is_used(7));
    
     box.add(40);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(box.is_used(3));
    CHECK(!box.is_used(4));
     CHECK(!box.is_used(5));
    CHECK(!box.is_used(6));
     CHECK(!box.is_used(7));
     
    box.add(50);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(box.is_used(3));
     CHECK(box.is_used(4));
     CHECK(!box.is_used(5));
    CHECK(!box.is_used(6));
     CHECK(!box.is_used(7));
    
      box.add(60);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(box.is_used(3));
    CHECK(box.is_used(4));
     CHECK(box.is_used(5));
     CHECK(!box.is_used(6));
    CHECK(!box.is_used(7));
     
      box.add(70);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(box.is_used(3));
    CHECK(box.is_used(4));
     CHECK(box.is_used(5));
    CHECK(box.is_used(6));
     CHECK(!box.is_used(7));
     
    box.add(80);
    CHECK(box.is_used(0));
    CHECK(box.is_used(1));
    CHECK(box.is_used(2));
    CHECK(box.is_used(3));
    CHECK(box.is_used(4));
     CHECK(box.is_used(5));
    CHECK(box.is_used(6));
     CHECK(box.is_used(7));
}

TEST_CASE("BoxSet Mixed Operations") {
    BoxSet<std::string, 8> box;

    // add and remove
    box.add("apple");
    box.add("banana");
    box.add("cherry");
    CHECK(box.size() == 3);

    box.remove(1); // remove banana
    CHECK(box.size() == 2);

    // Verify correct elements are still present
    CHECK(box.is_used(0));
    CHECK(!box.is_used(1));
    CHECK(box.is_used(2));
    
    //add more items and remove
    box.add("date");
    CHECK(box.size() == 3);
    
    box.remove(0);
     CHECK(box.size() == 2);
     
     box.remove(2);
      CHECK(box.size() == 1);

     box.remove(1);
      CHECK(box.size() == 0);
}