#pragma once

#include <d3d9.h>
#include <cstdint>
#include <string>
#include <vector>

class GwDatTextureManager {
public:
    static GwDatTextureManager& Instance() {
        static GwDatTextureManager instance;
        return instance;
    }

    void SetDevice(IDirect3DDevice9* device);
    void CpuUpdate();
    void DxUpdate(IDirect3DDevice9* device);
    IDirect3DTexture9* GetTexture(const std::wstring& texture_key);
    IDirect3DTexture9* GetTextureByFileId(uint32_t file_id);
    void CleanupOldTextures(int timeout_seconds = 30);
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);

    static bool IsDatTextureKey(const std::wstring& texture_key);
    static uint32_t ParseFileId(const std::wstring& texture_key);
    static bool ReadDatFile(const wchar_t* file_hash, std::vector<uint8_t>* bytes_out, uint32_t stream_id = 0);

private:
    GwDatTextureManager() = default;
public:
    ~GwDatTextureManager();
private:
    GwDatTextureManager(const GwDatTextureManager&) = delete;
    GwDatTextureManager& operator=(const GwDatTextureManager&) = delete;

    bool EnsureHooks();

private:
    IDirect3DDevice9* d3d_device_ = nullptr;
    bool hooks_initialized_ = false;
    bool hooks_ready_ = false;
};
