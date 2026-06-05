/**
 * Neptune Client - Player Hook
 * File: hooks/player_hook.h
 *
 * Hook serba guna pada LocalPlayer untuk fitur:
 *   - Membaca yaw (rotasi horizontal) → Compass
 *   - CPS counter (via touch input intercept)
 *   - Toggle Sprint
 *   - Freelook (rotasi kamera tanpa mengubah arah berjalan)
 *   - Fullbright (override ambient light level)
 */

#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include "dobby.h"
#include "../utils/offset_resolver.h"

// Referensi ke NeptuneHudData dan FeatureFlags dari engine utama
struct NeptuneHudData;
struct FeatureFlags;

// Offset dalam LocalPlayer / Actor untuk yaw
namespace PlayerOffsets {
    constexpr size_t YAW_OFFSET       = 0x1B0; // float yaw di Actor
    constexpr size_t SPRINT_FLAG      = 0x3A4; // bool isSprinting
    constexpr size_t AMBIENT_LIGHT    = 0x28;  // di GameplayUserInterface atau Level
}

class PlayerHook {
public:
    // Referensi ke HudData dan FeatureFlags global
    static NeptuneHudData* s_hud;
    static FeatureFlags*   s_features;

    // ── CPS Counter ────────────────────────────────────────────────────────
    // Simpan timestamp setiap click dalam window 1 detik terakhir
    static std::deque<std::chrono::steady_clock::time_point> s_left_clicks;
    static std::deque<std::chrono::steady_clock::time_point> s_right_clicks;
    static std::mutex s_cps_mutex;

    // ── Freelook ───────────────────────────────────────────────────────────
    // Simpan yaw/pitch asli saat freelook aktif
    static float s_saved_yaw;
    static float s_saved_pitch;
    static bool  s_freelook_saved;

    // ── Hooks: fungsi asli ─────────────────────────────────────────────────
    static void  (*s_orig_player_move)(void* player, void* moveInputHandler);
    static float (*s_orig_get_gamma)(void* level);
    static void  (*s_orig_handle_attack)(void* player, void* actor);
    static void  (*s_orig_handle_interact)(void* player, void* actor, void* vec);

    // ──────────────────────────────────────────────────────────────────────
    // HOOK 1: Player move / update tick
    // Dipanggil setiap tick. Gunakan untuk:
    //   - Baca yaw
    //   - Paksa sprint (toggle sprint)
    //   - Tahan yaw saat freelook aktif
    // ──────────────────────────────────────────────────────────────────────
    static void hookedPlayerMove(void* player, void* moveInputHandler) {
        if (!s_hud || !s_features) {
            s_orig_player_move(player, moveInputHandler);
            return;
        }

        // Baca yaw saat ini (sebelum panggil asli)
        float* yaw_ptr = reinterpret_cast<float*>(
            reinterpret_cast<uint8_t*>(player) + PlayerOffsets::YAW_OFFSET
        );
        float current_yaw = *yaw_ptr;

        // Toggle Sprint: paksa isMoving = true dan set sprint flag
        if (s_features->toggle_sprint.load()) {
            bool* sprint_ptr = reinterpret_cast<bool*>(
                reinterpret_cast<uint8_t*>(player) + PlayerOffsets::SPRINT_FLAG
            );
            *sprint_ptr = true;
        }

        // Freelook: sebelum panggil move asli, simpan yaw
        bool freelook_on = s_features->freelook.load();
        float yaw_before = *yaw_ptr;

        // Panggil move asli (ini yang memproses input dan mengubah yaw)
        s_orig_player_move(player, moveInputHandler);

        // Freelook: setelah move asli, restore yaw ke nilai sebelum input
        if (freelook_on) {
            if (!s_freelook_saved) {
                s_saved_yaw   = yaw_before;
                s_saved_pitch = 0.0f; // pitch bisa dibaca serupa
                s_freelook_saved = true;
            }
            // Restore yaw → karakter tetap menghadap ke arah asli
            // Kamera overlay bisa memutar secara independen dari UI layer
            *yaw_ptr = s_saved_yaw;
        } else {
            s_freelook_saved = false;
        }

        // Update yaw ke HUD (untuk compass) — gunakan yaw kamera (boleh beda saat freelook)
        s_hud->yaw.store(*yaw_ptr);
    }

    // ──────────────────────────────────────────────────────────────────────
    // HOOK 2: Level::getGamma() → Fullbright
    // Mengembalikan nilai ambient light level saat ini.
    // Kita override ke 1.0 saat fullbright aktif.
    // ──────────────────────────────────────────────────────────────────────
    static float hookedGetGamma(void* level) {
        float original = s_orig_get_gamma(level);
        if (s_features && s_features->fullbright.load()) {
            return 1.0f; // Pencahayaan maksimum
        }
        return original;
    }

