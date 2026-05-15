#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "dpc/util/queue.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <set>

using namespace dpc;

// ---------------------------------------------------------------------------
// single-threaded basics
// ---------------------------------------------------------------------------

TEST_CASE("mpsc: empty queue pending is zero") {
    MPSCQueue<int> q;
    CHECK(q.pending() == 0);
}

TEST_CASE("mpsc: try_pop on empty returns false") {
    MPSCQueue<int> q;
    int out = -1;
    CHECK_FALSE(q.try_pop(out));
    CHECK(out == -1);
}

TEST_CASE("mpsc: push then pop one item") {
    MPSCQueue<int> q;
    q.push(42);
    CHECK(q.pending() == 1);
    int out = 0;
    CHECK(q.try_pop(out));
    CHECK(out == 42);
    CHECK(q.pending() == 0);
}

TEST_CASE("mpsc: FIFO order preserved") {
    MPSCQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    int out = 0;
    CHECK(q.try_pop(out)); CHECK(out == 1);
    CHECK(q.try_pop(out)); CHECK(out == 2);
    CHECK(q.try_pop(out)); CHECK(out == 3);
    CHECK_FALSE(q.try_pop(out));
}

TEST_CASE("mpsc: interleaved push and pop") {
    MPSCQueue<int> q;
    int out = 0;
    q.push(10);
    CHECK(q.try_pop(out)); CHECK(out == 10);
    q.push(20);
    q.push(30);
    CHECK(q.try_pop(out)); CHECK(out == 20);
    q.push(40);
    CHECK(q.try_pop(out)); CHECK(out == 30);
    CHECK(q.try_pop(out)); CHECK(out == 40);
    CHECK_FALSE(q.try_pop(out));
}

TEST_CASE("mpsc: pending tracks count correctly") {
    MPSCQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    CHECK(q.pending() == 3);
    int out = 0;
    q.try_pop(out);
    CHECK(q.pending() == 2);
    q.try_pop(out);
    q.try_pop(out);
    CHECK(q.pending() == 0);
}

TEST_CASE("mpsc: many items") {
    MPSCQueue<int> q;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) q.push(i);
    CHECK(q.pending() == N);
    for (int i = 0; i < N; ++i) {
        int out = -1;
        CHECK(q.try_pop(out));
        CHECK(out == i);
    }
    CHECK(q.pending() == 0);
}

// ---------------------------------------------------------------------------
// move semantics
// ---------------------------------------------------------------------------

TEST_CASE("mpsc: works with move-only types") {
    MPSCQueue<std::unique_ptr<int>> q;
    q.push(std::make_unique<int>(99));
    std::unique_ptr<int> out;
    CHECK(q.try_pop(out));
    REQUIRE(out);
    CHECK(*out == 99);
}

TEST_CASE("mpsc: works with strings") {
    MPSCQueue<std::string> q;
    q.push("hello");
    q.push("world");
    std::string out;
    CHECK(q.try_pop(out)); CHECK(out == "hello");
    CHECK(q.try_pop(out)); CHECK(out == "world");
}

// ---------------------------------------------------------------------------
// destructor cleans up unconsumed items
// ---------------------------------------------------------------------------

TEST_CASE("mpsc: destructor with pending items does not leak") {
    // no assert, just checking it doesn't crash/hang
    MPSCQueue<std::shared_ptr<int>> q;
    auto p = std::make_shared<int>(1);
    q.push(p);
    q.push(p);
    CHECK(p.use_count() == 3);
    // q goes out of scope, nodes are deleted
}

// ---------------------------------------------------------------------------
// multi-producer single-consumer
// ---------------------------------------------------------------------------

TEST_CASE("mpsc: multiple producers all items received") {
    MPSCQueue<int> q;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int PER_PRODUCER = 5000;

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&q, p] {
            for (int i = 0; i < PER_PRODUCER; ++i) {
                q.push(p * PER_PRODUCER + i);
            }
        });
    }

    for (auto &t : producers) t.join();

    CHECK(q.pending() == NUM_PRODUCERS * PER_PRODUCER);

    std::set<int> seen;
    int out = 0;
    while (q.try_pop(out)) {
        seen.insert(out);
    }

    CHECK(seen.size() == NUM_PRODUCERS * PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS * PER_PRODUCER; ++i) {
        CHECK(seen.count(i) == 1);
    }
}

TEST_CASE("mpsc: concurrent push and pop") {
    MPSCQueue<int> q;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int PER_PRODUCER = 5000;
    constexpr int TOTAL = NUM_PRODUCERS * PER_PRODUCER;

    std::atomic<bool> start{false};
    std::atomic<int> consumed{0};
    std::vector<int> results;
    results.reserve(TOTAL);

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&q, &start, p] {
            while (!start.load()) {}
            for (int i = 0; i < PER_PRODUCER; ++i) {
                q.push(p * PER_PRODUCER + i);
            }
        });
    }

    std::thread consumer([&] {
        while (!start.load()) {}
        while (consumed < TOTAL) {
            int out = 0;
            if (q.try_pop(out)) {
                results.push_back(out);
                consumed.fetch_add(1);
            }
        }
    });

    start = true;
    for (auto &t : producers) t.join();
    consumer.join();

    std::set<int> seen(results.begin(), results.end());
    CHECK(seen.size() == TOTAL);
    CHECK(q.pending() == 0);
}