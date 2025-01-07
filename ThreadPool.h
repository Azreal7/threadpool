#include <iostream>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include <vector>
#include <queue>
#include <atomic>

struct TaskCompare {
    bool operator()(const std::pair<int, std::function<void()>> &a,
                    const std::pair<int, std::function<void()>> &b) {
        return a.first > b.first; // 数字小的优先级高
    }
};

class ThreadPool {
public:
    // 定义错误处理回调函数类型
    using ErrorCallback = std::function<void(const std::string &thread_id, const std::exception &e)>;

    // 构造函数增加错误处理回调
    ThreadPool(size_t num_threads, ErrorCallback error_cb = nullptr);
    ~ThreadPool();
    template <class F, class... Args>
    auto addTask(F &&f, int priority, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>;

    // 获取统计信息
    struct Stats {
        size_t failed_tasks;
        size_t completed_tasks;
        size_t queued_tasks;
    };

    Stats getStats() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return Stats{
            failed_tasks.load(),
            completed_tasks.load(),
            tasks.size()};
    }

private:
    using Task = std::pair<int, std::function<void()>>;
    std::vector<std::thread> threads;
    std::priority_queue<Task, std::vector<Task>, TaskCompare> tasks;
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    ErrorCallback error_callback;
    std::atomic<size_t> failed_tasks{0};
    std::atomic<size_t> completed_tasks{0};
};

ThreadPool::ThreadPool(size_t num_threads, ErrorCallback error_cb) :
    stop(false), error_callback(error_cb) {
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([this]() {
            const std::string thread_id = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { 
                        return this->stop || !this->tasks.empty(); 
                    });
                    
                    if (this->stop && this->tasks.empty()) return;
                    
                    task = std::move(this->tasks.top().second);
                    this->tasks.pop();
                }                
                task();
            } });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto &thread : this->threads) {
        thread.join();
    }
}

template <class F, class... Args>
auto ThreadPool::addTask(F &&f, int priority, Args &&...args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), ... args = std::forward<Args>(args), this]() {
            try {
                auto result = f(args...); // 先执行任务
                ++completed_tasks;        // 成功后才增加计数
                return result;
            } catch (const std::exception &e) {
                ++failed_tasks; // 失败时增加失败计数
                if (error_callback) {
                    error_callback("thread_id", e);
                }
                throw;
            }
        });

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop) {
            throw std::runtime_error("Cannot add task to stopped ThreadPool");
        }

        tasks.emplace(priority, [task]() { (*task)(); });
    }

    condition.notify_one();
    return res;
}
