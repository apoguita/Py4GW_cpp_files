#include "Headers.h"

namespace py = pybind11;

// Game-specific lookup for bit-packed character values 0-31.
// Maps frequent characters to small indices for compression:
//   0=NUL, 1-7='0'-'6', 8-13=s,t,r,n,u,m, 14-15=(,), 16-17=[,],
//   18-19=<,>, 20-21=%,#, 22-23=/,:, 24-27=-,',",space, 28-31=,.!,\n
static const uint16_t CHAR_TABLE[32] = {
    0x0000, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036,
    0x0073, 0x0074, 0x0072, 0x006E, 0x0075, 0x006D, 0x0028, 0x0029,
    0x005B, 0x005D, 0x003C, 0x003E, 0x0025, 0x0023, 0x002F, 0x003A,
    0x002D, 0x0027, 0x0022, 0x0020, 0x002C, 0x002E, 0x0021, 0x000A,
};

// ─── Key derivation ──────────────────────────────────────────────────
// Custom game-specific key derivation, reverse-engineered from gw.exe.
// Resembles a truncated SHA-1 round (rotl by 5/30, similar mixing pattern)
// but uses non-standard constants and only ~1 round of mixing.
// Input: 8-byte key padded to 20 bytes by repeating.
// Output: 20-byte RC4 key.

static inline uint32_t rotl32(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

static void derive_rc4_key(uint64_t key_u64, uint8_t out[20]) {
    uint8_t key_bytes[8];
    memcpy(key_bytes, &key_u64, 8);

    uint8_t buf[20];
    for (int i = 0; i < 20; i++)
        buf[i] = key_bytes[i % 8];

    uint32_t w[5];
    memcpy(w, buf, 20);

    uint32_t a = w[0] + 0x9fb498b3;
    uint32_t b = w[1] + 0x66b0cd0d + rotl32(a, 5);
    uint32_t a30 = rotl32(a, 30);
    uint32_t f_a = ~(a & 0x22222222) & 0x7bf36ae2;
    uint32_t c = rotl32(b, 5) + w[2] + f_a + 0xf33d5697;
    uint32_t b30 = rotl32(b, 30);
    uint32_t g = ((a30 ^ 0x59d148c0) & b) ^ 0x59d148c0;
    uint32_t d = w[3] + rotl32(c, 5) + g + 0xd675e47b;
    uint32_t c30 = rotl32(c, 30);
    uint32_t h = ((a30 ^ b30) & c) ^ a30;
    uint32_t e = h + w[4] + rotl32(d, 5) + 0xb453c259 + w[0];

    w[2] = w[2] + c30;
    w[0] = e;
    w[1] = w[1] + d;
    w[3] = b30 + w[3];
    w[4] = a30 + w[4];

    memcpy(out, w, 20);
}

// ─── RC4 (standard KSA + PRGA) ───────────────────────────────────────

static void rc4_init(const uint8_t* key, int key_len, uint8_t s[256]) {
    for (int i = 0; i < 256; i++)
        s[i] = static_cast<uint8_t>(i);
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + s[i] + key[i % key_len]) & 0xFF;
        std::swap(s[i], s[j]);
    }
}

static void rc4_crypt(uint8_t s[256], uint8_t* data, int len) {
    int i = 0, j = 0;
    for (int n = 0; n < len; n++) {
        i = (i + 1) & 0xFF;
        j = (j + s[i]) & 0xFF;
        std::swap(s[i], s[j]);
        data[n] ^= s[(s[i] + s[j]) & 0xFF];
    }
}

// ─── Bit unpacking ───────────────────────────────────────────────────

static int bit_unpack(const uint8_t* data, int data_len,
                      uint16_t base_char, uint8_t bits_per_char,
                      uint16_t* out, int out_max) {
    if (bits_per_char == 0) return 0;

    uint32_t mask = (1u << bits_per_char) - 1;
    uint32_t bit_buf = 0;
    int bits_avail = 0;
    int byte_pos = 0;
    int count = 0;
    int max_chars = (data_len * 8) / bits_per_char;
    if (max_chars > out_max) max_chars = out_max;

    for (int i = 0; i < max_chars; i++) {
        while (bits_avail < bits_per_char) {
            if (byte_pos < data_len)
                bit_buf |= static_cast<uint32_t>(data[byte_pos++]) << bits_avail;
            bits_avail += 8;
        }
        uint32_t val = bit_buf & mask;
        bit_buf >>= bits_per_char;
        bits_avail -= bits_per_char;

        if (val == 0) break;

        uint16_t ch;
        if (val < 0x20)
            ch = CHAR_TABLE[val];
        else
            ch = (base_char - 0x20) + static_cast<uint16_t>(val);

        if (ch == 0) break;
        out[count++] = ch;
    }
    return count;
}

// ─── Entry decode (key derivation + RC4 + unpack) ────────────────────

static int decode_entry_impl(
    const uint8_t* entry_data, int entry_len,
    uint64_t key,
    uint16_t* out_buf, int out_max)
{
    if (!entry_data || entry_len < 6 || !out_buf || out_max < 1)
        return -1;

    uint16_t total_size = *reinterpret_cast<const uint16_t*>(entry_data);
    uint16_t base_char  = *reinterpret_cast<const uint16_t*>(entry_data + 2);
    uint8_t  bpc        = entry_data[4];

    if (total_size < 6 || total_size > entry_len)
        return -1;

    int payload_len = total_size - 6;
    if (payload_len <= 0 || payload_len > 8192)
        return -1;

    uint8_t payload[8192];
    memcpy(payload, entry_data + 6, payload_len);

    if (key != 0) {
        uint8_t rc4_key[20];
        derive_rc4_key(key, rc4_key);
        uint8_t s[256];
        rc4_init(rc4_key, 20, s);
        rc4_crypt(s, payload, payload_len);
    }

    if (base_char == 0 && bpc == 0x10) {
        // Raw UTF-16LE
        int n = payload_len / 2;
        if (n > out_max) n = out_max;
        int count = 0;
        const auto* src = reinterpret_cast<const uint16_t*>(payload);
        for (int i = 0; i < n; i++) {
            if (src[i] == 0) break;
            out_buf[count++] = src[i];
        }
        return count;
    } else {
        return bit_unpack(payload, payload_len, base_char, bpc,
                          out_buf, out_max);
    }
}

// ─── Python binding ──────────────────────────────────────────────────

static py::str decode_entry(py::bytes entry_bytes, uint64_t key) {
    char* data;
    Py_ssize_t len;
    if (PyBytes_AsStringAndSize(entry_bytes.ptr(), &data, &len) < 0)
        return py::str("");

    static thread_local uint16_t out_buf[4096];
    int count = decode_entry_impl(
        reinterpret_cast<const uint8_t*>(data),
        static_cast<int>(len),
        key, out_buf, 4096);

    if (count <= 0)
        return py::str("");

    PyObject* unicode = PyUnicode_DecodeUTF16(
        reinterpret_cast<const char*>(out_buf),
        count * 2, nullptr, nullptr);
    if (!unicode) {
        PyErr_Clear();
        return py::str("");
    }
    return py::reinterpret_steal<py::str>(unicode);
}

PYBIND11_EMBEDDED_MODULE(PyStringDecoder, m) {
    m.def("decode_entry", &decode_entry,
        py::arg("entry_bytes"), py::arg("key"),
        "Decode a raw string table entry. Returns decoded string or empty string on error.");
}
