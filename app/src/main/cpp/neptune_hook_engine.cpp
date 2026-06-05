/**
 * Neptune Client - C++ Hook Engine
 * File: neptune_hook_engine.cpp
 *
 * Entry point untuk libneptune.so
 * Melakukan hooking ke libminecraftpe.so menggunakan teknik
 * inline hooking via Substrate/Dobby (ARM64 hook framework).
 *
 * PENTING: Library ini HANYA membaca data (FPS, armor NBT, yaw)
 * dan memodifikasi parameter visual (FOV, ambient light).
 * Tidak ada modifikasi pada logika gameplay, damage, atau network packet.
 */

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string>
#include <atomic>
#include <chrono>

// Hook framework (gunakan Dobby - open source, ARM64 support)
// https://github.com/jmpews/Dobby
#include "dobby.h"

// Neptune internal
#include "hooks/fps_hook.h"
#include "hooks/fov_hook.h"
#include "hooks/armor_hook.h"
#include "hooks/player_hook.h"
#include "utils/memory_utils.h"
#include "utils/offset_resolver.h"

#define LOG_TAG "NeptuneClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Global state ──────────────────────────────────────────────────────────────

static void* g_mcpe_handle = nullptr;  // handle ke libminecraftpe.so

// Data yang diexpose ke Java via JNI
struct NeptuneHudData {
    std::atomic<int>   fps{0};
    std::atomic<int>   left_cps{0};
    std::atomic<int>   right_cps{0};
    std::atomic<float> armor_durability[4];  // 0=Helm,1=Chest,2=Legs,3=Boots
    std::atomic<float> yaw{0.0f};

    NeptuneHudData() {
        for (auto& a : armor_durability) a.store(-1.0f);
    }
} g_hud_data;

// Feature flags
struct FeatureFlags {
    std::atomic<bool> fullbright{false};
    std::atomic<bool> freelook{false};
    std::atomic<bool> freecam{false};
    std::atomic<bool> toggle_sprint{false};
    std::atomic<float> zoom_fov{0.0f};  // 0 = off
} g_features;

// ── Library Loading ───────────────────────────────────────────────────────────

/**
 * Mencari dan meng-load libminecraftpe.so dari proses Minecraft
 * yang sudah berjalan. Menggunakan /proc/self/maps untuk menemukan
 * base address library.
 */
bool loadMinecraftLibrary() {
    // Minecraft sudah berjalan di proses yang sama (via wrapper/embedder)
    // atau di proses terpisah yang kita attach ke
    g_mcpe_handle = dlopen("libminecraftpe.so", RTLD_NOLOAD | RTLD_NOW);

    if (!g_mcpe_handle) {
        LOGE("Failed to get handle for libminecraftpe.so: %s", dlerror());
        return false;
    }

    LOGI("libminecraftpe.so loaded at handle: %p", g_mcpe_handle);
    return true;
}

// ── Hook Installation ─────────────────────────────────────────────────────────

/**
 * Install semua hooks.
 * Dipanggil sekali saat JNI_OnLoad atau saat initialize() dipanggil dari Java.
 *
 * Setiap hook menggunakan Dobby untuk intercept fungsi asli MCPE,
 * membaca data yang kita butuhkan, lalu memanggil fungsi asli kembali
 * (trampoline call) agar gameplay tetap normal.
 */
bool installAllHooks() {
    if (!g_mcpe_handle) {
        LOGE("Cannot install hooks: mcpe handle is null");
        return false;
    }

    bool success = true;

    // 1. FPS Hook - intercept RenderDragon frame callback
    success &= FpsHook::install(g_mcpe_handle, &g_hud_data.fps);

    // 2. FOV Hook - intercept getFov() untuk Zoom feature
    success &= FovHook::install(g_mcpe_handle, &g_features.zoom_fov);

    // 3. Armor Durability Hook - baca NBT slot armor dari LocalPlayer
    success &= ArmorHook::install(g_mcpe_handle, g_hud_data.armor_durability);

    // 4. Player Hook - baca yaw, CPS counter, sprint state
    success &= PlayerHook::install(g_mcpe_handle, g_hud_data, g_features);

    if (success) {
        LOGI("All Neptune hooks installed successfully");
    } else {
        LOGE("Some hooks failed to install - check logs above");
    }

    return success;
}

