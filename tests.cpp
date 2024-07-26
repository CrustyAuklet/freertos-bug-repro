#include <seal/thread.hpp>
#include <snitch/snitch.hpp>
#include <larid/memory.hpp>
#include <chrono>
using namespace std::chrono_literals;

namespace {
    seal::detail::InplaceThreadState<configMINIMAL_STACK_SIZE, sizeof(void*) * 4> state1;
    seal::detail::InplaceThreadState<configMINIMAL_STACK_SIZE, sizeof(void*) * 4> state2;
}  // namespace

TEST_CASE("seal::Thread construction", "[threads]") {
    seal::Thread t1;
    REQUIRE_FALSE(t1.joinable());
    REQUIRE(t1.get_id() == seal::Thread::id{});

    // Just this main testing thread is running now, not t1 or t2
    REQUIRE(seal::os::number_of_threads() == 1);

    std::atomic_flag flag1;
    t1 = seal::Thread("", 5, state1, [&flag1]() {
        for (;;) {
            flag1.test_and_set();
            seal::this_thread::yield();
        }
    });

    // there is now a thread running, and if we yeild time it will set flag1
    REQUIRE(seal::os::number_of_threads() == 2);
    REQUIRE(t1.priority() == 5);
    REQUIRE(t1.joinable());
    seal::this_thread::sleep_for(10ms);
    REQUIRE(flag1.test());

    flag1.clear();
    std::atomic_flag flag2;
    t1 = seal::Thread("", 2, state2, [&flag2]() {
        for (;;) {
            flag2.test_and_set();
            seal::this_thread::yield();
        }
    });

    // moved new thread into t1, so t1 will be killed and new thread started
    // the new thread is setting flag2, so flag1 shouldn't change.
    REQUIRE(seal::os::number_of_threads() == 2);
    REQUIRE(t1.priority() == 2);
    REQUIRE(t1.joinable());
    seal::this_thread::sleep_for(10ms);
    REQUIRE_FALSE(flag1.test());
    REQUIRE(flag2.test());

    // create a second thread at the same priority and have it setting
    // flag1. Once CPU time is given to the threads they should share time
    // and both flags will be set.
    flag2.clear();
    seal::Thread t2("", 2, state1, [&flag1](){
        for (;;) {
            flag1.test_and_set();
            seal::this_thread::yield();
        }
    });
    REQUIRE(seal::os::number_of_threads() == 3);
    REQUIRE(t1.priority() == 2);
    REQUIRE(t1.joinable());
    REQUIRE(t2.priority() == 2);
    REQUIRE(t2.joinable());
    seal::this_thread::sleep_for(10ms);
    REQUIRE(flag1.test());
    REQUIRE(flag2.test());

    // Move t2 into t1. t2 will continue on setting flag1 and
    // t1 will be killed leaving flag2 unset.
    flag1.clear();
    flag2.clear();
    t1 = std::move(t2);
    REQUIRE(seal::os::number_of_threads() == 2);
    REQUIRE(t1.priority() == 2);
    REQUIRE(t1.joinable());
    REQUIRE_FALSE(t2.joinable());
    seal::this_thread::sleep_for(10ms);
    REQUIRE(flag1.test());
    REQUIRE_FALSE(flag2.test());

#if SNITCH_WITH_EXCEPTIONS
    // An assert will fire if trying to create a thread with an active
    // thread state object.
    REQUIRE_THROWS_AS(
        t1 = seal::Thread("", 2, state1, [&flag2](){
            for (;;) {
                flag2.test_and_set();
                seal::this_thread::yield();
            }
        }), std::exception);
#endif
}

TEST_CASE("seal::Thread join and kill", "[threads]") {
    SECTION("Empty thread function is joinable") {
        // A thread that has a function that returns and is still attached
        // to a thread object will suspend itself. The thread is therefore
        // still "active" and joinable
        seal::Thread t("", 5, state1, []() {});
        REQUIRE(t.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(t.joinable());
        REQUIRE_FALSE(t.kill());
        REQUIRE_FALSE(t.joinable());
    }

    SECTION("Infinite thread function can be killed") {
        // A thread with a never-ending loop will always be joinable
        // until it is "killed"
        static_assert(std::atomic<int>::is_always_lock_free);
        std::atomic<int> val  = 0;
        std::atomic<int> val2 = 0;
        seal::Thread t("", 5, state1, [&]() {
            val = 42;
            for (;;) {
                ++val2;
                seal::this_thread::sleep_for(10ms);
            }
        });
        REQUIRE(t.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(t.joinable());
        REQUIRE_FALSE(t.kill());
        REQUIRE_FALSE(t.joinable());
        REQUIRE(val.load() == 42);
        REQUIRE(val2.load() > 0);
    }
}

TEST_CASE("seal::Thread priorities", "[threads]") {
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    std::atomic<uint32_t> x = 0;
    std::atomic<uint32_t> y = 0;

    // t1 is higher priority than t2 so t1 should get 100% of the CPU
    // and the variable y should not increment. Once t1 is killed t2
    // will get time and y will change and x should not change.
    seal::Thread t1("", 5, state1, [&]() {
        for (;;) {
            x += 1;
            seal::this_thread::yield();
        }
    });

    seal::Thread t2("", 4, state2, [&]() {
        for (;;) {
            y += 1;
            seal::this_thread::yield();
        }
    });

    REQUIRE(seal::os::number_of_threads() == 3);
    REQUIRE_FALSE(t1.get_id() == t2.get_id());
    REQUIRE(t1.joinable());
    REQUIRE(t2.joinable());
    REQUIRE(t1.priority() == 5);
    REQUIRE(t2.priority() == 4);
    seal::this_thread::sleep_for(50ms);
    REQUIRE(x.load() > 0);
    REQUIRE(y.load() == 0);

    SECTION("Kill higher priority task") {
        REQUIRE_FALSE(t1.kill());
        const auto xx = x.load();
        REQUIRE_FALSE(t1.joinable());
        REQUIRE(t2.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(xx == x.load());
        REQUIRE(y.load() > 0);
        REQUIRE_FALSE(t2.kill());
    }

    SECTION("Raise priority of t2") {
        t2.set_priority(t1.priority() + 1);
        const auto xx = x.load();
        REQUIRE(t1.joinable());
        REQUIRE(t2.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(xx == x.load());
        REQUIRE(y.load() > 0);
        REQUIRE_FALSE(t1.kill());
        REQUIRE_FALSE(t2.kill());
    }

    SECTION("Lower priority of t1") {
        t1.set_priority(t2.priority() - 1);
        const auto xx = x.load();
        REQUIRE(t1.joinable());
        REQUIRE(t2.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(xx == x.load());
        REQUIRE(y.load() > 0);
        REQUIRE_FALSE(t1.kill());
        REQUIRE_FALSE(t2.kill());
    }

    SECTION("Time slice with equal priorities") {
        t1.set_priority(t2.priority());
        const auto xx = x.load();
        REQUIRE(t1.joinable());
        REQUIRE(t2.joinable());
        seal::this_thread::sleep_for(50ms);
        REQUIRE(xx < x.load());
        REQUIRE(y.load() > 0);
        REQUIRE_FALSE(t1.kill());
        REQUIRE_FALSE(t2.kill());
    }

    REQUIRE_FALSE(t1.joinable());
    REQUIRE_FALSE(t2.joinable());
}
