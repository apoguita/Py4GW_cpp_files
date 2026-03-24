#include "Headers.h"
#include "AtexAsm.h"
#include "GwDatTextureManager.h"

namespace {
    class RecObj;

    struct Vec2i {
        int x = 0;
        int y = 0;
    };

    typedef enum : uint32_t {
        GR_FORMAT_A8R8G8B8 = 0,
        GR_FORMAT_UNK = 0x4,
        GR_FORMAT_DXT1 = 0xF,
        GR_FORMAT_DXT2,
        GR_FORMAT_DXT3,
        GR_FORMAT_DXT4,
        GR_FORMAT_DXT5,
        GR_FORMAT_DXTA,
        GR_FORMAT_DXTL,
        GR_FORMAT_DXTN,
        GR_FORMATS
    } GR_FORMAT;

    typedef uint8_t* gw_image_bits;

    struct RGBA {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;
    };

    union DXT1Color {
        struct {
            unsigned b1 : 5, g1 : 6, r1 : 5;
            unsigned b2 : 5, g2 : 6, r2 : 5;
        };
        struct {
            unsigned short c1, c2;
        };
    };

    static uint8_t Expand5To8(unsigned value) {
        return static_cast<uint8_t>((value << 3) | (value >> 2));
    }

    static uint8_t Expand6To8(unsigned value) {
        return static_cast<uint8_t>((value << 2) | (value >> 4));
    }

    struct DXT5Alpha {
        uint8_t a0 = 0;
        uint8_t a1 = 0;
        int64_t table = 0;
    };

    typedef RecObj*(__cdecl* FileIdToRecObj_pt)(wchar_t* fileHash, int unk1_1, int unk2_0);
    static FileIdToRecObj_pt FileHashToRecObj_func = nullptr;

    typedef uint8_t*(__cdecl* GetRecObjectBytes_pt)(RecObj* rec, int* size_out);
    static GetRecObjectBytes_pt ReadFileBuffer_Func = nullptr;

    typedef uint32_t(__cdecl* DecodeImage_pt)(int size, uint8_t* bytes, gw_image_bits* bits, uint8_t* pallete, GR_FORMAT* format, Vec2i* dims, int* levels);
    static DecodeImage_pt DecodeImage_func = nullptr;

    typedef void(__cdecl* UnkRecObjBytes_pt)(RecObj* rec, uint8_t* bytes);
    static UnkRecObjBytes_pt FreeFileBuffer_Func = nullptr;

    typedef void(__cdecl* CloseRecObj_pt)(RecObj* rec);
    static CloseRecObj_pt CloseRecObj_func = nullptr;

    typedef gw_image_bits(__cdecl* AllocateImage_pt)(GR_FORMAT format, Vec2i* destDims, uint32_t levels, uint32_t unk2);
    static AllocateImage_pt AllocateImage_func = nullptr;

    typedef void(__cdecl* Depalletize_pt)(
        gw_image_bits destBits, uint8_t* destPalette, GR_FORMAT destFormat, int* destMipWidths,
        gw_image_bits sourceBits, uint8_t* sourcePallete, GR_FORMAT sourceFormat, int* sourceMipWidths,
        Vec2i* sourceDims, uint32_t sourceLevels, uint32_t unk1_0, int* unk2_0);
    static Depalletize_pt Depalletize_func = nullptr;

    std::vector<RGBA> DecodeDXT1(const uint8_t* data, int width, int height) {
        const auto* d = reinterpret_cast<const uint32_t*>(data);
        std::vector<RGBA> image(width * height);

        int p = 0;
        for (int y = 0; y < height / 4; ++y) {
            for (int x = 0; x < width / 4; ++x, ++p) {
                const DXT1Color color = *reinterpret_cast<const DXT1Color*>(&d[p * 2]);
                uint32_t block = d[p * 2 + 1];
                RGBA table[4]{};
                table[0] = { Expand5To8(color.r1), Expand6To8(color.g1), Expand5To8(color.b1), 255 };
                table[1] = { Expand5To8(color.r2), Expand6To8(color.g2), Expand5To8(color.b2), 255 };

                if (color.c1 > color.c2) {
                    table[2] = {
                        static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                        static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                        static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                        255
                    };
                    table[3] = {
                        static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                        static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                        static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                        255
                    };
                }
                else {
                    table[2] = {
                        static_cast<uint8_t>((table[0].r + table[1].r) / 2),
                        static_cast<uint8_t>((table[0].g + table[1].g) / 2),
                        static_cast<uint8_t>((table[0].b + table[1].b) / 2),
                        255
                    };
                    table[3] = { 0, 0, 0, 0 };
                }

                for (int by = 0; by < 4; ++by) {
                    for (int bx = 0; bx < 4; ++bx) {
                        image[x * 4 + bx + (y * 4 + by) * width] = table[block & 3];
                        block >>= 2;
                    }
                }
            }
        }
        return image;
    }

