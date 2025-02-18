#include "MultiThreadHandle.h"

MultiThreadHandle::MultiThreadHandle(size_t threads)
    :   pool(threads)
{
//    std::cout << "-----------------------------MultiThreadHandle gouzao" << std::endl;
    // 注册消费者回调函数
    pool.registerConsumerCallBack([this](int result) { return consumer(result); });
}

MultiThreadHandle::~MultiThreadHandle()
{
//    std::cout << "-----------------------------MultiThreadHandle xigou" << std::endl;
}

int MultiThreadHandle::producer()
{
    int counter = 0;
    for (counter = 0; counter < 8; ++counter) {
        // 添加任务到线程池
        pool.enqueue(&MultiThreadHandle::processLongTime, this, counter);
        std::cout << "Task " << counter << " added to the pool." << std::endl;
//        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 定时添加任务
    }

    return counter;
}

int MultiThreadHandle::processLongTime(int value)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 模拟任务耗时
    value = value*10;  // 简单地将数据乘以 10

    return value;
}

bool MultiThreadHandle::consumer(int result)
{
    std::cout << "Task result: " << result << std::endl;

    return true;  // or some other logic
}
