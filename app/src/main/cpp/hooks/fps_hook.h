/**
 * Neptune Client - FPS Hook
 * File: hooks/fps_hook.h + fps_hook.cpp
 *
 * Intercept fungsi tick/render di RenderDragon (engine grafis MCPE)
 * untuk menghitung frame rate secara akurat.
 *
 * Strategi:
 * 1. Hook fungsi "AppPlatform::getFrameTime()" atau setara di MCPE
 * 2. Hitung FPS dari delta waktu antar frame
 * 3. Simpan ke atomic<int> yang dibaca HUD Manager
 */

#pragma once
#include <atomic>
#include <chrono>
#include <dlfcn.h>
#include "dobby.h"
#include "../utils/offset_resolver.h"

class FpsHook {
public:
    // Pointer ke atomic FPS counter yang dimiliki HudData
    static std::atomic<int>* s_fps_counter;

    // Pointer ke fungsi asli (trampoline Dobby)
    static void (*s_original_frame_tick)();

    // Untuk kalkulasi FPS
    static std::chrono::steady_clock::time_point s_last_frame_time;
    static int s_frame_count;
    static float s_accumulated_time;

    /**
     * Hook ini dipasang pada fungsi render frame Minecraft.
     * Dipanggil sekali per frame render.
     *
     * Nama fungsi asli di MCPE (ARM64, ditemukan via reverse engineering):
     * "_ZN13ScreenContext9_doRenderEv"
     * atau alternatif via vtable offset.
     */
    static void hookedFrameTick() {
        // 1. Panggil fungsi asli dulu (gameplay tidak terpengaruh)
        s_original_frame_tick();

        // 2. Hitung FPS
        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - s_last_frame_time).count();
        s_last_frame_time = now;

        s_frame_count++;
        s_accumulated_time += delta;

        // Update FPS setiap 0.5 detik
        if (s_accumulated_time >= 0.5f) {
            int new_fps = static_cast<int>(s_frame_count / s_accumulated_time);
            if (s_fps_counter) {
                s_fps_counter->store(new_fps);
            }
            s_frame_count = 0;
            s_accumulated_time = 0.0f;
        }
    }

    static bool install(void* mcpe_handle, std::atomic<int>* fps_out) {
        s_fps_counter = fps_out;
        s_last_frame_time = std::chrono::steady_clock::now();
        s_frame_count = 0;
        s_accumulated_time = 0.0f;

        // Resolve offset fungsi render di versi MCPE yang sedang berjalan
        // OffsetResolver membaca versi MCPE dari BuildInfo lalu lookup tabel offset
        uintptr_t base = OffsetResolver::getMcpeBase();
        uintptr_t offset = OffsetResolver::getOffset("ScreenContext__doRender");

        if (offset == 0) {
            __android_log_print(ANDROID_LOG_ERROR, "Neptune",
                "FpsHook: Cannot find ScreenContext::_doRender offset");
            return false;
        }

        void* target = reinterpret_cast<void*>(base + offset);

        // Dobby hook: target -> hookedFrameTick
        // s_original_frame_tick = trampoline ke fungsi asli
        DobbyHook(target,
            reinterpret_cast<void*>(hookedFrameTick),
            reinterpret_cast<void**>(&s_original_frame_tick));

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "FpsHook installed at %p (base+0x%zx)", target, offset);
        return true;
    }

    static void uninstall() {
        if (s_original_frame_tick) {
            uintptr_t base = OffsetResolver::getMcpeBase();
            uintptr_t offset = OffsetResolver::getOffset("ScreenContext__doRender");
            void* target = reinterpret_cast<void*>(base + offset);
            DobbyDestroy(target);
            s_original_frame_tick = nullptr;
        }
    }
};

// Static member definitions (di .cpp)
std::atomic<int>* FpsHook::s_fps_counter = nullptr;
void (*FpsHook::s_original_frame_tick)() = nullptr;
std::chrono::steady_clock::time_point FpsHook::s_last_frame_time;
int FpsHook::s_frame_count = 0;
float FpsHook::s_accumulated_time = 0.0f;
