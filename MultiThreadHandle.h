#ifndef MULTITHREADHANDLE_H
#define MULTITHREADHANDLE_H

#include "ThreadPool.h"

class MultiThreadHandle
{
public:
    MultiThreadHandle(size_t threads);
    ~MultiThreadHandle();

    size_t producer();

private:
    int processLongTime(int value);
    bool consumer(int result);

private:
    ThreadPool pool;
};

#endif // MULTITHREADHANDLE_H
