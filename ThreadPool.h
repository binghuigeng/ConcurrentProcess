#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>

class ThreadPool {
public:
    ThreadPool(size_t);
    void registerConsumerCallBack(std::function<void(std::tuple<int, int, unsigned short*, size_t>)> consumer);
    template<class F, class... Args>
    void enqueue(F&& f, Args&&... args);
    bool isActive() const;
    size_t workingThreadCount() const;
    void waitForCompletion();
    ~ThreadPool();

private:
    // notify consumer that a task has been completed
    void notifyTaskCompleted();

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

    // consumer function
    std::function<void(std::tuple<int, int, unsigned short*, size_t>)> consumer_function;

    // store future
    std::queue<std::future<std::tuple<int, int, unsigned short*, size_t>>> results;

    // tracking the number of active threads
    std::atomic<size_t> active_threads;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false), active_threads(0)
{
    std::cout << "-----------------------------ThreadPool gouzao" << std::endl;

    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    // increase active thread count
                    ++active_threads;

                    task();

                    // decrease active thread count
                    --active_threads;
                }
            }
        );
}

// register consumer callback function
inline void ThreadPool::registerConsumerCallBack(std::function<void(std::tuple<int, int, unsigned short*, size_t>)> consumer)
{
    consumer_function = std::move(consumer);
}

// add new work item to the pool
template<class F, class... Args>
void ThreadPool::enqueue(F&& f, Args&&... args)
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task, this](){
            (*task)();
            this->notifyTaskCompleted();
        });
    }

    condition.notify_one();
    // move the future to the queue
    results.push(std::move(res));
}

// check active threads and task queues to determine whether the thread pool is active
inline bool ThreadPool::isActive() const
{
    // return true if active threads greater than 0 or task queue is not empty
    return active_threads > 0 || !tasks.empty();
}

// get the number of active threads
inline size_t ThreadPool::workingThreadCount() const
{
    // return the number of active threads
    return active_threads.load();
}

// wait for all tasks to complete
inline void ThreadPool::waitForCompletion()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    std::cout << "-----------------------------ThreadPool xigou" << std::endl;
    if (isActive()) {
        std::cout << "-----------------------------ThreadPool xigou isActive" << std::endl;
        waitForCompletion();
    }
}

// notify consumer that a task has been completed
inline void ThreadPool::notifyTaskCompleted()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        try {
            // call the consumer function and pass the result after get result from future
            consumer_function(results.front().get());
        } catch (const std::exception& e) {
            std::cerr << "Error retrieving result: " << e.what() << std::endl;
        }
        results.pop();
    }
}

#endif
