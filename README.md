# threadpool

A simple thread pool implementation in C++11.

Compared to other thread pool implementations, this one is more flexible and easy to use with exception handling and tasks priority.

You can use stats to get the number of completed and failed tasks.

## Usage
```cpp
ThreadPool pool(4, [](const std::string &thread_id, const std::exception &e)
                { std::cerr << "Thread " << thread_id << " caught exception: "
                            << e.what() << std::endl; });

auto future = pool.addTask([]() { return 2; }, 1);

auto stats = pool.getStats();
```