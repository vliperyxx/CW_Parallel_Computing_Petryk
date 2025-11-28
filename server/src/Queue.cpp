#include "Queue.h"

Queue::~Queue() {
    Clear();
}

void Queue::ShutdownQueue() {
    write_lock lock(m_rw_lock);
    m_shutdown = true;
    condVar.notify_all();
}

void Queue::Emplace(const Task& task) {
    write_lock lock(m_rw_lock);
    tasks.emplace(task);
    condVar.notify_one();
}

Task Queue::Pop() {
    write_lock lock(m_rw_lock);
    while (tasks.empty() && !m_shutdown) {
        condVar.wait(lock);
    }

    if (tasks.empty() && m_shutdown) {
        return Task([] {});
    }

    Task task = tasks.front();
    tasks.pop();
    return task;
}

bool Queue::Empty() {
    read_lock lock(m_rw_lock);
    return tasks.empty();
}

int Queue::Size() {
    read_lock lock(m_rw_lock);
    return (int)tasks.size();
}

void Queue::Clear() {
    write_lock lock(m_rw_lock);
    while (!tasks.empty()) {
        tasks.pop();
    }
}