    std::vector<RGBA> DecodeDXT3(const uint8_t* data, int width, int height) {
        const auto* d = reinterpret_cast<const uint32_t*>(data);
        std::vector<RGBA> image(width * height);

        int p = 0;
        for (int y = 0; y < height / 4; ++y) {
            for (int x = 0; x < width / 4; ++x, ++p) {
                const int64_t alpha = reinterpret_cast<const int64_t*>(d)[p * 2];
                const DXT1Color color = *reinterpret_cast<const DXT1Color*>(&d[p * 4 + 2]);
                uint32_t block = d[p * 4 + 3];
                int64_t alpha_bits = alpha;

                RGBA table[4]{};
                table[0] = { Expand5To8(color.r1), Expand6To8(color.g1), Expand5To8(color.b1), 255 };
                table[1] = { Expand5To8(color.r2), Expand6To8(color.g2), Expand5To8(color.b2), 255 };
                table[2] = {
                    static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                    static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                    static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                    255
                };
                table[3] = {
                    static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                    static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                    static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                    255
                };

                for (int by = 0; by < 4; ++by) {
                    for (int bx = 0; bx < 4; ++bx) {
                        RGBA pixel = table[block & 3];
                        block >>= 2;
                        pixel.a = static_cast<uint8_t>((alpha_bits & 0xF) << 4);
                        alpha_bits >>= 4;
                        image[x * 4 + bx + (y * 4 + by) * width] = pixel;
                    }
                }
            }
        }
        return image;
    }

    std::vector<RGBA> DecodeDXT5(const uint8_t* data, int width, int height, bool premultiply_alpha) {
        const auto* d = reinterpret_cast<const uint32_t*>(data);
        std::vector<RGBA> image(width * height);

        int p = 0;
        for (int y = 0; y < height / 4; ++y) {
            for (int x = 0; x < width / 4; ++x, ++p) {
                const DXT5Alpha alpha = *reinterpret_cast<const DXT5Alpha*>(&reinterpret_cast<const int64_t*>(d)[p * 2]);
                const DXT1Color color = *reinterpret_cast<const DXT1Color*>(&d[p * 4 + 2]);
                uint32_t block = d[p * 4 + 3];
                int64_t alpha_bits = alpha.table;

                uint8_t alpha_table[8]{};
                alpha_table[0] = alpha.a0;
                alpha_table[1] = alpha.a1;
                if (alpha.a0 > alpha.a1) {
                    for (int i = 0; i < 6; ++i)
                        alpha_table[i + 2] = static_cast<uint8_t>(((6 - i) * alpha.a0 + (i + 1) * alpha.a1) / 7);
                }
                else {
                    for (int i = 0; i < 4; ++i)
                        alpha_table[i + 2] = static_cast<uint8_t>(((4 - i) * alpha.a0 + (i + 1) * alpha.a1) / 5);
                    alpha_table[6] = 0;
                    alpha_table[7] = 255;
                }

                RGBA table[4]{};
                table[0] = { Expand5To8(color.r1), Expand6To8(color.g1), Expand5To8(color.b1), 255 };
                table[1] = { Expand5To8(color.r2), Expand6To8(color.g2), Expand5To8(color.b2), 255 };
                table[2] = {
                    static_cast<uint8_t>((table[0].r * 2 + table[1].r) / 3),
                    static_cast<uint8_t>((table[0].g * 2 + table[1].g) / 3),
                    static_cast<uint8_t>((table[0].b * 2 + table[1].b) / 3),
                    255
                };
                table[3] = {
                    static_cast<uint8_t>((table[0].r + table[1].r * 2) / 3),
                    static_cast<uint8_t>((table[0].g + table[1].g * 2) / 3),
                    static_cast<uint8_t>((table[0].b + table[1].b * 2) / 3),
                    255
                };

                for (int by = 0; by < 4; ++by) {
                    for (int bx = 0; bx < 4; ++bx) {
                        RGBA pixel = table[block & 3];
                        block >>= 2;
                        pixel.a = alpha_table[alpha_bits & 7];
                        alpha_bits >>= 3;
                        if (premultiply_alpha) {
                            pixel.r = static_cast<uint8_t>((pixel.r * pixel.a) / 255);
                            pixel.g = static_cast<uint8_t>((pixel.g * pixel.a) / 255);
                            pixel.b = static_cast<uint8_t>((pixel.b * pixel.a) / 255);
                        }
                        image[x * 4 + bx + (y * 4 + by) * width] = pixel;
                    }
                }
            }
        }
        return image;
    }

