#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "ArrayList.hpp"
#include <string>

TEST_CASE("ArrayList Construction and Basic Operations") {
	ArrayList<int, 5> list;
	CHECK(list.empty());
	CHECK(list.size() == 0);
	CHECK(list.capacity() == 5);
	CHECK(!list.full());

	list.push(10);
	CHECK(!list.empty());
	CHECK(list.size() == 1);
	CHECK(list[0] == 10);

	list.push(20);
	CHECK(list.size() == 2);
	CHECK(list[1] == 20);

	list.push(30);
	list.push(40);
	list.push(50);
	CHECK(list.full());

	const ArrayList<int, 5> constList = list;
	CHECK(constList[0] == 10);
	CHECK(constList.size() == 5);
	CHECK(constList.full());
}

TEST_CASE("ArrayList Remove Operation"){
	ArrayList<int, 5> list;
	list.push(10);
	list.push(20);
	list.push(30);
	list.push(40);
	list.push(50);

	// remove from middle
	list.remove(1);
	CHECK(list.size() == 4);
	CHECK(list[0] == 10);
	CHECK(list[1] == 30);
	CHECK(list[2] == 40);
	CHECK(list[3] == 50);

	// remove last
	list.remove(3);
	CHECK(list.size() == 3);
	CHECK(list[0] == 10);
	CHECK(list[1] == 30);
	CHECK(list[2] == 40);

	// remove first
	list.remove(0);
	CHECK(list.size() == 2);
	CHECK(list[0] == 30);
	CHECK(list[1] == 40);

	// fail quiet remove
	list.remove(10);
	CHECK(list.size() == 2);

	// remove all elements
	list.remove(0);
	list.remove(0);
	CHECK(list.size() == 0);
	CHECK(list.empty());
}

TEST_CASE("ArrayList Iterator") {
	ArrayList<int, 5> list;
	list.push(10);
	list.push(20);
	list.push(30);

	auto it = list.begin();
	CHECK(*it == 10);
	++it;
	CHECK(*it == 20);
	it++;
	CHECK(*it == 30);
	++it;
	CHECK(it == list.end());

	int sum = 0;
	for (auto i : list){
		sum+= i;
	}
	CHECK(sum == 60);

	const ArrayList<int,5> constList = list;
	int constSum = 0;
	for (auto i : constList){
		constSum+= i;
	}
	CHECK(constSum == 60);

	// check pre and post decrement
	auto it2 = list.end();
	--it2;
	CHECK(*it2 == 30);
	it2--;
	CHECK(*it2 == 20);

	// check + and - operation
	auto it3 = list.begin();
	CHECK(*(it3+2) == 30);
	CHECK(*(it3- (-1)) == 20);

	auto it4 = it3 + 2;
	CHECK(it4 - it3 == 2);

	//check []
			 CHECK(it3[1] == 20);

			 //check comparison
			 auto beginIt = list.begin();
			 auto endIt = list.end();
			 CHECK(beginIt < endIt);
			 CHECK(beginIt <= endIt);
			 CHECK(endIt > beginIt);
			 CHECK(endIt >= beginIt);
			 CHECK(beginIt == list.begin());
			 CHECK(beginIt != list.end());

}

TEST_CASE("ArrayList Out of Bounds access"){
	ArrayList<int,5> list;
	CHECK(list[0] == 0); // should return a default constructed int
	list.push(10);
	CHECK(list[10] == 0); // should return a default constructed int
	CHECK(list[0] == 10);

	const ArrayList<int,5> constList = list;
	CHECK(constList[0] == 10);
	CHECK(constList[10] == 0);
}


TEST_CASE("ArrayList pushOrReplace operation"){
	ArrayList<int, 5> list;
	list.push(1);
	list.push(2);
	list.push(3);

	// Add 4 or replace if it exists
	list.pushOrReplace(4, [](int a, int b) {return a == b;});

	list.pushOrReplace(4, [](int a, int b) {return a == b;});

	CHECK(list[0] == 1);
	CHECK(list[1] == 2);
	CHECK(list[2] == 3);
	CHECK(list[3] == 4);

	//Replace 4 if it exists
	list.pushOrReplace(5, [](int a, int b) {return a == 4;});

	CHECK(list[0] == 1);
	CHECK(list[1] == 2);
	CHECK(list[2] == 3);
	CHECK(list[3] == 5);

	ArrayList<std::string, 3> names;
	names.push("Alice");
	names.push("Bob");

	// Replace "Alice" if exists with "Alice"
	names.pushOrReplace("Alice", [](std::string a, std::string b) { return a == b;});
	CHECK(names[0] == "Alice");
	CHECK(names[1] == "Bob");

	// Replace "Bob" if exists with "Charlie"
	names.pushOrReplace("Charlie", [](std::string a, std::string b) { return a == "Bob";});

	CHECK(names[0] == "Alice");
	CHECK(names[1] == "Charlie");

	//Add "David"
	names.pushOrReplace("David", [](std::string a, std::string b) { return a == b;});
	CHECK(names[0] == "Alice");
	CHECK(names[1] == "Charlie");
	CHECK(names[2] == "David");
}

TEST_CASE("ArrayList Find Operation") {
    ArrayList<int, 5> list;
    list.push(10);
    list.push(20);
    list.push(30);

    // Find an existing element
    size_t index1 = list.find(20, [](int a, int b) { return a == b; });
    CHECK(index1 == 1);
    CHECK(list[index1] == 20);

    // Find a non-existing element (should add it)
    size_t index2 = list.find(40, [](int a, int b) { return a == b; });
	CHECK(index2 == 3);
    CHECK(list[index2] == 40);
	CHECK(list.size() == 4);

	//Find an existing element after adding a new one
    size_t index3 = list.find(10, [](int a, int b) { return a == b; });
	CHECK(index3 == 0);
	CHECK(list[index3] == 10);
	CHECK(list.size() == 4);

	ArrayList<std::string, 4> names;
	names.push("Alice");
	names.push("Bob");
	names.push("Charlie");
	// Find an existing string
	size_t index4 = names.find("Bob", [](const std::string& a, const std::string& b) { return a == b; });
	CHECK(index4 == 1);
	CHECK(names[index4] == "Bob");
	// Find a non existing string (should add it)
	size_t index5 = names.find("David", [](const std::string& a, const std::string& b) { return a == b; });
	CHECK(index5 == 3);
	CHECK(names[index5] == "David");
	CHECK(names.size() == 4);
}
