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
    IDirect3DTexture9* GetColoredModelTexture(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4);
    void CleanupOldTextures(int timeout_seconds = 30);
    static IDirect3DTexture9** LoadTextureFromFileId(uint32_t file_id);
    static IDirect3DTexture9** LoadColoredTextureFromModel(uint32_t model_file_id, uint8_t dye_tint, uint8_t dye1, uint8_t dye2, uint8_t dye3, uint8_t dye4);

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
