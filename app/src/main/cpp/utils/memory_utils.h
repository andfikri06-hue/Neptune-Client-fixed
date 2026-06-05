/**
 * Neptune Client - Memory Utils
 * File: utils/memory_utils.h
 *
 * Helper functions untuk operasi memori yang aman.
 * Mencegah crash akibat membaca pointer null/invalid.
 */

#pragma once
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

class MemoryUtils {
public:

    /**
     * Cek apakah pointer valid dan dapat dibaca.
     * Gunakan ini sebelum dereferencing pointer yang tidak diketahui.
     *
     * Strategi: mincore() syscall mengembalikan info apakah halaman memori
     * ada di RAM. Lebih aman dari langsung deref.
     */
    static bool isValidPtr(void* ptr) {
        if (!ptr) return false;

        // Cek alignment dasar
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr < 0x1000) return false; // Null page area

        // Cek apakah halaman memori ada dan bisa dibaca
        // Gunakan /proc/self/maps untuk validasi
        static const long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t page_start = addr & ~(page_size - 1);

        unsigned char vec = 0;
        int result = mincore(reinterpret_cast<void*>(page_start), page_size, &vec);

        // result == 0 dan vec & 1 = halaman ada di memori
        return (result == 0 && (vec & 1));
    }

    /**
     * Safe read: baca nilai dari pointer, return default jika tidak valid.
     */
    template<typename T>
    static T safeRead(void* base, size_t offset, T default_val = T{}) {
        uint8_t* ptr = reinterpret_cast<uint8_t*>(base) + offset;
        if (!isValidPtr(ptr)) return default_val;
        T val;
        memcpy(&val, ptr, sizeof(T));
        return val;
    }

    /**
     * Safe write: tulis nilai ke pointer, return false jika tidak valid.
     */
    template<typename T>
    static bool safeWrite(void* base, size_t offset, T value) {
        uint8_t* ptr = reinterpret_cast<uint8_t*>(base) + offset;
        if (!isValidPtr(ptr)) return false;
        memcpy(ptr, &value, sizeof(T));
        return true;
    }

    /**
     * Ubah permission memori menjadi writable (untuk patching).
     * Gunakan HANYA untuk modifikasi yang diperlukan, restore setelah selesai.
     */
    static bool makeWritable(void* addr, size_t size) {
        static const long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t start = reinterpret_cast<uintptr_t>(addr);
        uintptr_t page_start = start & ~(page_size - 1);
        size_t page_len = ((start + size - page_start) + page_size - 1) & ~(page_size - 1);
        return mprotect(reinterpret_cast<void*>(page_start), page_len,
                        PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
    }

    static bool makeReadExec(void* addr, size_t size) {
        static const long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t start = reinterpret_cast<uintptr_t>(addr);
        uintptr_t page_start = start & ~(page_size - 1);
        size_t page_len = ((start + size - page_start) + page_size - 1) & ~(page_size - 1);
        return mprotect(reinterpret_cast<void*>(page_start), page_len,
                        PROT_READ | PROT_EXEC) == 0;
    }

    /**
     * Cari byte pattern di range memori.
     * Berguna untuk menemukan fungsi via signature jika offset tidak diketahui.
     *
     * Pattern: "48 8B 05 ?? ?? ?? ??" — gunakan 0xFF untuk fixed, 0x00 untuk wildcard
     * Mask:    "xx x xxxx" — 'x' = match, '?' = skip
     */
    static void* findPattern(uintptr_t start, size_t searchLen,
                             const uint8_t* pattern, const char* mask) {
        size_t patLen = strlen(mask);
        for (size_t i = 0; i < searchLen - patLen; i++) {
            bool found = true;
            for (size_t j = 0; j < patLen; j++) {
                if (mask[j] == 'x' &&
                    reinterpret_cast<uint8_t*>(start)[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return reinterpret_cast<void*>(start + i);
        }
        return nullptr;
    }
};
