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
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();

private:
    template<typename T>
    void notifyTaskCompleted(T result);

    void consumerCompletedTasks();

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable consumer_condition;  // notify consumer of condition variables
    bool stop;
    bool done;

    // completed tasks queue
    std::queue<int> completed_tasks;

    // consumer thread for completed tasks
    std::thread consumer_thread;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false), done(false)
{
    // Start the consumer thread to process completed tasks
    consumer_thread = std::thread(&ThreadPool::consumerCompletedTasks, this);

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

                    task();
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
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
            this->notifyTaskCompleted(999);
        });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
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

// notify that the task has completed and pass on the result
template<typename T>
inline void ThreadPool::notifyTaskCompleted(T result)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow notifyTaskCompleted after stopping the pool
        if(done)
            throw std::runtime_error("notifyTaskCompleted on stopped ThreadPool");

        completed_tasks.push(result);
    }
    consumer_condition.notify_one();
}

// consumer thread for printing completed tasks
inline void ThreadPool::consumerCompletedTasks()
{
    while (true) {
        int result;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            consumer_condition.wait(lock, [this] { return this->done || !this->completed_tasks.empty(); });
            if (done && completed_tasks.empty())
                return;
            if (!completed_tasks.empty()) {
                result = completed_tasks.front();
                completed_tasks.pop();
            }
            else {
                continue;
            }

        }

        std::cout << result << std::endl;
    }
}

#endif
