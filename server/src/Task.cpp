#include "Task.h"

Task::Task(std::function<void()> func)
    : m_function(func) {
}

void Task::Execute() {
    if (m_function) {
        m_function();
    }
}