#include "ThreadPool.h"

ThreadPool::~ThreadPool() {
    Terminate();
}

void ThreadPool::Initialize(size_t worker_count) {
    write_lock lock(m_rw_lock);
    if (m_initialized || m_terminated) {
        return;
    }
    m_workers.reserve(worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
        m_workers.emplace_back(&ThreadPool::Routine, this);
    }
    m_initialized = !m_workers.empty();
    m_paused = false;
    m_forceStop = false;
    m_terminated = false;
}

void ThreadPool::Start() {
    {
        write_lock lock(m_rw_lock);
        if (!WorkingUnsafe()) {
            return;
        }
        m_paused = false;
    }
    m_taskWaiter.notify_all();
}

void ThreadPool::Pause() {
    write_lock lock(m_rw_lock);
    if (!WorkingUnsafe()) {
        return;
    }
    m_paused = true;
}

void ThreadPool::Resume() {
    {
        write_lock lock(m_rw_lock);
        if (!WorkingUnsafe()) {
            return;
        }
        m_paused = false;
    }
    m_taskWaiter.notify_all();
}

void ThreadPool::AddTask(const Task& task) {
    {
        read_lock lock(m_rw_lock);
        if (!WorkingUnsafe()) {
            return;
        }
    }
    m_queue.Emplace(task);
    m_taskWaiter.notify_one();
}

void ThreadPool::Terminate() {
    {
        write_lock lock(m_rw_lock);
        if (WorkingUnsafe()) {
            m_terminated = true;
            m_forceStop = true;
        } else {
            m_workers.clear();
            m_initialized = false;
            m_terminated = false;
            return;
        }
    }

    m_queue.ShutdownQueue();
    m_taskWaiter.notify_all();

    for (std::thread& worker : m_workers) {
        worker.join();
    }
    m_workers.clear();
    m_initialized = false;
}

void ThreadPool::TerminateNow() {
    {
        write_lock lock(m_rw_lock);
        m_terminated = true;
        m_forceStop  = true;
    }
    m_taskWaiter.notify_all();
    m_queue.ShutdownQueue();

    for (auto& worker : m_workers) {
        worker.join();
    }
    m_workers.clear();
    m_initialized = false;
}

bool ThreadPool::Working() const {
    read_lock lock(m_rw_lock);
    return WorkingUnsafe();
}

bool ThreadPool::WorkingUnsafe() const {
    return m_initialized && !m_terminated;
}

void ThreadPool::Routine() {
    while (true) {
        {
            write_lock lock(m_rw_lock);
            while (m_paused && !m_terminated) {
                m_taskWaiter.wait(lock);
            }
            if (m_terminated || m_forceStop) {
                return;
            }
        }

        Task task = m_queue.Pop();
        task.Execute();
    }
}