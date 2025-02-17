#include "ThreadPool.h"

int processLongTime(int val)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 模拟任务耗时
    val = val*10; // 简单地将数据乘以 2

    return val;
}

void producer(ThreadPool& pool)
{
    for (int i = 0; i < 8; ++i) {
        pool.enqueue(processLongTime, i); // 将任务添加到线程池
        std::cout << "Task " << i << " added to the pool." << std::endl;
//        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 定时添加任务
    }
}

int main()
{
    // 获取当前时间，作为开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 创建指定线程数量的线程池
    ThreadPool pool(4);

    // 启动生产者线程
    std::thread producerThread(producer, std::ref(pool));

    // 等待线程完成
    producerThread.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(2100)); // 定时添加任务

    // 获取当前时间，作为开始时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时间差，单位是微妙
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "exec time " << duration << " us" << std::endl;

    return 0;
}