    // ──────────────────────────────────────────────────────────────────────
    // HOOK 3: Player::attack() → Left CPS Counter
    // Dipanggil setiap kali pemain melakukan serangan/pukulan.
    // ──────────────────────────────────────────────────────────────────────
    static void hookedHandleAttack(void* player, void* actor) {
        // Panggil asli dulu
        s_orig_handle_attack(player, actor);

        // Catat waktu click
        recordClick(s_left_clicks);

        // Update CPS ke HUD
        if (s_hud) {
            s_hud->left_cps.store(countCps(s_left_clicks));
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // HOOK 4: Player::interact() → Right CPS Counter
    // Dipanggil saat pemain interact (taruh blok, pakai item, dll).
    // ──────────────────────────────────────────────────────────────────────
    static void hookedHandleInteract(void* player, void* actor, void* vec) {
        s_orig_handle_interact(player, actor, vec);

        recordClick(s_right_clicks);
        if (s_hud) {
            s_hud->right_cps.store(countCps(s_right_clicks));
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // Install semua hook
    // ──────────────────────────────────────────────────────────────────────
    static bool install(void* mcpe_handle, NeptuneHudData& hud, FeatureFlags& features) {
        s_hud      = &hud;
        s_features = &features;

        uintptr_t base = OffsetResolver::getMcpeBase();
        bool ok = true;

        // Hook 1: Player::moveRelative (atau setara)
        uintptr_t move_offset = OffsetResolver::getOffset("LocalPlayer__moveRelative");
        if (move_offset) {
            void* target = reinterpret_cast<void*>(base + move_offset);
            DobbyHook(target,
                reinterpret_cast<void*>(hookedPlayerMove),
                reinterpret_cast<void**>(&s_orig_player_move));
        } else { ok = false; }

        // Hook 2: Level::getGamma
        uintptr_t gamma_offset = OffsetResolver::getOffset("Level__getGamma");
        if (gamma_offset) {
            void* target = reinterpret_cast<void*>(base + gamma_offset);
            DobbyHook(target,
                reinterpret_cast<void*>(hookedGetGamma),
                reinterpret_cast<void**>(&s_orig_get_gamma));
        } else { ok = false; }

        // Hook 3: Player::attack
        uintptr_t attack_offset = OffsetResolver::getOffset("Player__attack");
        if (attack_offset) {
            void* target = reinterpret_cast<void*>(base + attack_offset);
            DobbyHook(target,
                reinterpret_cast<void*>(hookedHandleAttack),
                reinterpret_cast<void**>(&s_orig_handle_attack));
        } else { ok = false; }

        // Hook 4: Player::interact
        uintptr_t interact_offset = OffsetResolver::getOffset("Player__interact");
        if (interact_offset) {
            void* target = reinterpret_cast<void*>(base + interact_offset);
            DobbyHook(target,
                reinterpret_cast<void*>(hookedHandleInteract),
                reinterpret_cast<void**>(&s_orig_handle_interact));
        } else { ok = false; }

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "PlayerHook: %s", ok ? "all installed" : "some hooks failed");
        return ok;
    }

    static void uninstall() {
        uintptr_t base = OffsetResolver::getMcpeBase();
        const char* keys[] = {
            "LocalPlayer__moveRelative",
            "Level__getGamma",
            "Player__attack",
            "Player__interact"
        };
        for (auto& key : keys) {
            uintptr_t off = OffsetResolver::getOffset(key);
            if (off) DobbyDestroy(reinterpret_cast<void*>(base + off));
        }
    }

private:
    // Rekam satu click baru, buang yang lebih dari 1 detik lalu
    static void recordClick(std::deque<std::chrono::steady_clock::time_point>& q) {
        std::lock_guard<std::mutex> lock(s_cps_mutex);
        auto now = std::chrono::steady_clock::now();
        q.push_back(now);

        // Buang entry lama (> 1 detik)
        auto cutoff = now - std::chrono::seconds(1);
        while (!q.empty() && q.front() < cutoff) {
            q.pop_front();
        }
    }

    // Hitung CPS = jumlah click dalam 1 detik terakhir
    static int countCps(std::deque<std::chrono::steady_clock::time_point>& q) {
        std::lock_guard<std::mutex> lock(s_cps_mutex);
        auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        while (!q.empty() && q.front() < cutoff) q.pop_front();
        return static_cast<int>(q.size());
    }
};

// Static definitions
NeptuneHudData* PlayerHook::s_hud      = nullptr;
FeatureFlags*   PlayerHook::s_features = nullptr;
float PlayerHook::s_saved_yaw          = 0.0f;
float PlayerHook::s_saved_pitch        = 0.0f;
bool  PlayerHook::s_freelook_saved     = false;
std::deque<std::chrono::steady_clock::time_point> PlayerHook::s_left_clicks;
std::deque<std::chrono::steady_clock::time_point> PlayerHook::s_right_clicks;
std::mutex PlayerHook::s_cps_mutex;
void  (*PlayerHook::s_orig_player_move)(void*, void*)       = nullptr;
float (*PlayerHook::s_orig_get_gamma)(void*)                = nullptr;
void  (*PlayerHook::s_orig_handle_attack)(void*, void*)     = nullptr;
void  (*PlayerHook::s_orig_handle_interact)(void*, void*, void*) = nullptr;
