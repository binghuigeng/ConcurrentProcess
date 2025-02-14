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
    // 模拟耗时操作
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟耗时
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
    std::vector<std::vector<int>> inputBuffers;
    for (int i = 0; i < 10; ++i) {
        inputBuffers.emplace_back(10, i); // 每个 vector 包含 10 个相同的数
    }

    // 提交任务并收集结果
    std::vector<std::future<int>> results;
    for (auto& buffer : inputBuffers) {
        results.push_back(pool.enqueue(process<int>, &buffer)); // 提交任务
    }

    double dt = 999.25;

    // enqueue and store future
    auto result = pool.enqueue(process2<double>, &dt);

    // 等待所有任务完成并输出结果
    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;

    auto ret = result.get();
    std::cout << "ret " << ret << std::endl;

    // 输出修改后的 inputBuffers
    for (size_t i = 0; i < inputBuffers.size(); ++i) {
        std::cout << "Buffer " << i << ": ";
        for (int val : inputBuffers[i]) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    std::cout << dt << std::endl;

    // 获取当前时间，作为开始时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时间差，单位是微妙
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "exec time " << duration << " us" << std::endl;

    return 0;
}
