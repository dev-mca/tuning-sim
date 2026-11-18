// Lock-free SPSC queue and triple buffer: single-threaded semantics plus a threaded smoke test.

#include "concurrency/spsc_queue.hpp"
#include "concurrency/triple_buffer.hpp"

#include <doctest/doctest.h>

#include <thread>

using namespace simcore;

TEST_CASE("SPSC queue is first in, first out and reports full and empty") {
    SpscQueue<int, 4> queue; // capacity 4 = 3 usable slots
    CHECK_FALSE(queue.pop().has_value());
    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK_FALSE(queue.push(4)); // full
    CHECK(queue.pop() == 1);
    CHECK(queue.push(4)); // wraps around
    CHECK(queue.pop() == 2);
    CHECK(queue.pop() == 3);
    CHECK(queue.pop() == 4);
    CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("SPSC queue delivers every value in order across threads") {
    constexpr int kCount = 200'000;
    SpscQueue<int, 256> queue;
    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });
    int expected = 0;
    bool inOrder = true;
    while (expected < kCount) {
        if (const auto value = queue.pop()) {
            inOrder = inOrder && (*value == expected);
            ++expected;
        }
    }
    producer.join();
    CHECK(inOrder);
}

TEST_CASE("triple buffer returns the newest value once") {
    TripleBuffer<int> buffer;
    int value = -1;
    CHECK_FALSE(buffer.read(value));
    buffer.write(1);
    buffer.write(2);
    CHECK(buffer.read(value));
    CHECK(value == 2); // intermediate values may be skipped
    CHECK_FALSE(buffer.read(value));
    buffer.write(3);
    CHECK(buffer.read(value));
    CHECK(value == 3);
}

TEST_CASE("triple buffer never hands out a torn value across threads") {
    struct Pair {
        long a = 0;
        long b = 0; // always == -a when written
    };
    constexpr long kCount = 200'000;
    TripleBuffer<Pair> buffer;
    std::thread writer([&] {
        for (long i = 1; i <= kCount; ++i) {
            buffer.write({i, -i});
        }
    });
    bool consistent = true;
    long last = 0;
    Pair pair;
    while (last < kCount) {
        if (buffer.read(pair)) {
            consistent = consistent && (pair.b == -pair.a) && (pair.a >= last);
            last = pair.a;
        }
    }
    writer.join();
    CHECK(consistent);
}
