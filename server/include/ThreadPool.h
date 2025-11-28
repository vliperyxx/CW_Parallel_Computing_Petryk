#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include "Task.h"
#include "Queue.h"

using write_lock = std::unique_lock<std::shared_mutex>;
using read_lock  = std::shared_lock<std::shared_mutex>;

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool();

    void Initialize(size_t worker_count);
    void Start();
    void Pause();
    void Resume();
    void AddTask(const Task& task);

    void Terminate();
    void TerminateNow();

    bool Working() const;

private:
    void Routine();
    bool WorkingUnsafe() const;

    mutable std::shared_mutex m_rw_lock;
    std::condition_variable_any m_taskWaiter;
    std::vector<std::thread> m_workers;
    Queue m_queue;

    bool m_initialized = false;
    bool m_terminated = false;
    bool m_paused = false;
    bool m_forceStop = false;
};

#endif