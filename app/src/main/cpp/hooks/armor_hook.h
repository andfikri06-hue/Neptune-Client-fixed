/**
 * Neptune Client - Armor Durability Hook
 * File: hooks/armor_hook.h
 *
 * Membaca data durabilitas armor dari slot inventory LocalPlayer secara real-time.
 * Menggunakan hook pada fungsi tick/update inventory untuk mendapatkan
 * nilai NBT "Damage" dan "MaxDamage" dari setiap slot armor.
 *
 * Slot index (MCPE standard):
 *   0 = Helm
 *   1 = Chestplate
 *   2 = Leggings
 *   3 = Boots
 */

#pragma once
#include <atomic>
#include <cstdint>
#include "dobby.h"
#include "../utils/offset_resolver.h"
#include "../utils/memory_utils.h"

// ── NBT / ItemStack Structure helpers ──────────────────────────────────────────
// Offset ini bervariasi per versi MCPE. OffsetResolver akan lookup tabel.
// Contoh untuk MCPE 1.21.x ARM64:
namespace McpeOffsets {
    // Dari base address Player object:
    constexpr size_t PLAYER_ARMOR_CONTAINER = 0x6B8;   // ArmorContainer*

    // Dari ArmorContainer:
    constexpr size_t CONTAINER_ITEMS_ARRAY  = 0x10;    // ItemStack* items[]

    // Dari ItemStack:
    constexpr size_t ITEM_STACK_ITEM_PTR    = 0x00;    // Item*
    constexpr size_t ITEM_STACK_DAMAGE      = 0x10;    // int16_t aux/damage
    constexpr size_t ITEM_STACK_MAX_DAMAGE  = 0x18;    // via Item::getMaxDamage()
}

class ArmorHook {
public:
    static std::atomic<float>* s_durabilities; // array of 4 atomics
    static void* s_local_player_ptr;           // pointer ke LocalPlayer object

    // Hook pada Player::tick() untuk update berkala
    static void (*s_original_player_tick)(void* /*player*/, void* /*blockSource*/);

    /**
     * Dipanggil setiap game tick (~20x/detik) dengan pointer 'this' = Player.
     * Kita baca slot armor dari sini.
     */
    static void hookedPlayerTick(void* player, void* blockSource) {
        // Panggil tick asli PERTAMA
        s_original_player_tick(player, blockSource);

        // Simpan pointer ke local player untuk fitur lain
        s_local_player_ptr = player;

        if (!s_durabilities) return;

        // Baca ArmorContainer dari offset Player
        void** armor_container_ptr = reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(player) + McpeOffsets::PLAYER_ARMOR_CONTAINER
        );
        if (!armor_container_ptr || !*armor_container_ptr) return;
        void* armor_container = *armor_container_ptr;

        // Baca array ItemStack[4] dari ArmorContainer
        void** items_array = reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(armor_container) + McpeOffsets::CONTAINER_ITEMS_ARRAY
        );
        if (!items_array) return;

        for (int slot = 0; slot < 4; slot++) {
            void* item_stack = items_array[slot];

            if (!item_stack || !MemoryUtils::isValidPtr(item_stack)) {
                // Slot kosong
                s_durabilities[slot].store(-1.0f);
                continue;
            }

            // Baca Item* (jika null = slot kosong / air item)
            void* item_ptr = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(item_stack) + McpeOffsets::ITEM_STACK_ITEM_PTR
            );
            if (!item_ptr) {
                s_durabilities[slot].store(-1.0f);
                continue;
            }

            // Baca damage saat ini (kerusakan yang sudah diterima)
            int16_t current_damage = *reinterpret_cast<int16_t*>(
                reinterpret_cast<uint8_t*>(item_stack) + McpeOffsets::ITEM_STACK_DAMAGE
            );

            // Baca max durability via virtual function getMaxDamage()
            // vtable index untuk getMaxDamage bervariasi, pakai offset resolver
            using GetMaxDamageFn = int(*)(void*);
            uintptr_t vtable = *reinterpret_cast<uintptr_t*>(item_ptr);
            uintptr_t max_dmg_fn_ptr = *reinterpret_cast<uintptr_t*>(
                vtable + OffsetResolver::getOffset("Item__getMaxDamage_vtable_idx") * sizeof(uintptr_t)
            );

            if (max_dmg_fn_ptr == 0) {
                s_durabilities[slot].store(-1.0f);
                continue;
            }

            GetMaxDamageFn getMaxDamage = reinterpret_cast<GetMaxDamageFn>(max_dmg_fn_ptr);
            int max_durability = getMaxDamage(item_ptr);

            if (max_durability <= 0) {
                // Item tidak memiliki durabilitas (misal: chainmail tanpa enchant)
                s_durabilities[slot].store(-1.0f);
                continue;
            }

            // Hitung persentase: (max - damage) / max = sisa durabilitas
            float percent = static_cast<float>(max_durability - current_damage)
                          / static_cast<float>(max_durability);
            percent = std::max(0.0f, std::min(1.0f, percent));
            s_durabilities[slot].store(percent);
        }
    }

    static bool install(void* mcpe_handle, std::atomic<float>* durabilities_out) {
        s_durabilities = durabilities_out;

        uintptr_t base   = OffsetResolver::getMcpeBase();
        uintptr_t offset = OffsetResolver::getOffset("Player__tick");

        if (offset == 0) {
            __android_log_print(ANDROID_LOG_ERROR, "Neptune",
                "ArmorHook: Cannot find Player::tick offset");
            return false;
        }

        void* target = reinterpret_cast<void*>(base + offset);
        DobbyHook(target,
            reinterpret_cast<void*>(hookedPlayerTick),
            reinterpret_cast<void**>(&s_original_player_tick));

        __android_log_print(ANDROID_LOG_INFO, "Neptune",
            "ArmorHook installed at %p", target);
        return true;
    }

    static void uninstall() {
        if (s_original_player_tick) {
            uintptr_t base   = OffsetResolver::getMcpeBase();
            uintptr_t offset = OffsetResolver::getOffset("Player__tick");
            DobbyDestroy(reinterpret_cast<void*>(base + offset));
            s_original_player_tick = nullptr;
        }
    }
};

// Static definitions
std::atomic<float>* ArmorHook::s_durabilities           = nullptr;
void*               ArmorHook::s_local_player_ptr        = nullptr;
void (*ArmorHook::s_original_player_tick)(void*, void*) = nullptr;
