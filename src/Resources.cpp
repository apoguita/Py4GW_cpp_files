#include "Headers.h"
#include "Resources.h"

#include <queue>

namespace {
    std::recursive_mutex dx_mutex;
    std::queue<std::function<void(IDirect3DDevice9*)>> dx_jobs;
}

void Resources::EnqueueDxTask(const std::function<void(IDirect3DDevice9*)>& f)
{
    dx_mutex.lock();
    dx_jobs.push(f);
    dx_mutex.unlock();
}

void Resources::DxUpdate(IDirect3DDevice9* device, size_t max_jobs)
{
    size_t jobs_run = 0;
    while (true) {
        if (max_jobs && jobs_run >= max_jobs) {
            return;
        }

        dx_mutex.lock();
        if (dx_jobs.empty()) {
            dx_mutex.unlock();
            return;
        }
        const std::function<void(IDirect3DDevice9*)> func = std::move(dx_jobs.front());
        dx_jobs.pop();
        dx_mutex.unlock();
        func(device);
        ++jobs_run;
    }
}
