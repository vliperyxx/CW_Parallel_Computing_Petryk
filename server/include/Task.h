#ifndef TASK_H
#define TASK_H

#include <functional>

class Task {
public:
    Task() = default;
    Task(std::function<void()> func);

    void Execute();

private:
    std::function<void()> m_function;
};

#endif