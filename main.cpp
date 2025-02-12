/****************************************************************************************************************
**
** @brief 这是一个使用多线程加速有序数据流处理的 C++ 示例，重点在于线程调度、负载均衡和回调机制。
**
** 设计思路
** 1. 数据结构
**     • Buffer 结构体：包含数据、序号以及处理结果的存储位置。
**     • 线程池：管理一组工作线程。
**     • 任务队列：存储待处理的 Buffer。
**     • 完成队列：存储已处理完成的 Buffer，等待按顺序输出。
**     • 线程状态：每个线程有一个标志位，指示其是否可以处理新的 Buffer。
** 2. 线程调度
**     • 主线程负责接收数据流，并将 Buffer 添加到任务队列。
**     • 工作线程从任务队列取出 Buffer，并通过回调函数处理 Buffer（执行耗时操作）。
**     • 处理完成后，将 Buffer 放入完成队列。
**     • 主线程从完成队列按顺序取出 Buffer，并将处理结果输出。
** 3. 负载均衡
**     • 线程池优先将 Buffer 分配给空闲的线程。
**     • 可以动态调整线程池大小，以适应不同的负载。
** 4. Buffer 单位大小
**     • Buffer 的大小需要根据实际情况进行调整，太小会导致线程切换开销过大，太大则可能导致负载不均衡。
** 5. 回调机制
**     • 提供一个耗时操作的回调函数，使得该加速函数可以通用于所有这类计算加速问题。
**
**
** 代码说明
** 1. Buffer 结构体
**     • id：用于标识 Buffer 的序号，保证输出结果的顺序。
**     • data：存储输入数据。
**     • result：存储处理结果。
**     • ready：这里虽然定义了这个 bool 值，但是没有使用，原因是在这个版本的实现中，通过控制 enqueue 的顺序来保证 buffer 处理的顺序。
**     • 完成队列：存储已处理完成的 Buffer，等待按顺序输出。
**     • 线程状态：每个线程有一个标志位，指示其是否可以处理新的 Buffer。
** 2. ThreadPool 类
**     • m_numThreads：线程池大小。
**     • m_callback：耗时操作的回调函数。
**     • m_threads：线程池中的线程。
**     • m_taskQueue：任务队列，存储待处理的 Buffer。
**     • m_completeQueue：完成队列，存储已处理完成的 Buffer。
**     • m_mutex：互斥锁，用于保护共享数据。
**     • m_condition：条件变量，用于线程同步。
**     • enqueue：将 Buffer 添加到任务队列。
**     • workerThread：工作线程，从任务队列获取 Buffer 进行处理，并通过回调函数执行耗时操作，然后将 Buffer 放入完成队列。
**     • getCompletedBuffer：从完成队列按顺序取出 Buffer，并将处理结果输出。
** 3. simulateExpensiveOperation 函数
**     • 模拟耗时操作的回调函数，简单地将数据乘以 2，并模拟 100 毫秒的延迟。
** 4. main 函数
**     • 创建线程池，传入耗时操作的回调函数。
**     • 模拟输入数据流，并将 Buffer 提交到线程池进行处理。
**     • 按照顺序获取处理结果，并验证结果的顺序。
**     • 输出处理结果。
**
**
** 要点说明
** • 线程同步：使用互斥锁和条件变量来保护共享数据，避免数据竞争。
** • 负载均衡：线程池中的线程会自动竞争任务队列中的 Buffer，从而实现负载均衡。
** • 回调机制：通过回调函数，可以将耗时操作的实现与线程池的调度逻辑分离，提高代码的灵活性和可重用性。
** • 数据顺序：通过 Buffer 的 id 字段，保证输出结果的顺序与输入顺序一致。
** • 错误处理：代码中没有包含错误处理，例如处理回调函数抛出的异常。在实际应用中，需要添加适当的错误处理机制。
** • 性能优化：代码中还有一些可以优化的点，例如使用无锁队列、减少锁的粒度等。
** • 动态线程池：可以根据负载动态调整线程池的大小。
**
**
** @revision 实现一个不会在外部进行排序的多线程调度机制
** 可以在 ThreadPool 类中通过维护一个有序的任务队列来实现，确保任务在处理完成后能够按照原始输入的顺序输出。
** 这可以通过多种方式来实现，但一种有效的方式是使用 std::future 和 std::promise。
**
** 我们将使用 std::promise 来存储每个 Buffer 的处理结果，并通过 std::future 来确保按顺序获取结果。
** 原理是在处理时还保留每个任务的原始索引，以便将来能够按顺序集中处理结果。这样我们就没有必要在外部进行排序。
**
** 关键变化和功能解释
** 1. 改进的任务提交逻辑
**     • 在 ThreadPool 的 enqueue 函数中，现在返回一个 std::future<Buffer>，用于在将任务添加到队列时跟踪结果。这通过 std::promise 完成。
** 2. 任务队列支持
**     • 每个任务使用 std::function<void()> 类型的 lambda 表达式来处理 Buffer，并在处理完成后设置 promise 的值。
** 3. 按顺序获取结果
**     • 在 main 函数中，我们通过 std::future<Buffer> 抓取处理的每个 Buffer，这样我们能够保证按照提交的顺序获取结果
**
****************************************************************************************************************/
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