    uint32_t GetAtexFormat(GR_FORMAT source_format) {
        switch (source_format) {
        case GR_FORMAT_DXT1:
            return 0x0F;
        case GR_FORMAT_DXT2:
        case GR_FORMAT_DXT3:
        case GR_FORMAT_DXTN:
            return 0x11;
        case GR_FORMAT_DXT4:
        case GR_FORMAT_DXT5:
        case GR_FORMAT_DXTA:
            return 0x13;
        case GR_FORMAT_DXTL:
            return 0x12;
        default:
            return 0;
        }
    }

    bool DecodeAtexFormatToArgb(
        const uint8_t* image_bytes, int image_size, GR_FORMAT source_format, const Vec2i& dims,
        gw_image_bits* dest_bits, AllocateImage_pt allocate_image) {
        if (!image_bytes || image_size <= 0 || !dest_bits || !allocate_image || dims.x <= 0 || dims.y <= 0 || (dims.x % 4) != 0 || (dims.y % 4) != 0) {
            return false;
        }

        const uint32_t atex_format = GetAtexFormat(source_format);
        if (!atex_format) {
            return false;
        }

        std::vector<uint32_t> intermediate(static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y), 0);
        SImageDescriptor descriptor{};
        descriptor.xres = dims.x;
        descriptor.yres = dims.y;
        descriptor.Data = const_cast<unsigned char*>(image_bytes);
        descriptor.a = image_size;
        descriptor.b = 6;
        descriptor.image = reinterpret_cast<unsigned char*>(intermediate.data());
        descriptor.imageformat = 0x0F;
        descriptor.c = 0;

        AtexDecompress(
            reinterpret_cast<unsigned int*>(const_cast<uint8_t*>(image_bytes)),
            static_cast<unsigned int>(image_size),
            atex_format,
            descriptor,
            intermediate.data());

