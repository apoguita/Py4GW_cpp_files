#include "Headers.h"
#include "AtexAsm.h"
#include "ArenaNetFileParser.h"
#include "GwDatTextureManager.h"
#include "Resources.h"

#include <functional>
#include <memory>
#include <queue>

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

    std::vector<RGBA> DecodeDXT1(const uint8_t* data, int width, int height);
    std::vector<RGBA> DecodeDXT3(const uint8_t* data, int width, int height);
    std::vector<RGBA> DecodeDXT5(const uint8_t* data, int width, int height, bool premultiply_alpha);

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

    typedef RecObj*(__cdecl* OpenFileByFileId_pt)(uint32_t archive, uint32_t file_id, uint32_t stream_id, uint32_t flags, uint32_t* error_out);
    static OpenFileByFileId_pt OpenFileByFileId_func = nullptr;

    typedef RecObj*(__cdecl* FileIdToRecObj_pt)(const wchar_t* fileHash, int unk1_1, int unk2_0);
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

    void LogTextureStage(uint32_t file_id, const char* stage) {
        UNREFERENCED_PARAMETER(file_id);
        UNREFERENCED_PARAMETER(stage);
    }

    void LogTextureStage(uint32_t file_id, const std::string& stage) {
        UNREFERENCED_PARAMETER(file_id);
        UNREFERENCED_PARAMETER(stage);
    }

    bool CopyBytesNoFault(const void* src, size_t size, std::vector<uint8_t>& out) {
        if (!src || !size) {
            return false;
        }

        out.resize(size);
        __try {
            memcpy(out.data(), src, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            out.clear();
            return false;
        }
    }

    void* ReadPointerNoFault(const void* address) {
        void* value = nullptr;
        __try {
            value = *reinterpret_cast<void* const*>(address);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            value = nullptr;
        }
        return value;
    }

    bool CopyBytesNoFault(const void* src, void* dst, size_t size) {
        if (!src || !dst || !size) {
            return false;
        }

        __try {
            memcpy(dst, src, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    gw_image_bits AllocateImageNoFault(GR_FORMAT format, Vec2i* dims, uint32_t levels, uint32_t unk2) {
        __try {
            return AllocateImage_func(format, dims, levels, unk2);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    bool AtexDecompressNoFault(unsigned int* input, unsigned int size, unsigned int image_format, SImageDescriptor descriptor, unsigned int* output) {
        __try {
            AtexDecompress(input, size, image_format, descriptor, output);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    RecObj* OpenFileByFileIdNoFault(uint32_t archive, uint32_t file_id, uint32_t stream_id, uint32_t flags, uint32_t* error_out) {
        __try {
            return OpenFileByFileId_func ? OpenFileByFileId_func(archive, file_id, stream_id, flags, error_out) : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    RecObj* FileHashToRecObjNoFault(const wchar_t* file_hash, int unk1_1, int unk2_0) {
        __try {
            return FileHashToRecObj_func ? FileHashToRecObj_func(file_hash, unk1_1, unk2_0) : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    uint8_t* ReadFileBufferNoFault(RecObj* rec, int* size_out) {
        __try {
            return ReadFileBuffer_Func ? ReadFileBuffer_Func(rec, size_out) : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    bool FreeFileBufferNoFault(RecObj* rec, uint8_t* bytes) {
        __try {
            if (FreeFileBuffer_Func) {
                FreeFileBuffer_Func(rec, bytes);
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        return false;
    }

    bool CloseRecObjNoFault(RecObj* rec) {
        __try {
            if (CloseRecObj_func) {
                CloseRecObj_func(rec);
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        return false;
    }

    bool DecodeAtexToBgra(uint32_t file_id, uint8_t* image_bytes, size_t image_size, std::vector<uint8_t>* dst_pixels, Vec2i& dims, int& levels, GR_FORMAT& format) {
        if (!image_bytes || image_size < 12 || !dst_pixels) {
            LogTextureStage(file_id, "DecodeAtexToBgra: invalid input");
            return false;
        }

        const uint32_t id1 = reinterpret_cast<uint32_t*>(image_bytes)[0];
        const uint32_t id2 = reinterpret_cast<uint32_t*>(image_bytes)[1];
        if (id1 != 'XTTA' && id1 != 'XETA') {
            LogTextureStage(file_id, "DecodeAtexToBgra: not ATEX/ATTX");
            return false;
        }
        if ((id2 & 0xffffff) != 'TXD') {
            LogTextureStage(file_id, "DecodeAtexToBgra: not DXT compressed");
            return false;
        }

        const int compression_type = id2 >> 24;
        dims.x = *reinterpret_cast<uint16_t*>(image_bytes + 8);
        dims.y = *reinterpret_cast<uint16_t*>(image_bytes + 10);
        levels = 1;

        if (dims.x <= 0 || dims.y <= 0 || (dims.x % 4) != 0 || (dims.y % 4) != 0) {
            LogTextureStage(file_id, "DecodeAtexToBgra: invalid dims " + std::to_string(dims.x) + "x" + std::to_string(dims.y));
            return false;
        }

        uint32_t atex_format = 0;
        bool premultiply_alpha = false;
        switch (compression_type) {
        case '1':
            format = GR_FORMAT_DXT1;
            atex_format = 0x0f;
            break;
        case '2':
        case '3':
        case 'N':
            format = compression_type == 'N' ? GR_FORMAT_DXTN : GR_FORMAT_DXT3;
            atex_format = 0x11;
            break;
        case '4':
        case '5':
        case 'A':
            format = compression_type == 'A' ? GR_FORMAT_DXTA : GR_FORMAT_DXT5;
            atex_format = 0x13;
            break;
        case 'L':
            format = GR_FORMAT_DXTL;
            atex_format = 0x12;
            premultiply_alpha = true;
            break;
        default:
            LogTextureStage(file_id, "DecodeAtexToBgra: unsupported compression type " + std::to_string(compression_type));
            return false;
        }

        LogTextureStage(file_id,
            "DecodeAtexToBgra: begin dims=" + std::to_string(dims.x) + "x" + std::to_string(dims.y)
            + " compression=" + std::to_string(compression_type)
            + " atex_format=" + std::to_string(atex_format));

        std::vector<uint32_t> dxt_intermediate(static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y), 0);
        SImageDescriptor descriptor{};
        descriptor.xres = dims.x;
        descriptor.yres = dims.y;
        descriptor.Data = image_bytes;
        descriptor.a = static_cast<int>(image_size);
        descriptor.b = 6;
        descriptor.image = reinterpret_cast<unsigned char*>(dxt_intermediate.data());
        descriptor.imageformat = 0x0f;
        descriptor.c = 0;

        if (!AtexDecompressNoFault(
            reinterpret_cast<unsigned int*>(image_bytes),
            static_cast<unsigned int>(image_size),
            atex_format,
            descriptor,
            dxt_intermediate.data())) {
            LogTextureStage(file_id, "DecodeAtexToBgra: AtexDecompress fault");
            return false;
        }
        LogTextureStage(file_id, "DecodeAtexToBgra: AtexDecompress ok");

        std::vector<RGBA> rgba;
        switch (format) {
        case GR_FORMAT_DXT1:
            rgba = DecodeDXT1(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y);
            break;
        case GR_FORMAT_DXT2:
        case GR_FORMAT_DXT3:
        case GR_FORMAT_DXTN:
            rgba = DecodeDXT3(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y);
            break;
        case GR_FORMAT_DXT4:
        case GR_FORMAT_DXT5:
        case GR_FORMAT_DXTA:
            rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y, false);
            break;
        case GR_FORMAT_DXTL:
            rgba = DecodeDXT5(reinterpret_cast<const uint8_t*>(dxt_intermediate.data()), dims.x, dims.y, premultiply_alpha);
            break;
        default:
            return false;
        }

        if (rgba.size() != static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y)) {
            LogTextureStage(file_id, "DecodeAtexToBgra: unexpected decoded pixel count");
            return false;
        }

        dst_pixels->resize(rgba.size() * 4);
        for (size_t i = 0; i < rgba.size(); ++i) {
            (*dst_pixels)[i * 4 + 0] = rgba[i].b;
            (*dst_pixels)[i * 4 + 1] = rgba[i].g;
            (*dst_pixels)[i * 4 + 2] = rgba[i].r;
            (*dst_pixels)[i * 4 + 3] = rgba[i].a;
        }
        LogTextureStage(file_id, "DecodeAtexToBgra: done bytes=" + std::to_string(dst_pixels->size()));
        return true;
    }

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

    uint32_t OpenImage(uint32_t file_id, std::vector<uint8_t>* dst_pixels, Vec2i& dims, int& levels, GR_FORMAT& format) {
        LogTextureStage(file_id, "OpenImage: readFromDat begin");
        ArenaNetFileParser::GameAssetFile asset;
        if (!asset.readFromDat(file_id)) {
            LogTextureStage(file_id, "OpenImage: readFromDat failed");
            return 0;
        }
        LogTextureStage(file_id, "OpenImage: readFromDat ok bytes=" + std::to_string(asset.data.size()));

        uint8_t* image_bytes = asset.data.data();
        size_t image_size = asset.data.size();
        if (strncmp(reinterpret_cast<char*>(image_bytes), "ffna", 4) == 0) {
            LogTextureStage(file_id, "OpenImage: FFNA chunk lookup begin");
            const auto anet_file = (ArenaNetFileParser::ArenaNetFile*)&asset;
            if (!anet_file->isValid()) {
                LogTextureStage(file_id, "OpenImage: FFNA invalid");
                return 0;
            }
            const auto chunk = (ArenaNetFileParser::UnknownChunk*)anet_file->FindChunk(ArenaNetFileParser::ChunkType::FA3_InlineTextureDXT3);
            if (!chunk) {
                LogTextureStage(file_id, "OpenImage: FFNA inline texture chunk missing");
                return 0;
            }

            image_bytes = chunk->data;
            image_size = chunk->chunk_size;
            LogTextureStage(file_id, "OpenImage: FFNA chunk ok size=" + std::to_string(image_size));
        }

        if (strncmp((char*)image_bytes, "ATEX", 4) != 0
            && strncmp((char*)image_bytes, "DDS", 3) != 0) {
            LogTextureStage(file_id, "OpenImage: payload not ATEX/DDS");
            return 0;
        }

        if (strncmp((char*)image_bytes, "DDS", 3) == 0) {
            LogTextureStage(file_id, "OpenImage: DDS payload unsupported by custom decoder");
            return 0;
        }

        return DecodeAtexToBgra(file_id, image_bytes, image_size, dst_pixels, dims, levels, format) ? 1 : 0;
    }

    struct DecodedTexture {
        Vec2i dims;
        int levels = 1;
        std::vector<uint8_t> pixels;
    };

    std::shared_ptr<DecodedTexture> DecodeTexture(uint32_t file_id) {
        auto decoded = std::make_shared<DecodedTexture>();

        if (!file_id) {
            return nullptr;
        }

        LogTextureStage(file_id, "CPU job: decode begin");
        int levels;
        GR_FORMAT format;
        auto ret = OpenImage(file_id, &decoded->pixels, decoded->dims, levels, format);
        if (!ret || decoded->pixels.empty() || !decoded->dims.x || !decoded->dims.y) {
            LogTextureStage(file_id,
                "CPU job: OpenImage failed ret=" + std::to_string(ret)
                + " pixels=" + std::to_string(decoded->pixels.size())
                + " dims=" + std::to_string(decoded->dims.x) + "x" + std::to_string(decoded->dims.y));
            return nullptr;
        }

        LogTextureStage(file_id,
            "CPU job: OpenImage ok dims=" + std::to_string(decoded->dims.x) + "x" + std::to_string(decoded->dims.y)
            + " levels=" + std::to_string(levels));
        decoded->levels = levels;
        LogTextureStage(file_id, "CPU job: decode done");
        return decoded;
    }

    IDirect3DTexture9* CreateTexture(IDirect3DDevice9* device, uint32_t file_id, const DecodedTexture& decoded) {
        if (!device || decoded.pixels.empty() || !decoded.dims.x || !decoded.dims.y) {
            LogTextureStage(file_id, "DX job: invalid decoded texture/device");
            return nullptr;
        }

        IDirect3DTexture9* tex = nullptr;
        LogTextureStage(file_id,
            "DX job: CreateTexture begin dims=" + std::to_string(decoded.dims.x) + "x" + std::to_string(decoded.dims.y)
            + " levels=" + std::to_string(decoded.levels));
        if (device->CreateTexture(decoded.dims.x, decoded.dims.y, decoded.levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK) {
            LogTextureStage(file_id, "DX job: CreateTexture failed");
            return nullptr;
        }
        LogTextureStage(file_id, "DX job: CreateTexture ok tex=" + std::to_string(reinterpret_cast<uintptr_t>(tex)));

        D3DLOCKED_RECT rect;
        LogTextureStage(file_id, "DX job: LockRect begin");
        if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK) {
            LogTextureStage(file_id, "DX job: LockRect failed");
            tex->Release();
            return nullptr;
        }
        LogTextureStage(file_id, "DX job: LockRect ok pitch=" + std::to_string(rect.Pitch));

        LogTextureStage(file_id, "DX job: upload begin");
        const uint8_t* srcdata = decoded.pixels.data();
        for (int y = 0; y < decoded.dims.y; y++) {
            uint8_t* destAddr = ((uint8_t*)rect.pBits + y * rect.Pitch);
            memcpy(destAddr, srcdata, decoded.dims.x * 4);
            srcdata += static_cast<size_t>(decoded.dims.x) * 4;
        }

        tex->UnlockRect(0);
        LogTextureStage(file_id, "DX job: upload done");
        return tex;
    }

    struct GwImg {
        uint32_t m_file_id = 0;
        Vec2i m_dims;
        IDirect3DTexture9* m_tex = nullptr;
        std::chrono::steady_clock::time_point m_last_used;
        bool m_evicted = false;

        explicit GwImg(uint32_t file_id)
            : m_file_id(file_id)
            , m_last_used(std::chrono::steady_clock::now()) {
        }

        void Touch() {
            m_last_used = std::chrono::steady_clock::now();
        }
    };

    static std::map<uint32_t, std::shared_ptr<GwImg>> textures_by_file_id;
    static std::recursive_mutex textures_mutex;
    static std::recursive_mutex cpu_jobs_mutex;
    static std::queue<std::function<void()>> cpu_jobs;

    bool IsDatTextureLoadSafe(IDirect3DDevice9* device) {
        if (!device || device->TestCooperativeLevel() != D3D_OK) {
            return false;
        }
        if (GW::GetPreGameContext()) {
            return false;
        }
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
            return false;
        }
        if (GW::Map::GetIsInCinematic()) {
            return false;
        }
        if (!GW::UI::GetIsUIDrawn()) {
            return false;
        }
        return true;
    }

    void EnqueueCpuTask(const std::function<void()>& task) {
        std::lock_guard<std::recursive_mutex> lock(cpu_jobs_mutex);
        cpu_jobs.push(task);
    }
}

void GwDatTextureManager::SetDevice(IDirect3DDevice9* device) {
    d3d_device_ = device;
}

void GwDatTextureManager::CpuUpdate() {
    while (true) {
        std::function<void()> task;
        {
            std::lock_guard<std::recursive_mutex> lock(cpu_jobs_mutex);
            if (cpu_jobs.empty()) {
                return;
            }
            task = std::move(cpu_jobs.front());
            cpu_jobs.pop();
        }
        task();
    }
}

void GwDatTextureManager::DxUpdate(IDirect3DDevice9* device) {
    SetDevice(device);
    Resources::DxUpdate(device);
}

GwDatTextureManager::~GwDatTextureManager() {
    std::lock_guard<std::recursive_mutex> lock(textures_mutex);
    textures_by_file_id.clear();
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

bool GwDatTextureManager::ReadDatFile(const wchar_t* file_hash, std::vector<uint8_t>* bytes_out, uint32_t stream_id) {
    const uint32_t file_id_for_log = ArenaNetFileParser::FileHashToFileId(file_hash);
    LogTextureStage(file_id_for_log, "ReadDatFile: begin stream_id=" + std::to_string(stream_id));

    if (!(file_hash && *file_hash && bytes_out && CloseRecObj_func && FileHashToRecObj_func && FreeFileBuffer_Func)) {
        LogTextureStage(file_id_for_log, "ReadDatFile: missing argument or hook");
        return false;
    }

    uint32_t file_id = file_id_for_log;
    RecObj* rec = 0;
    if (file_id && OpenFileByFileId_func) {
        LogTextureStage(file_id, "ReadDatFile: OpenFileByFileId begin func=" + std::to_string(reinterpret_cast<uintptr_t>(OpenFileByFileId_func)));
        rec = OpenFileByFileIdNoFault(0, file_id, stream_id, 1, 0);
        LogTextureStage(file_id, "ReadDatFile: OpenFileByFileId returned rec=" + std::to_string(reinterpret_cast<uintptr_t>(rec)));
    }
    if (!rec) {
        LogTextureStage(file_id, "ReadDatFile: FileHashToRecObj begin func=" + std::to_string(reinterpret_cast<uintptr_t>(FileHashToRecObj_func)));
        rec = FileHashToRecObjNoFault(file_hash, 1, 0);
        LogTextureStage(file_id, "ReadDatFile: FileHashToRecObj returned rec=" + std::to_string(reinterpret_cast<uintptr_t>(rec)));
    }
    if (!rec) {
        LogTextureStage(file_id, "ReadDatFile: no rec object");
        return false;
    }

    int size = 0;
    LogTextureStage(file_id, "ReadDatFile: ReadFileBuffer begin func=" + std::to_string(reinterpret_cast<uintptr_t>(ReadFileBuffer_Func)));
    const auto bytes = ReadFileBufferNoFault(rec, &size);
    LogTextureStage(file_id,
        "ReadDatFile: ReadFileBuffer returned bytes=" + std::to_string(reinterpret_cast<uintptr_t>(bytes))
        + " size=" + std::to_string(size));
    if (!bytes) {
        LogTextureStage(file_id, "ReadDatFile: closing rec after null bytes");
        CloseRecObjNoFault(rec);
        return false;
    }

    LogTextureStage(file_id, "ReadDatFile: copy begin size=" + std::to_string(size));
    bytes_out->resize(size);
    LogTextureStage(file_id, "ReadDatFile: resize ok");
    if (!CopyBytesNoFault(bytes, bytes_out->data(), static_cast<size_t>(size))) {
        LogTextureStage(file_id, "ReadDatFile: copy fault");
        bytes_out->clear();
        FreeFileBufferNoFault(rec, bytes);
        CloseRecObjNoFault(rec);
        return false;
    }
    LogTextureStage(file_id, "ReadDatFile: copy ok");

    LogTextureStage(file_id, "ReadDatFile: FreeFileBuffer begin");
    const bool freed = FreeFileBufferNoFault(rec, bytes);
    LogTextureStage(file_id, std::string("ReadDatFile: FreeFileBuffer ") + (freed ? "ok" : "failed"));

    LogTextureStage(file_id, "ReadDatFile: CloseRecObj begin");
    const bool closed = CloseRecObjNoFault(rec);
    LogTextureStage(file_id, std::string("ReadDatFile: CloseRecObj ") + (closed ? "ok" : "failed"));

    LogTextureStage(file_id, "ReadDatFile: done empty=" + std::to_string(bytes_out->empty()));
    return !bytes_out->empty();
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

    OpenFileByFileId_func = (OpenFileByFileId_pt)Scanner::ToFunctionStart(
        Scanner::FindAssertion("File.cpp", "!(flags & (FILE_OPEN_READ | FILE_OPEN_WRITE) & ~source.m_flags)", 0, 0),
        0xfff);
    Logger::AssertAddress("OpenFileByFileId_func", reinterpret_cast<uintptr_t>(OpenFileByFileId_func), "GwDatTextureManager");

    AllocateImage_func = (AllocateImage_pt)Scanner::ToFunctionStart(Scanner::Find("\x7c\x11\x6a\x5c", "xxxx"));
    Logger::AssertAddress("AllocateImage_func", reinterpret_cast<uintptr_t>(AllocateImage_func), "GwDatTextureManager");

    Depalletize_func = (Depalletize_pt)Scanner::ToFunctionStart(Scanner::FindNthUseOfString("destPalette", 1));
    Logger::AssertAddress("Depalletize_func", reinterpret_cast<uintptr_t>(Depalletize_func), "GwDatTextureManager");

    hooks_ready_ = FileHashToRecObj_func
        && ReadFileBuffer_Func
        && DecodeImage_func
        && FreeFileBuffer_Func
        && CloseRecObj_func
        && AllocateImage_func
        && Depalletize_func;

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

IDirect3DTexture9** GwDatTextureManager::LoadTextureFromFileId(uint32_t file_id) {
    std::shared_ptr<GwImg> gwimg_ptr;
    {
        std::lock_guard<std::recursive_mutex> lock(textures_mutex);
        auto found = textures_by_file_id.find(file_id);
        if (found != textures_by_file_id.end()) {
            found->second->Touch();
            return &found->second->m_tex;
        }
        gwimg_ptr = std::make_shared<GwImg>(file_id);
        textures_by_file_id[file_id] = gwimg_ptr;
    }
    EnqueueCpuTask([gwimg_ptr]() {
        if (gwimg_ptr->m_evicted) {
            LogTextureStage(gwimg_ptr->m_file_id, "CPU job: skipped evicted texture");
            return;
        }
        auto decoded = DecodeTexture(gwimg_ptr->m_file_id);
        if (!decoded) {
            LogTextureStage(gwimg_ptr->m_file_id, "CPU job: decoded texture null");
            return;
        }
        if (gwimg_ptr->m_evicted) {
            LogTextureStage(gwimg_ptr->m_file_id, "CPU job: decoded texture evicted before upload");
            return;
        }
        gwimg_ptr->m_dims = decoded->dims;
        LogTextureStage(gwimg_ptr->m_file_id, "CPU job: queue DX upload");
        Resources::EnqueueDxTask([gwimg_ptr, decoded](IDirect3DDevice9* device) {
            if (gwimg_ptr->m_evicted) {
                LogTextureStage(gwimg_ptr->m_file_id, "DX job: skipped evicted texture");
                return;
            }
            LogTextureStage(gwimg_ptr->m_file_id, "DX job: begin");
            gwimg_ptr->m_tex = CreateTexture(device, gwimg_ptr->m_file_id, *decoded);
            LogTextureStage(gwimg_ptr->m_file_id, "DX job: done tex=" + std::to_string(reinterpret_cast<uintptr_t>(gwimg_ptr->m_tex)));
        });
    });
    return &gwimg_ptr->m_tex;
}

IDirect3DTexture9* GwDatTextureManager::GetTexture(const std::wstring& texture_key) {
    return GetTextureByFileId(ParseFileId(texture_key));
}

IDirect3DTexture9* GwDatTextureManager::GetTextureByFileId(uint32_t file_id) {
    if (!EnsureHooks() || !file_id || !IsDatTextureLoadSafe(d3d_device_ ? d3d_device_ : g_d3d_device)) {
        return nullptr;
    }

    auto texture = LoadTextureFromFileId(file_id);
    return texture ? *texture : nullptr;
}

void GwDatTextureManager::CleanupOldTextures(int timeout_seconds) {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<GwImg>> expired;

    {
        std::lock_guard<std::recursive_mutex> lock(textures_mutex);
        for (auto it = textures_by_file_id.begin(); it != textures_by_file_id.end();) {
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->m_last_used).count();
            if (duration > timeout_seconds) {
                it->second->m_evicted = true;
                expired.push_back(it->second);
                it = textures_by_file_id.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    for (auto& gwimg_ptr : expired) {
        LogTextureStage(gwimg_ptr->m_file_id, "CleanupOldTextures: evicted");
        Resources::EnqueueDxTask([gwimg_ptr](IDirect3DDevice9*) {
            if (gwimg_ptr->m_tex) {
                LogTextureStage(gwimg_ptr->m_file_id, "CleanupOldTextures: Release begin");
                gwimg_ptr->m_tex->Release();
                gwimg_ptr->m_tex = nullptr;
                LogTextureStage(gwimg_ptr->m_file_id, "CleanupOldTextures: Release ok");
            }
        });
    }
}
