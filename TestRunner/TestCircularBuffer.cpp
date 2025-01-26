#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "CircularBuffer.hpp"

TEST_CASE("CircularBuffer Functionality") {

	SUBCASE("Capacity and Size") {
		CircularBuffer<int, 5> cbf;
		CHECK(cbf.capacity() == 5);
		CHECK(cbf.size() == 0);
	}

	SUBCASE("Empty and Full") {
		CircularBuffer<int, 3> cbf;
		CHECK(cbf.empty());
		CHECK(!cbf.full());

		cbf.push(1);
		CHECK(!cbf.empty());
		CHECK(!cbf.full());

		cbf.push(2);
		CHECK(!cbf.empty());
		CHECK(!cbf.full());

		cbf.push(3);
		CHECK(!cbf.empty());
		CHECK(cbf.full());
	}

	SUBCASE("Push and Pop with Copy") {
		CircularBuffer<int, 5> cbf;

		cbf.push(10);
		cbf.push(20);

		CHECK(cbf.size() == 2);

		int val1 = cbf.pop();
		CHECK(val1 == 10);
		CHECK(cbf.size() == 1);

		int val2 = cbf.pop();
		CHECK(val2 == 20);
		CHECK(cbf.size() == 0);
		CHECK(cbf.empty());

	}

	SUBCASE("Push and Pop with Move") {
		CircularBuffer<std::string, 5> cbf;

		std::string str1 = "Hello";
		std::string str2 = "World";

		cbf.push(std::move(str1));
		cbf.push(std::move(str2));

		CHECK(cbf.size() == 2);
		CHECK(str1.empty());//ensure the move has taken place
		CHECK(str2.empty());//ensure the move has taken place
		std::string popped1 = cbf.pop();
		CHECK(popped1 == "Hello");
		CHECK(cbf.size() == 1);

		std::string popped2 = cbf.pop();
		CHECK(popped2 == "World");
		CHECK(cbf.size() == 0);
		CHECK(cbf.empty());
	}

	SUBCASE("Multiple Push, Peek, and Pop") {
		CircularBuffer<int, 4> cbf;

		cbf.push(1);
		cbf.push(2);
		cbf.push(3);
		CHECK(cbf.size() == 3);
		
		CHECK(cbf.peek() == 1);
		CHECK(cbf.pop() == 1);
		CHECK(cbf.size() == 2);
		
		CHECK(cbf.peek() == 2);
		CHECK(cbf.pop() == 2);
		CHECK(cbf.size() == 1);
		
		cbf.push(4);
		CHECK(cbf.size() == 2);
		CHECK(cbf.peek() == 3);
		CHECK(cbf.pop() == 3);
		
		CHECK(cbf.size() == 1);
		CHECK(cbf.peek() == 4);
		CHECK(cbf.pop() == 4);

		CHECK(cbf.size() == 0);
		CHECK(cbf.empty());

	}

	SUBCASE("Unsafe Overflow behavior") {
		CircularBuffer<int, 3> cbf;

		cbf.push(1);
		cbf.push(2);
		cbf.push(3);
		CHECK(cbf.full());
		// No exception
		cbf.push(4);  // Overwrites the first element (1)
		CHECK(cbf.size() != 3);


		CHECK(cbf.pop() != 2);
		CHECK(cbf.pop() != 3);
		CHECK(cbf.pop() != 4);
	}

	SUBCASE("Unsafe Underflow behavior") {
		CircularBuffer<int, 3> cbf;

		//pop when empty is undefined behavior
		// No exception
		cbf.push(1);
		cbf.pop();
		CHECK(cbf.empty());
		cbf.pop();// Undefined behavior, but we show it will return garbage.
	}
}
