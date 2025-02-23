#ifndef MULTITHREADHANDLE_H
#define MULTITHREADHANDLE_H

#include "ThreadPool.h"

class MultiThreadHandle
{
public:
    MultiThreadHandle(size_t threads);
    ~MultiThreadHandle();

    int producer();

    std::tuple<int, int, unsigned short*, size_t> processLongTime(int number, unsigned short* framebuf, size_t size);

private:
    void consumer(std::tuple<int, int, unsigned short*, size_t> t);

private:
    ThreadPool pool;

    unsigned short* buffer = nullptr;
};

#endif // MULTITHREADHANDLE_H
