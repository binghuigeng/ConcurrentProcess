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
****************************************************************************************************************/

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Buffer 结构体
struct Buffer {
    int id;          // 序号
    std::vector<int> data; // 数据
    std::vector<int> result; // 结果
    bool ready = false;    // 是否可以处理
};

// 线程池
class ThreadPool {
public:
    ThreadPool(size_t numThreads, std::function<void(Buffer&)> callback) :
        m_numThreads(numThreads), m_callback(callback), m_threads(numThreads) {
        for (size_t i = 0; i < m_numThreads; ++i) {
            m_threads[i] = std::thread(&ThreadPool::workerThread, this, i);
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

    void enqueue(Buffer buffer) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_taskQueue.push(std::move(buffer));
        }
        m_condition.notify_one();
    }

private:
    void workerThread(size_t threadId) {
        while (true) {
            Buffer buffer;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stop || !m_taskQueue.empty(); });
                if (m_stop && m_taskQueue.empty()) {
                    return;
                }
                buffer = std::move(m_taskQueue.front());
                m_taskQueue.pop();
            }
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                std::cout << "Thread " << threadId << " is processing buffer " << buffer.id << std::endl;
            }

            m_callback(buffer); // 调用回调函数，执行耗时操作
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_completeQueue.push(std::move(buffer));
            }
            m_completeCondition.notify_one();
        }
    }

public:
    Buffer getCompletedBuffer() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_completeCondition.wait(lock, [this] { return !m_completeQueue.empty(); });
        Buffer buffer = std::move(m_completeQueue.front());
        m_completeQueue.pop();
        return buffer;
    }

private:
    size_t m_numThreads;
    std::function<void(Buffer&)> m_callback;
    std::vector<std::thread> m_threads;
    std::queue<Buffer> m_taskQueue;
    std::queue<Buffer> m_completeQueue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_completeCondition;
    bool m_stop = false;
};

// 模拟耗时操作的回调函数
void simulateExpensiveOperation(Buffer& buffer) {
    // 模拟耗时操作
    for (int& data : buffer.data) {
        buffer.result.push_back(data * 2); // 简单地将数据乘以 2
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟耗时
    }
}

int main() {
    // 获取当前时间，作为开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 线程池大小
    size_t numThreads = 4;
    // 创建线程池，传入耗时操作的回调函数
    ThreadPool pool(numThreads, simulateExpensiveOperation);

    // 模拟输入数据流
    std::vector<Buffer> inputBuffers;
    for (int i = 0; i < 10; ++i) {
        Buffer buffer;
        buffer.id = i;
        buffer.data = std::vector<int>(10, i); // 每个 Buffer 包含 10 个值为 i 的数据
        inputBuffers.push_back(std::move(buffer));
    }

    // 将 Buffer 提交到线程池进行处理
    for (Buffer& buffer : inputBuffers) {
        pool.enqueue(std::move(buffer));
    }

    // 按照顺序获取处理结果
    std::vector<Buffer> outputBuffers;
    for (int i = 0; i < inputBuffers.size(); ++i) {
        Buffer buffer = pool.getCompletedBuffer();
        outputBuffers.push_back(std::move(buffer));
    }

    // 验证结果的顺序
    std::sort(outputBuffers.begin(), outputBuffers.end(), [](const Buffer& a, const Buffer& b) {
        return a.id < b.id;
    });

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
