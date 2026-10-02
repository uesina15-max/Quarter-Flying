#include "WorkStealingDeque.h"
#include <utility>

namespace Engine
{
    WorkStealingDeque::WorkStealingDeque(WorkStealingDeque&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex);
        jobs = std::move(other.jobs);
    }

    WorkStealingDeque& WorkStealingDeque::operator=(WorkStealingDeque&& other) noexcept
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex, other.mutex);
            jobs = std::move(other.jobs);
        }
        return *this;
    }

    void WorkStealingDeque::Push(Job* job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        jobs.push_back(job);
    }

    Job* WorkStealingDeque::Pop()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.empty())
        {
            return nullptr;
        }
        Job* job = jobs.back();
        jobs.pop_back();
        return job;
    }

    Job* WorkStealingDeque::Steal()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.empty())
        {
            return nullptr;
        }
        Job* job = jobs.front();
        jobs.pop_front();
        return job;
    }

    bool WorkStealingDeque::IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return jobs.empty();
    }

    size_t WorkStealingDeque::Size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return jobs.size();
    }
}
