#ifndef MULTITHREADHANDLE_H
#define MULTITHREADHANDLE_H

#include "ThreadPool.h"

class MultiThreadHandle
{
public:
    MultiThreadHandle(size_t threads);
    ~MultiThreadHandle();

    int producer();

    int processLongTime(int number, unsigned short* framebuf, size_t size);

private:
    bool consumer(int result, int number, unsigned short* framebuf, size_t size);

private:
    ThreadPool pool;

    unsigned short* buffer = nullptr;
};

#endif // MULTITHREADHANDLE_H
