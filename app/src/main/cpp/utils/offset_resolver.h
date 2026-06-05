/**
 * Neptune Client - Offset Resolver
 * File: utils/offset_resolver.h
 *
 * Mengelola tabel offset per versi Minecraft Bedrock.
 *
 * Masalah utama: Setiap update MCPE mengubah posisi fungsi di memori.
 * Solusi: Kita maintain tabel offset per versi, lalu saat startup
 * deteksi versi MCPE yang aktif dan gunakan tabel yang sesuai.
 *
 * Cara mendapatkan offset baru:
 *   1. Gunakan Ghidra/IDA Pro untuk decompile libminecraftpe.so
 *   2. Cari fungsi via nama simbol (jika tersedia) atau pattern signature
 *   3. Catat offset (relative dari base address library)
 */

#pragma once
#include <string>
#include <unordered_map>
#include <android/log.h>

// Versi MCPE yang didukung Neptune Client
enum class McpeVersion {
    UNKNOWN,
    V1_20_80,
    V1_21_0,
    V1_21_2,
    V1_21_50,
};

class OffsetResolver {
public:

    // ── Offset tables per versi ────────────────────────────────────────────
    // Format: { "NamaFungsi", offset_dari_base }
    // Offset ini HARUS diupdate tiap MCPE rilis versi baru.

    using OffsetTable = std::unordered_map<std::string, uintptr_t>;

    static const OffsetTable& getTableFor(McpeVersion ver) {
        // Table untuk MCPE 1.21.50 ARM64
        static OffsetTable table_1_21_50 = {
            { "ScreenContext__doRender",         0x1A23B40 },
            { "LevelRenderer__getFov",           0x2B14C80 },
            { "Player__tick",                    0x1F38D10 },
            { "LocalPlayer__moveRelative",       0x20A5E60 },
            { "Level__getGamma",                 0x18C3720 },
            { "Player__attack",                  0x1F88B40 },
            { "Player__interact",                0x1FA1C20 },
            { "Item__getMaxDamage_vtable_idx",   18         }, // vtable slot index
        };

        // Table untuk MCPE 1.21.2 ARM64
        static OffsetTable table_1_21_2 = {
            { "ScreenContext__doRender",         0x1A01B20 },
            { "LevelRenderer__getFov",           0x2AF3C60 },
            { "Player__tick",                    0x1F12D00 },
            { "LocalPlayer__moveRelative",       0x208AE40 },
            { "Level__getGamma",                 0x18A3700 },
            { "Player__attack",                  0x1F68B20 },
            { "Player__interact",                0x1F81C00 },
            { "Item__getMaxDamage_vtable_idx",   18         },
        };

        // Table untuk MCPE 1.21.0 ARM64
        static OffsetTable table_1_21_0 = {
            { "ScreenContext__doRender",         0x19E1A00 },
            { "LevelRenderer__getFov",           0x2AD1A40 },
            { "Player__tick",                    0x1EF0C80 },
            { "LocalPlayer__moveRelative",       0x206CE20 },
            { "Level__getGamma",                 0x188B6E0 },
            { "Player__attack",                  0x1F48A00 },
            { "Player__interact",                0x1F61BE0 },
            { "Item__getMaxDamage_vtable_idx",   17         },
        };

        // Table untuk MCPE 1.20.80 ARM64
        static OffsetTable table_1_20_80 = {
            { "ScreenContext__doRender",         0x19A1960 },
            { "LevelRenderer__getFov",           0x2A91900 },
            { "Player__tick",                    0x1EC0B60 },
            { "LocalPlayer__moveRelative",       0x204CE00 },
            { "Level__getGamma",                 0x186B6C0 },
            { "Player__attack",                  0x1F28960 },
            { "Player__interact",                0x1F41BC0 },
            { "Item__getMaxDamage_vtable_idx",   17         },
        };

        switch (ver) {
            case McpeVersion::V1_21_50: return table_1_21_50;
            case McpeVersion::V1_21_2:  return table_1_21_2;
            case McpeVersion::V1_21_0:  return table_1_21_0;
            case McpeVersion::V1_20_80: return table_1_20_80;
            default:                    return table_1_21_50; // fallback
        }
    }

    // ── Deteksi versi MCPE ────────────────────────────────────────────────

    static McpeVersion detectVersion() {
        if (s_detected_version != McpeVersion::UNKNOWN) {
            return s_detected_version;
        }

        // Baca versi dari /proc/self/maps atau dari APK metadata
        // Metode 1: Cek file package_info via Android PackageManager (dari JNI env)
        // Metode 2: Baca string versi dari libminecraftpe.so langsung
        std::string ver = readVersionString();

        if (ver.find("1.21.50") != std::string::npos || ver.find("1.21.5") != std::string::npos) {
            s_detected_version = McpeVersion::V1_21_50;
        } else if (ver.find("1.21.2") != std::string::npos) {
            s_detected_version = McpeVersion::V1_21_2;
        } else if (ver.find("1.21.0") != std::string::npos) {
            s_detected_version = McpeVersion::V1_21_0;
        } else if (ver.find("1.20.80") != std::string::npos) {
            s_detected_version = McpeVersion::V1_20_80;
        } else {
            __android_log_print(ANDROID_LOG_WARN, "Neptune",
                "OffsetResolver: Unknown MCPE version '%s', using latest table",
                ver.c_str());
            s_detected_version = McpeVersion::V1_21_50;
        }

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "OffsetResolver: Detected MCPE version: %s", ver.c_str());

        return s_detected_version;
    }

    // ── Public API ─────────────────────────────────────────────────────────

    static uintptr_t getMcpeBase() {
        if (s_mcpe_base != 0) return s_mcpe_base;

        // Baca /proc/self/maps untuk menemukan base address libminecraftpe.so
        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps) return 0;

        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "libminecraftpe.so") && strstr(line, "r-xp")) {
                uintptr_t start = 0;
                sscanf(line, "%lx-", &start);
                s_mcpe_base = start;
                break;
            }
        }
        fclose(maps);

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "libminecraftpe.so base: 0x%lx", s_mcpe_base);
        return s_mcpe_base;
    }

    static uintptr_t getOffset(const std::string& name) {
        McpeVersion ver = detectVersion();
        const auto& table = getTableFor(ver);
        auto it = table.find(name);
        if (it == table.end()) {
            __android_log_print(ANDROID_LOG_WARN, "Neptune",
                "OffsetResolver: No offset for '%s'", name.c_str());
            return 0;
        }
        return it->second;
    }

private:
    static McpeVersion s_detected_version;
    static uintptr_t   s_mcpe_base;

    static std::string readVersionString() {
        // Coba baca versi dari symlink /proc/self → cek package name
        // lalu query PackageManager via JNI
        // Fallback: scan string di binary untuk "1.2x.x"
        void* handle = dlopen("libminecraftpe.so", RTLD_NOLOAD | RTLD_NOW);
        if (!handle) return "unknown";

        // Cari simbol versi string (bisa jadi ada di data section)
        const char** ver_sym = reinterpret_cast<const char**>(
            dlsym(handle, "g_minecraftVersion")
        );
        if (ver_sym && *ver_sym) {
            return std::string(*ver_sym);
        }

        // Fallback: return "unknown", akan pakai tabel terbaru
        return "unknown";
    }
};

// Static definitions
McpeVersion OffsetResolver::s_detected_version = McpeVersion::UNKNOWN;
uintptr_t   OffsetResolver::s_mcpe_base        = 0;
