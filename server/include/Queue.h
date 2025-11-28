#ifndef QUEUE_H
#define QUEUE_H

#include <queue>
#include <shared_mutex>
#include <condition_variable>
#include "Task.h"

using write_lock = std::unique_lock<std::shared_mutex>;
using read_lock  = std::shared_lock<std::shared_mutex>;

class Queue {
public:
    Queue() = default;
    ~Queue();

    void ShutdownQueue();

    void Emplace(const Task& task);
    Task Pop();

    bool Empty();
    int Size();
    void Clear();

private:
    std::queue<Task> tasks;

    mutable std::shared_mutex m_rw_lock;
    std::condition_variable_any condVar;
    bool m_shutdown = false;
};

#endif