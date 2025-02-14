#include <iostream>

#include "ThreadPool.h"

// 模拟耗时操作的回调函数，enqueue 方法的设计需要能够接受一个可调用对象（如函数指针或 lambda 表达式）和其参数。
template<typename T>
int process(std::vector<T> *aSrc)
{
    // 模拟耗时操作
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟耗时
    for (auto && value : *aSrc) {
        value *= 2; // 简单地将数据乘以 2
    }
    return (*aSrc).size();
}

template<typename T>
bool process2(T *aSrc)
{
    *aSrc *= 10; // 简单地将数据乘以 2

    return true;
}

int main()
{
    // 获取当前时间，作为开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 创建指定线程数量的线程池
    ThreadPool pool(4);

    // 模拟输入数据流
    for (int i = 0; i < 16; ++i) {
        std::vector<int> buffer(10, i);

        // enqueue and store future
        auto result = pool.enqueue(process<int>, &buffer);

        // 处理任务结果
        // 这里只示例取出一个 future 的处理方式，假设程序在某个时刻停止或得到了其他条件来退出循环
        std::cout << "Added task for buffer " << i << std::endl;

        if (result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            auto ret = result.get();
            std::cout << "ret " << ret << std::endl;

            std::cout << "Buffer " << i << ": ";
            for (auto && val : buffer) {
                std::cout << val << ' ';
            }
            std::cout << std::endl;

            auto ret2 = process2(&buffer[0]);
            std::cout << "ret2 " << ret2 << ' ' << buffer[0] << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250)); // 模拟耗时
    }

    // 获取当前时间，作为开始时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时间差，单位是微妙
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "exec time " << duration << " us" << std::endl;

    return 0;
}