// ── JNI Implementations ───────────────────────────────────────────────────────

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_initialize(
    JNIEnv* env, jobject /* thiz */)
{
    LOGI("Neptune Client initializing...");

    if (!loadMinecraftLibrary()) return JNI_FALSE;
    if (!installAllHooks())     return JNI_FALSE;

    LOGI("Neptune Client ready!");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_shutdown(
    JNIEnv* env, jobject /* thiz */)
{
    LOGI("Neptune Client shutting down...");
    FpsHook::uninstall();
    FovHook::uninstall();
    ArmorHook::uninstall();
    PlayerHook::uninstall();

    if (g_mcpe_handle) {
        dlclose(g_mcpe_handle);
        g_mcpe_handle = nullptr;
    }
}

/**
 * Mengembalikan semua data HUD sekaligus ke Java dalam satu objek.
 * Lebih efisien daripada memanggil JNI berkali-kali tiap field.
 */
JNIEXPORT jobject JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_getHudData(
    JNIEnv* env, jobject /* thiz */)
{
    // Cari class HudData
    jclass hudClass = env->FindClass("com/neptune/client/hooks/HudData");
    if (!hudClass) return nullptr;

    // Cari constructor: HudData(int fps, int leftCps, int rightCps, float[] armor, float yaw)
    jmethodID ctor = env->GetMethodID(hudClass, "<init>", "(III[FF)V");
    if (!ctor) return nullptr;

    // Buat array durabilitas armor
    jfloatArray armorArray = env->NewFloatArray(4);
    float durabilities[4];
    for (int i = 0; i < 4; i++) {
        durabilities[i] = g_hud_data.armor_durability[i].load();
    }
    env->SetFloatArrayRegion(armorArray, 0, 4, durabilities);

    // Bungkus array dalam Kotlin Array<Float> (boxing)
    jclass floatArrayClass = env->FindClass("[Ljava/lang/Float;");
    jobjectArray boxedArray = env->NewObjectArray(4,
        env->FindClass("java/lang/Float"), nullptr);
    jmethodID floatValueOf = env->GetStaticMethodID(
        env->FindClass("java/lang/Float"), "valueOf", "(F)Ljava/lang/Float;");
    for (int i = 0; i < 4; i++) {
        env->SetObjectArrayElement(boxedArray, i,
            env->CallStaticObjectMethod(env->FindClass("java/lang/Float"),
                floatValueOf, durabilities[i]));
    }

    // Buat objek HudData
    return env->NewObject(hudClass, ctor,
        (jint)  g_hud_data.fps.load(),
        (jint)  g_hud_data.left_cps.load(),
        (jint)  g_hud_data.right_cps.load(),
        boxedArray,
        (jfloat) g_hud_data.yaw.load()
    );
}

// ── Feature Setters ────────────────────────────────────────────────────────

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_setFullbright(
    JNIEnv*, jobject, jboolean enabled)
{
    g_features.fullbright.store(enabled);
    LOGI("Fullbright: %s", enabled ? "ON" : "OFF");
}

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_setFreelook(
    JNIEnv*, jobject, jboolean enabled)
{
    g_features.freelook.store(enabled);
}

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_setFreecam(
    JNIEnv*, jobject, jboolean enabled)
{
    g_features.freecam.store(enabled);
}

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_setToggleSprint(
    JNIEnv*, jobject, jboolean enabled)
{
    g_features.toggle_sprint.store(enabled);
}

JNIEXPORT void JNICALL
Java_com_neptune_client_hooks_NeptuneHookBridge_setZoomFov(
    JNIEnv*, jobject, jfloat fovDegrees)
{
    g_features.zoom_fov.store(fovDegrees);
}

} // extern "C"
