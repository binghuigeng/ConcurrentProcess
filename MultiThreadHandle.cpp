#include "MultiThreadHandle.h"

MultiThreadHandle::MultiThreadHandle(size_t threads)
    :   pool(threads)
{
    std::cout << "-----------------------------MultiThreadHandle gouzao" << std::endl;
    // 注册消费者回调函数
    pool.registerConsumerCallBack([this](int result, int number, unsigned short* framebuf, size_t size) { return consumer(result, number, framebuf, size); });
}

MultiThreadHandle::~MultiThreadHandle()
{
    std::cout << "-----------------------------MultiThreadHandle xigou" << std::endl;
//    std::cout << "Thread Pool isActive: " << pool.isActive() << std::endl;
    std::cout << "active threads amount: " << pool.workingThreadCount() << std::endl;
    // 判断是否为 nullptr
    if (buffer) {
        pool.waitForCompletion();
        delete[] buffer;  // 释放内存
        buffer = nullptr;  // 设置为 nullptr，避免悬挂指针
    }
}

int MultiThreadHandle::producer()
{
    unsigned short counter = 0;
//    std::cout << "Thread Pool isActive: " << pool.isActive() << std::endl;
    std::cout << "active threads amount: " << pool.workingThreadCount() << std::endl;
    for (counter = 0; counter < 8; ++counter) {
        buffer = new unsigned short[10]; // 分配10个unsigned short
        // 使用循环填充 buffer
        for (unsigned short i = 0; i < 10; ++i) {
            buffer[i] = counter; // 将每个元素设置为 5
        }
        // 添加任务到线程池
        pool.enqueue(&MultiThreadHandle::processLongTime, this, counter, buffer, sizeof(unsigned short) * 10);
        std::cout << "Task " << counter << " added to the pool." << std::endl;
//        std::cout << "Thread Pool isActive: " << pool.isActive() << std::endl;
        std::cout << "active threads amount: " << pool.workingThreadCount() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 定时添加任务
    }

    return counter;
}

int MultiThreadHandle::processLongTime(int number, unsigned short* framebuf, size_t size)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 模拟任务耗时

    // 遍历并将每个数乘以 10  注意：size / sizeof(size_t) 是实际元素数量
    for (unsigned short i = 0; i < size / sizeof(unsigned short); ++i) {
        framebuf[i] *= 10;  // 将每个元素乘以 10
    }

    return number*10;
}

bool MultiThreadHandle::consumer(int result, int number, unsigned short* framebuf, size_t size)
{
    std::cout << "Task result: " << result << ' ' << number << ' ' << size << std::endl;
    for (unsigned short i = 0; i < size / sizeof(unsigned short); ++i) {
        std::cout << framebuf[i] << ' ';
    }
    std::cout << std::endl;

    return true;  // or some other logic
}
