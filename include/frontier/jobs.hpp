#pragma once
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace rf {
class Jobs {
    std::mutex mutex;
    std::condition_variable ready;
    bool stopping = false;
    std::queue<std::function<void()>> work;
    std::vector<std::thread> workers;

  public:
    explicit Jobs(unsigned count = 0) {
        if (!count)
            count = std::clamp(std::thread::hardware_concurrency(), 2u, 8u) - 1;
        for (unsigned i = 0; i < count; i++)
            workers.emplace_back([this] {
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
    unsigned count() const { return workers.size(); }
};
} // namespace rf
