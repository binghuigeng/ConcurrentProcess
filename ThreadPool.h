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

class ThreadPool {
public:
    ThreadPool(size_t, std::function<bool(int)> consumer);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> void;
    ~ThreadPool();

private:
    // notify consumer that a task has been completed
    void notifyTaskCompleted();

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

    // completed tasks queue
    std::queue<int> completed_tasks;

    // consumer thread for completed tasks
    std::thread consumer_thread;

    // consumer function
    std::function<bool(int)> consumer_function;

    // store the futures for completed tasks
    std::queue<std::future<int>> futures;  // Store future results
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads, std::function<bool(int)> consumer)
    :   stop(false), done(false), consumer_function(consumer)
{
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

                    task();
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> void
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
    futures.push(std::move(res));
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

// notify consumer that a task has been completed
inline void ThreadPool::notifyTaskCompleted()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow notifyTaskCompleted after stopping the pool
        if(done)
            throw std::runtime_error("notifyTaskCompleted on stopped ThreadPool");

        completed_tasks.emplace(futures.front().get());
        futures.pop();
    }
    consumer_condition.notify_one();
}

// consumer thread for handle completed task
inline void ThreadPool::consumerCompletedTask()
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

        // call the consumer function and pass the result
        consumer_function(result);
    }
}

#endif
