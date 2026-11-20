#pragma once
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#if defined(__linux__)
#include <sched.h>
#endif
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace rf {
inline unsigned availableHardwareThreads() {
    unsigned count = std::max(1u, std::thread::hardware_concurrency());
#if defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) == 0)
        count = std::min(count, unsigned(std::max(1, CPU_COUNT(&allowed))));
#endif
    return count;
}

class Jobs {
    inline static thread_local const Jobs *activePool = nullptr;
    std::mutex mutex;
    std::condition_variable ready;
    bool stopping = false;
    std::queue<std::function<void()>> work;
    std::vector<std::thread> workers;

  public:
    explicit Jobs(unsigned count = 0) {
        if (!count) {
            const unsigned available = std::max(1u, availableHardwareThreads() - 1);
            count = available;
            if (const char *setting = std::getenv("JSD_WORKERS")) {
                char *end = nullptr;
                unsigned long requested = std::strtoul(setting, &end, 10);
                if (end == setting || *end || requested == 0 || requested > 256)
                    throw std::invalid_argument("JSD_WORKERS must be an integer from 1 to 256");
                count = unsigned(std::min<unsigned long>(requested, available));
            }
        }
        for (unsigned i = 0; i < count; i++)
            workers.emplace_back([this] {
                activePool = this;
                for (;;) {
                    std::function<void()> f;
                    {
                        std::unique_lock lock(mutex);
                        ready.wait(lock, [this] { return stopping || !work.empty(); });
                        if (stopping && work.empty())
                            return;
                        f = std::move(work.front());
                        work.pop();
                    }
                    f();
                }
            });
    }
    ~Jobs() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        ready.notify_all();
        for (auto &t : workers)
            t.join();
    }
    template <class F> auto submit(F f) {
        using R = decltype(f());
        auto p = std::make_shared<std::packaged_task<R()>>(std::move(f));
        auto future = p->get_future();
        {
            std::lock_guard lock(mutex);
            work.emplace([p] { (*p)(); });
        }
        ready.notify_one();
        return future;
    }
    // Small jobs and nested work stay local; GL remains on the owning render thread.
    template <class F> unsigned parallelFor(size_t size, size_t grain, F &&fn) {
        grain = std::max<size_t>(1, grain);
        unsigned lanes = unsigned(std::min<size_t>({workers.size() + 1, availableHardwareThreads(),
                                                    (size + grain - 1) / grain}));
        if (lanes < 2 || activePool == this) { fn(0, size); return 1; }
        std::vector<std::future<void>> futures;
        futures.reserve(lanes - 1);
        std::exception_ptr failure;
        try {
            for (unsigned lane = 1; lane < lanes; ++lane) {
                size_t lo = size * lane / lanes, hi = size * (lane + 1) / lanes;
                futures.push_back(submit([&, lo, hi] { fn(lo, hi); }));
            }
            fn(0, size / lanes);
        } catch (...) { failure = std::current_exception(); }
        for (auto &future : futures) {
            try { future.get(); }
            catch (...) { if (!failure) failure = std::current_exception(); }
        }
        if (failure) std::rethrow_exception(failure);
        return lanes;
    }
    unsigned count() const { return workers.size(); }
};
} // namespace rf
