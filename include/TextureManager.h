#pragma once
#include "Headers.h"
#include "GwDatTextureManager.h"

struct TimedTexture {
    IDirect3DTexture9* texture = nullptr;
    std::wstring name;
    std::chrono::steady_clock::time_point last_used;

    TimedTexture() = default;

    TimedTexture(IDirect3DTexture9* tex, const std::wstring& key)
        : texture(tex), name(key), last_used(std::chrono::steady_clock::now()) {
    }

    void Touch() {
        last_used = std::chrono::steady_clock::now();
    }
};

class TextureManager {
public:
    static TextureManager& Instance() {
        static TextureManager instance;
        return instance;
    }

    // Assign device before any texture use
    void SetDevice(IDirect3DDevice9* device) {
        d3d_device = device;
        GwDatTextureManager::Instance().SetDevice(device);
    }

    void AddTexture(const std::wstring& name, IDirect3DTexture9* texture) {
        textures[name] = TimedTexture(texture, name);
    }

    IDirect3DTexture9* GetTexture(const std::wstring& name) {
        constexpr std::wstring_view dat_prefix = L"gwdat://";
        const bool is_dat_key = name.size() > dat_prefix.size() && name.rfind(dat_prefix.data(), 0) == 0;
        if (is_dat_key) {
            if (!d3d_device && g_d3d_device)
                SetDevice(g_d3d_device);

            std::vector<uint32_t> parts;
            std::wstringstream stream(name.substr(dat_prefix.size()));
            std::wstring part;
            while (std::getline(stream, part, L'/')) {
                if (part.empty()) {
                    return nullptr;
                }
                try {
                    parts.push_back(static_cast<uint32_t>(std::stoul(part)));
                }
                catch (...) {
                    return nullptr;
                }
            }
            if (parts.size() == 1) {
                return GwDatTextureManager::Instance().GetTextureByFileId(parts[0]);
            }
            if (parts.size() != 6) {
                return nullptr;
            }
            return GwDatTextureManager::Instance().GetColoredModelTexture(
                parts[0],
                static_cast<uint8_t>(parts[1]),
                static_cast<uint8_t>(parts[2]),
                static_cast<uint8_t>(parts[3]),
                static_cast<uint8_t>(parts[4]),
                static_cast<uint8_t>(parts[5]));
        }

        auto it = textures.find(name);
        if (it != textures.end()) {
            it->second.Touch();
            return it->second.texture;
        }

        // Lazy-load if not found
        if (!d3d_device && g_d3d_device)
            SetDevice(g_d3d_device);

        if (!d3d_device)
            return nullptr;

        IDirect3DTexture9* new_texture = nullptr;
        if (D3DXCreateTextureFromFileW(d3d_device, name.c_str(), &new_texture) == D3D_OK && new_texture) {
            AddTexture(name, new_texture);
            return new_texture;
        }

        return nullptr;
    }

    void AddInMemoryTexture(const std::wstring& name, IDirect3DTexture9* texture) {
        if (!texture) return;

        // Store without taking ownership beyond caching
        textures[name] = TimedTexture(texture, name);
    }


    void CleanupOldTextures(int timeout_seconds = 30) {
        auto now = std::chrono::steady_clock::now();
        for (auto it = textures.begin(); it != textures.end(); ) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_used).count();
            if (duration > timeout_seconds) {
                if (it->second.texture) it->second.texture->Release();
                it = textures.erase(it);
            }
            else {
                ++it;
            }
        }
        GwDatTextureManager::Instance().CleanupOldTextures(timeout_seconds);
    }

private:
    std::unordered_map<std::wstring, TimedTexture> textures;
    IDirect3DDevice9* d3d_device = nullptr;
};


