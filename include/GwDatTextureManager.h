#pragma once

#include <d3d9.h>
#include <cstdint>
#include <string>

class GwDatTextureManager {
public:
    static GwDatTextureManager& Instance() {
        static GwDatTextureManager instance;
        return instance;
    }

    void SetDevice(IDirect3DDevice9* device);
    IDirect3DTexture9* GetTexture(const std::wstring& texture_key);
    IDirect3DTexture9* GetTextureByFileId(uint32_t file_id);
    void CleanupOldTextures(int timeout_seconds = 30);

    static bool IsDatTextureKey(const std::wstring& texture_key);
    static uint32_t ParseFileId(const std::wstring& texture_key);

private:
    GwDatTextureManager() = default;
    ~GwDatTextureManager() = default;
    GwDatTextureManager(const GwDatTextureManager&) = delete;
    GwDatTextureManager& operator=(const GwDatTextureManager&) = delete;

    bool EnsureHooks();
    IDirect3DTexture9* LoadTextureFromFileId(uint32_t file_id);

private:
    IDirect3DDevice9* d3d_device_ = nullptr;
    bool hooks_initialized_ = false;
    bool hooks_ready_ = false;
};