        std::vector<RGBA> rgba;
        switch (source_format) {
        case GR_FORMAT_DXT1:
            rgba = DecodeDXT1(reinterpret_cast<const uint8_t*>(intermediate.data()), dims.x, dims.y);
            break;
        case GR_FORMAT_DXT2:
        case GR_FORMAT_DXT3:
        case GR_FORMAT_DXTN:
            rgba = DecodeDXT3(reinterpret_cast<const uint8_t*>(intermediate.data()), dims.x, dims.y);
            break;
        case GR_FORMAT_DXT4:
        case GR_FORMAT_DXT5:
        case GR_FORMAT_DXTA:
            rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(intermediate.data()), dims.x, dims.y, false);
            break;
        case GR_FORMAT_DXTL:
            rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(intermediate.data()), dims.x, dims.y, true);
            break;
        default:
            return false;
        }

        *dest_bits = allocate_image(GR_FORMAT_A8R8G8B8, const_cast<Vec2i*>(&dims), 1, 0);
        if (!*dest_bits) {
            return false;
        }
        auto* out = reinterpret_cast<uint8_t*>(*dest_bits);
        for (size_t i = 0; i < rgba.size(); ++i) {
            out[i * 4 + 0] = rgba[i].b;
            out[i * 4 + 1] = rgba[i].g;
            out[i * 4 + 2] = rgba[i].r;
            out[i * 4 + 3] = rgba[i].a;
        }
        return true;
    }

    void FileIdToFileHash(uint32_t file_id, wchar_t* fileHash) {
        fileHash[0] = static_cast<wchar_t>(((file_id - 1) % 0xff00) + 0x100);
        fileHash[1] = static_cast<wchar_t>(((file_id - 1) / 0xff00) + 0x100);
        fileHash[2] = 0;
    }

    const char* strnstr(char* str, const char* substr, size_t n) {
        char* p = str;
        char* p_end = str + n;
        const size_t substr_len = strlen(substr);

        if (substr_len == 0)
            return str;

        p_end -= (substr_len - 1);
        for (; p < p_end; ++p) {
            if (strncmp(p, substr, substr_len) == 0)
                return p;
        }
        return nullptr;
    }

    uint32_t OpenImage(uint32_t file_id, gw_image_bits* dst_bits, Vec2i& dims, int& levels, GR_FORMAT& format) {
        int size = 0;
        uint8_t* pallete = nullptr;
        gw_image_bits bits = nullptr;

        wchar_t fileHash[4] = { 0 };
        FileIdToFileHash(file_id, fileHash);

        auto rec = FileHashToRecObj_func(fileHash, 1, 0);
        if (!rec) {
            Logger::Instance().LogError("GwDatTextureManager: FileHashToRecObj returned null for file_id " + std::to_string(file_id));
            return 0;
        }

        const auto bytes = ReadFileBuffer_Func(rec, &size);
        if (!bytes) {
            Logger::Instance().LogError("GwDatTextureManager: ReadFileBuffer_Func returned null for file_id " + std::to_string(file_id));
            CloseRecObj_func(rec);
            return 0;
        }

        int image_size = size;
        auto image_bytes = bytes;
        if (memcmp((char*)bytes, "ffna", 4) == 0) {
            const auto found = strnstr((char*)bytes, "ATEX", size);
            if (!found) {
                Logger::Instance().LogError("GwDatTextureManager: FFNA payload missing ATEX chunk for file_id " + std::to_string(file_id));
                FreeFileBuffer_Func(rec, bytes);
                CloseRecObj_func(rec);
                return 0;
            }
            image_bytes = (uint8_t*)found;
            image_size = *(int*)(found - 4);
        }

        uint32_t result = DecodeImage_func(image_size, image_bytes, &bits, pallete, &format, &dims, &levels);
        if (rec) {
            FreeFileBuffer_Func(rec, bytes);
            if (levels > 13) {
                Logger::Instance().LogError("GwDatTextureManager: DecodeImage returned invalid mip level count for file_id " + std::to_string(file_id));
                return 0;
            }
            CloseRecObj_func(rec);
        }

        if (format >= GR_FORMATS || !result) {
            Logger::Instance().LogError(
                "GwDatTextureManager: DecodeImage failed for file_id " + std::to_string(file_id) +
                " result=" + std::to_string(result) +
                " format=" + std::to_string(static_cast<uint32_t>(format)) +
                " levels=" + std::to_string(levels));
            if (bits) {
                GW::MemoryMgr::MemFree(bits);
            }
            return 0;
        }

        levels = 1;
        if (format == GR_FORMAT_A8R8G8B8) {
            *dst_bits = bits;
            return result;
        }

        if (!Depalletize_func) {
            if (!DecodeAtexFormatToArgb(image_bytes, image_size, format, dims, dst_bits, AllocateImage_func)) {
                Logger::Instance().LogError(
                    "GwDatTextureManager: Depalletize_func is unavailable and decoded format "
                    + std::to_string(static_cast<uint32_t>(format)) +
                    " requires conversion for file_id " + std::to_string(file_id));
                GW::MemoryMgr::MemFree(bits);
                return 0;
            }
            GW::MemoryMgr::MemFree(bits);
            return result;
        }

        *dst_bits = AllocateImage_func(GR_FORMAT_A8R8G8B8, &dims, levels, 0);
        if (!*dst_bits) {
            Logger::Instance().LogError("GwDatTextureManager: AllocateImage_func returned null for file_id " + std::to_string(file_id));
            GW::MemoryMgr::MemFree(bits);
            return 0;
        }

        Depalletize_func((gw_image_bits)dst_bits, nullptr, GR_FORMAT_A8R8G8B8, nullptr, bits, pallete, format, nullptr, &dims, levels, 0, 0);
        GW::MemoryMgr::MemFree(bits);
        return result;
    }

    IDirect3DTexture9* CreateTexture(IDirect3DDevice9* device, uint32_t file_id, Vec2i& dims) {
        if (!device || !file_id) {
            return nullptr;
        }

        gw_image_bits bits = nullptr;
        int levels = 0;
        GR_FORMAT format = GR_FORMAT_A8R8G8B8;
        const auto ret = OpenImage(file_id, &bits, dims, levels, format);
        if (!ret || !bits || !dims.x || !dims.y) {
            if (bits) {
                GW::MemoryMgr::MemFree(bits);
            }
            Logger::Instance().LogError("GwDatTextureManager: OpenImage failed for file_id " + std::to_string(file_id));
            return nullptr;
        }

        IDirect3DTexture9* tex = nullptr;
        if (device->CreateTexture(dims.x, dims.y, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK) {
            Logger::Instance().LogError("GwDatTextureManager: CreateTexture failed for file_id " + std::to_string(file_id));
            GW::MemoryMgr::MemFree(bits);
            return nullptr;
        }

        D3DLOCKED_RECT rect;
        if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK) {
            Logger::Instance().LogError("GwDatTextureManager: LockRect failed for file_id " + std::to_string(file_id));
            tex->Release();
            GW::MemoryMgr::MemFree(bits);
            return nullptr;
        }

        unsigned int* srcdata = (unsigned int*)bits;
        for (int y = 0; y < dims.y; y++) {
            uint8_t* destAddr = ((uint8_t*)rect.pBits + y * rect.Pitch);
            memcpy(destAddr, srcdata, dims.x * 4);
            srcdata += dims.x;
        }
        GW::MemoryMgr::MemFree(bits);
        tex->UnlockRect(0);
        return tex;
    }

    struct GwImg {
        uint32_t m_file_id = 0;
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
        std::chrono::steady_clock::time_point last_used = std::chrono::steady_clock::now();

        explicit GwImg(uint32_t file_id)
            : m_file_id(file_id) {
        }

        ~GwImg() {
            if (m_tex) {
                m_tex->Release();
                m_tex = nullptr;
            }
        }

        void Touch() {
            last_used = std::chrono::steady_clock::now();
        }
    };

    static std::map<uint32_t, GwImg*> textures_by_file_id;
}

