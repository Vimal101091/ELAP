#include "elap/threading/ThreadManager.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

namespace {

void runThreadManagerTests()
{
    std::atomic_bool observedStop {false};
    elap::threading::ThreadManager threads;

    assert(threads.startThread("worker", [&observedStop](elap::threading::StopToken stopToken) {
        while (!stopToken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        observedStop.store(true);
    }));

    assert(!threads.startThread("worker", [](elap::threading::StopToken) {}));
    assert(threads.threadCount() == 1);
    threads.requestStop();
    threads.joinAll();
    assert(observedStop.load());
    assert(threads.threadCount() == 0);
}

} // namespace

void runServiceApplicationTests();

struct ThreadManagerTestRunner {
    ThreadManagerTestRunner()
    {
        runThreadManagerTests();
    }
} threadManagerTestRunner;

int main()
{
    runServiceApplicationTests();
    return 0;
}
