#pragma once

#include <d3d9.h>
#include <functional>

class Resources {
public:
    static void EnqueueDxTask(const std::function<void(IDirect3DDevice9*)>& f);
    static void DxUpdate(IDirect3DDevice9* device, size_t max_jobs = 0);
};