void GwDatTextureManager::SetDevice(IDirect3DDevice9* device) {
    d3d_device_ = device;
}

bool GwDatTextureManager::IsDatTextureKey(const std::wstring& texture_key) {
    constexpr std::wstring_view prefix = L"gwdat://";
    return texture_key.size() > prefix.size() && texture_key.rfind(prefix.data(), 0) == 0;
}

uint32_t GwDatTextureManager::ParseFileId(const std::wstring& texture_key) {
    if (!IsDatTextureKey(texture_key)) {
        return 0;
    }
    try {
        return static_cast<uint32_t>(std::stoul(texture_key.substr(8)));
    }
    catch (...) {
        return 0;
    }
}

bool GwDatTextureManager::EnsureHooks() {
    if (hooks_initialized_) {
        return hooks_ready_;
    }

    hooks_initialized_ = true;

    using namespace GW;
    uintptr_t address = 0;

    DecodeImage_func = (DecodeImage_pt)Scanner::ToFunctionStart(Scanner::FindAssertion("GrImage.cpp", "bits || !palette", 0, 0));
    Logger::AssertAddress("DecodeImage_func", reinterpret_cast<uintptr_t>(DecodeImage_func), "GwDatTextureManager");

    address = Scanner::FindAssertion("Amet.cpp", "data", 0, 0);
    if (address) {
        address = Scanner::FindInRange("\xe8", "x", 0, address + 0xc, address + 0xff);
        FileHashToRecObj_func = (FileIdToRecObj_pt)Scanner::FunctionFromNearCall(address);
        address = Scanner::FindInRange("\xe8", "x", 0, address + 1, address + 0xff);
        ReadFileBuffer_Func = (GetRecObjectBytes_pt)Scanner::FunctionFromNearCall(address);
    }
    Logger::AssertAddress("FileHashToRecObj_func", reinterpret_cast<uintptr_t>(FileHashToRecObj_func), "GwDatTextureManager");
    Logger::AssertAddress("ReadFileBuffer_Func", reinterpret_cast<uintptr_t>(ReadFileBuffer_Func), "GwDatTextureManager");

    address = Scanner::Find("\x81\x3a\x41\x4d\x45\x54", "xxxxxx");
    if (address) {
        address = Scanner::FindInRange("\xe8", "x", 0, address, address - 0xff);
        CloseRecObj_func = (CloseRecObj_pt)Scanner::FunctionFromNearCall(address);
        address = Scanner::FindInRange("\xe8", "x", 0, address - 1, address - 0xff);
        FreeFileBuffer_Func = (UnkRecObjBytes_pt)Scanner::FunctionFromNearCall(address);
    }
    Logger::AssertAddress("CloseRecObj_func", reinterpret_cast<uintptr_t>(CloseRecObj_func), "GwDatTextureManager");
    Logger::AssertAddress("FreeFileBuffer_Func", reinterpret_cast<uintptr_t>(FreeFileBuffer_Func), "GwDatTextureManager");

    AllocateImage_func = (AllocateImage_pt)Scanner::ToFunctionStart(Scanner::Find("\x7c\x11\x6a\x5c", "xxxx"));
    Logger::AssertAddress("AllocateImage_func", reinterpret_cast<uintptr_t>(AllocateImage_func), "GwDatTextureManager");

    Depalletize_func = (Depalletize_pt)Scanner::ToFunctionStart(Scanner::FindNthUseOfString("destPalette", 1));
    Logger::AssertAddress("Depalletize_func", reinterpret_cast<uintptr_t>(Depalletize_func), "GwDatTextureManager");

    hooks_ready_ = FileHashToRecObj_func
        && ReadFileBuffer_Func
        && DecodeImage_func
        && FreeFileBuffer_Func
        && CloseRecObj_func
        && AllocateImage_func;

    if (hooks_ready_) {
        if (Depalletize_func) {
            Logger::Instance().LogInfo("GwDatTextureManager: Toolbox-style hooks resolved successfully");
        }
        else {
            Logger::Instance().LogWarning("GwDatTextureManager: Depalletize_func is missing; only natively A8R8G8B8 decoded textures will load");
        }
    }
    else {
        Logger::Instance().LogError("GwDatTextureManager: one or more Toolbox-style hooks failed to resolve");
    }

    return hooks_ready_;
}