// Buffer 结构体
struct Buffer {
    int id;                       // 序号
    std::vector<int> data;       // 数据
    std::vector<int> result;     // 处理结果
};

// 线程池类
class ThreadPool {
public:
    ThreadPool(size_t numThreads, std::function<void(Buffer&)> callback)
        : m_numThreads(numThreads), m_callback(callback) {
        for (size_t i = 0; i < m_numThreads; ++i) {
            m_threads.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (std::thread& thread : m_threads) {
            thread.join();
        }
    }

    // 提交任务
    std::future<Buffer> enqueue(Buffer buffer) {
        std::shared_ptr<std::promise<Buffer>> promise = std::make_shared<std::promise<Buffer>>();
        auto future = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_taskQueue.push([buffer, promise, this]() mutable {
                m_callback(buffer);
                promise->set_value(buffer);
            });
        }
        m_condition.notify_one();
        return future;
    }

private:
    void workerThread() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stop || !m_taskQueue.empty(); });
                if (m_stop && m_taskQueue.empty()) return;
                task = std::move(m_taskQueue.front());
                m_taskQueue.pop();
            }
            task(); // 执行任务
        }
    }

    size_t m_numThreads;                                  // 线程数量
    std::function<void(Buffer&)> m_callback;             // 耗时操作回调
    std::vector<std::thread> m_threads;                  // 工作线程
    std::queue<std::function<void()>> m_taskQueue;      // 任务队列
    std::mutex m_mutex;                                  // 互斥锁
    std::condition_variable m_condition;                 // 条件变量
    bool m_stop = false;                                 // 是否停止标志
};

void simulateExpensiveOperation(Buffer& buffer) {
    for (int& data : buffer.data) {
        buffer.result.push_back(data * 2); // 模拟一个耗时操作，将数据乘以 2
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟延迟
    }
}

int main()
{
    // 获取当前时间，作为开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 创建线程池，指定线程数量和耗时操作的回调函数
    ThreadPool pool(4, simulateExpensiveOperation);

    // 模拟输入数据流
    std::vector<Buffer> inputBuffers;
    for (int i = 0; i < 10; ++i) {
        Buffer buffer;
        buffer.id = i;
        buffer.data = std::vector<int>(10, i); // 每个 Buffer 包含 10 个数据
        inputBuffers.push_back(std::move(buffer));
    }

    // 提交任务并收集结果
    std::vector<std::future<Buffer>> futures;
    for (Buffer& buffer : inputBuffers) {
        futures.push_back(pool.enqueue(std::move(buffer)));
    }

    // 获取按原始顺序处理的结果
    std::vector<Buffer> outputBuffers;
    for (auto& future : futures) {
        outputBuffers.push_back(future.get()); // 按顺序获取结果
    }

    // 输出处理结果
    for (const Buffer& buffer : outputBuffers) {
        std::cout << "Buffer " << buffer.id << " result: ";
        for (int result : buffer.result) {
            std::cout << result << " ";
        }
        std::cout << std::endl;
    }

    // 获取当前时间，作为开始时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时间差，单位是微妙
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "exec time " << duration << " us" << std::endl;

    return 0;
}
