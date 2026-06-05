/**
 * Neptune Client - FOV Hook
 * File: hooks/fov_hook.h
 *
 * Intercept fungsi getFov() pada kelas LevelRenderer atau Camera
 * untuk mengimplementasikan fitur Zoom (perkecil FOV saat tombol ditekan).
 *
 * Zoom bukan cheat karena:
 * - Hanya mengubah parameter visual lokal (field of view)
 * - Tidak mengungkap entitas yang belum di-render server
 * - Setara dengan menggunakan Optifine zoom di Java Edition
 */

#pragma once
#include <atomic>
#include <cmath>
#include "dobby.h"
#include "../utils/offset_resolver.h"

class FovHook {
public:
    static std::atomic<float>* s_zoom_fov;    // 0 = off, >0 = target FOV
    static float (*s_original_get_fov)(void* /*this*/, bool /*flag*/);

    // Current interpolated FOV (untuk smooth zoom transition)
    static float s_current_fov;
    static constexpr float ZOOM_SMOOTH_SPEED = 8.0f;  // lerp speed per frame

    /**
     * Hooked getFov()
     *
     * Signature asli di MCPE (ARM64):
     * float LevelRenderer::getFov(LevelRenderer* this, bool flag)
     *
     * 'flag' menentukan apakah ini FOV untuk proyektil/item (kita abaikan).
     */
    static float hookedGetFov(void* self, bool flag) {
        // Ambil FOV asli dari game
        float original_fov = s_original_get_fov(self, flag);

        float target_zoom = s_zoom_fov ? s_zoom_fov->load() : 0.0f;

        if (target_zoom <= 0.0f) {
            // Zoom off: kembalikan ke FOV asli (dengan smooth transition keluar)
            s_current_fov = lerpFov(s_current_fov, original_fov, 0.2f);

            // Jika sudah mendekati FOV asli, kembalikan persis
            if (std::abs(s_current_fov - original_fov) < 0.5f) {
                s_current_fov = original_fov;
            }
        } else {
            // Zoom on: interpolasi ke target FOV
            s_current_fov = lerpFov(s_current_fov, target_zoom, 0.15f);
        }

        return s_current_fov;
    }

    static bool install(void* mcpe_handle, std::atomic<float>* zoom_fov_out) {
        s_zoom_fov = zoom_fov_out;

        uintptr_t base = OffsetResolver::getMcpeBase();
        uintptr_t offset = OffsetResolver::getOffset("LevelRenderer__getFov");

        if (offset == 0) {
            __android_log_print(ANDROID_LOG_ERROR, "Neptune",
                "FovHook: Cannot find LevelRenderer::getFov offset");
            return false;
        }

        void* target = reinterpret_cast<void*>(base + offset);

        DobbyHook(target,
            reinterpret_cast<void*>(hookedGetFov),
            reinterpret_cast<void**>(&s_original_get_fov));

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "FovHook installed at %p", target);
        return true;
    }

    static void uninstall() {
        if (s_original_get_fov) {
            uintptr_t base = OffsetResolver::getMcpeBase();
            uintptr_t offset = OffsetResolver::getOffset("LevelRenderer__getFov");
            DobbyDestroy(reinterpret_cast<void*>(base + offset));
            s_original_get_fov = nullptr;
        }
    }

private:
    static float lerpFov(float from, float to, float t) {
        return from + (to - from) * t;
    }
};

// Static definitions
std::atomic<float>* FovHook::s_zoom_fov = nullptr;
float (*FovHook::s_original_get_fov)(void*, bool) = nullptr;
float FovHook::s_current_fov = 70.0f;  // Default MC FOV