IDirect3DTexture9* GwDatTextureManager::LoadTextureFromFileId(uint32_t file_id) {
    if (!d3d_device_ && g_d3d_device) {
        d3d_device_ = g_d3d_device;
    }
    if (!d3d_device_ || !EnsureHooks() || !file_id) {
        Logger::Instance().LogError("GwDatTextureManager: cannot load file_id " + std::to_string(file_id) + " because device, hooks, or file id are unavailable");
        return nullptr;
    }

    auto found = textures_by_file_id.find(file_id);
    if (found != textures_by_file_id.end()) {
        found->second->Touch();
        if (!found->second->m_tex) {
            found->second->m_tex = CreateTexture(d3d_device_, found->second->m_file_id, found->second->m_dims);
        }
        return found->second->m_tex;
    }

    auto* gwimg_ptr = new GwImg(file_id);
    textures_by_file_id[file_id] = gwimg_ptr;
    gwimg_ptr->m_tex = CreateTexture(d3d_device_, gwimg_ptr->m_file_id, gwimg_ptr->m_dims);
    return gwimg_ptr->m_tex;
}

IDirect3DTexture9* GwDatTextureManager::GetTexture(const std::wstring& texture_key) {
    return GetTextureByFileId(ParseFileId(texture_key));
}

IDirect3DTexture9* GwDatTextureManager::GetTextureByFileId(uint32_t file_id) {
    return LoadTextureFromFileId(file_id);
}

void GwDatTextureManager::CleanupOldTextures(int timeout_seconds) {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = textures_by_file_id.begin(); it != textures_by_file_id.end();) {
        GwImg* gwimg_ptr = it->second;
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - gwimg_ptr->last_used).count();
        if (age > timeout_seconds) {
            delete gwimg_ptr;
            it = textures_by_file_id.erase(it);
        }
        else {
            ++it;
        }
    }
}
