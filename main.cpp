#include "ThreadPool.h"
#include <iostream>
#include <string>
#include <exception>

int main()
{
    // 创建线程池，设置错误处理回调
    ThreadPool pool(4, [](const std::string &thread_id, const std::exception &e)
                    { std::cerr << "Thread " << thread_id << " caught exception: "
                                << e.what() << std::endl; });

    // 提交可能抛出异常的任务
    auto future = pool.addTask([]()
                               {
        throw std::runtime_error("Task failed!");
        return 42; }, 1);

    try
    {
        future.get(); // 将在这里捕获异常
    }
    catch (const std::exception &e)
    {
        std::cout << "Caught exception from task: " << e.what() << std::endl;
    }

    // 获取统计信息
    auto stats = pool.getStats();
    std::cout << "Failed tasks: " << stats.failed_tasks << std::endl;
    std::cout << "Completed tasks: " << stats.completed_tasks << std::endl;

    return 0;
}