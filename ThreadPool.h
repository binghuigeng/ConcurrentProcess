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
    void registerConsumerCallBack(std::function<bool(int, int, unsigned short*, size_t)> consumer);
    template<class F, class... Args>
    void enqueue(F&& f, Args&&... args);
    bool isActive() const;
    size_t workingThreadCount() const;
    void waitForCompletion();
    ~ThreadPool();

private:
    // notify consumer that a task has been completed
    template<class... Args>
    void notifyTaskCompleted(Args&&... args);

    // consumer thread for handle completed task
    void consumerCompletedTask();

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable consumer_condition;  // notify consumer of condition variable
    bool stop;
    bool done;

    // completed tasks queue (holds result, number, framebuf, and size)
    std::queue<std::tuple<int, int, unsigned short*, size_t>> completed_tasks;

    // consumer thread for completed task
    std::thread consumer_thread;

    // consumer function
    std::function<bool(int, int, unsigned short*, size_t)> consumer_function;

    // store future
    std::queue<std::future<int>> results;

    // tracking the number of active threads
    std::atomic<size_t> active_threads;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false), done(false), active_threads(0)
{
    std::cout << "-----------------------------ThreadPool gouzao" << std::endl;
    // start the consumer thread to process completed task
    consumer_thread = std::thread(&ThreadPool::consumerCompletedTask, this);

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
inline void ThreadPool::registerConsumerCallBack(std::function<bool(int, int, unsigned short*, size_t)> consumer)
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

        tasks.emplace([task, this, args...](){
            (*task)();
            this->notifyTaskCompleted(args...);
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

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        done = true;
    }

    consumer_condition.notify_one();
    consumer_thread.join();
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
template<class... Args>
inline void ThreadPool::notifyTaskCompleted(Args&&... args)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow notifyTaskCompleted after stopping the pool
        if(done)
            throw std::runtime_error("notifyTaskCompleted on stopped ThreadPool");

        try {
            // get result, number, framebuf, size from args
            auto number = std::get<1>(std::forward_as_tuple(args...));
            auto framebuf = std::get<2>(std::forward_as_tuple(args...));
            auto size = std::get<3>(std::forward_as_tuple(args...));
            // get result from future and add it to completed_tasks
            completed_tasks.emplace(results.front().get(), number, framebuf, size);
        } catch (const std::exception& e) {
            std::cerr << "Error retrieving result: " << e.what() << std::endl;
        }
        results.pop();
    }

    consumer_condition.notify_one();
}

// consumer thread for handle completed task
inline void ThreadPool::consumerCompletedTask()
{
    while (true) {
        int result;
        int number;
        unsigned short* framebuf;
        size_t size;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            consumer_condition.wait(lock, [this] { return this->done || !this->completed_tasks.empty(); });
            if (done && completed_tasks.empty())
                return;
            // unpack the result, framebuf, and size
            std::tie(result, number, framebuf, size) = std::move(completed_tasks.front());
            completed_tasks.pop();
        }

        // call the consumer function and pass the result
        consumer_function(result, number, framebuf, size);
    }
}

#endif
