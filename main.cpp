#include "MultiThreadHandle.h"

int main()
{
    // 获取当前时间，作为开始时间
    auto start = std::chrono::high_resolution_clock::now();

    {
        MultiThreadHandle handle(4);
        int counter = handle.producer();

        std::cout << "added to the pool tasks: " << counter << std::endl;
    }

    // 获取当前时间，作为结束时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时间差，单位是微妙
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "exec time " << duration << " us" << std::endl;

    return 0;
}
