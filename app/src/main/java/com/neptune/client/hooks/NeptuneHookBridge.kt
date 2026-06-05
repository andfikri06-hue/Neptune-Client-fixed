package com.neptune.client.hooks

/**
 * NeptuneHookBridge
 *
 * Jembatan JNI antara Kotlin (UI Layer) dan C++ (Hook Layer).
 *
 * Native library: libneptune.so
 * Library ini di-load saat aplikasi start, lalu melakukan hooking
 * ke fungsi-fungsi di dalam libminecraftpe.so.
 */
object NeptuneHookBridge {

    init {
        System.loadLibrary("neptune") // load libneptune.so
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────
    external fun initialize(): Boolean
    external fun shutdown()

    // ── Data polling (dipanggil tiap 100ms dari HudManager) ───────────────
    external fun getHudData(): HudData

    // ── Feature toggles (dipanggil dari FeatureManager) ───────────────────
    external fun setFullbright(enabled: Boolean)
    external fun setFreelook(enabled: Boolean)
    external fun setFreecam(enabled: Boolean)
    external fun setToggleSprint(enabled: Boolean)
    external fun setZoomFov(fovDegrees: Float) // 0 = off, 30.0f = zoomed in

    // ── Input passthrough (untuk zoom/freelook via tombol virtual) ─────────
    external fun onZoomButtonDown()
    external fun onZoomButtonUp()
    external fun onFreelookDrag(deltaX: Float, deltaY: Float)
}

/**
 * Data class yang dikembalikan oleh getHudData()
 * Di-populate oleh C++ layer setiap dipanggil.
 */
data class HudData(
    val fps: Int = 0,
    val leftCps: Int = 0,
    val rightCps: Int = 0,

    // Durabilitas armor: index 0=Helm, 1=Chest, 2=Legs, 3=Boots
    // Nilai: 0.0..1.0 (persentase), -1.0 = slot kosong
    val armorDurability: Array<Float> = arrayOf(-1f, -1f, -1f, -1f),

    // Yaw karakter dalam derajat (untuk compass)
    val yaw: Float = 0f
) {
    // Override equals/hashCode karena ada Array
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is HudData) return false
        return fps == other.fps &&
               leftCps == other.leftCps &&
               rightCps == other.rightCps &&
               armorDurability.contentEquals(other.armorDurability) &&
               yaw == other.yaw
    }

    override fun hashCode(): Int {
        var result = fps
        result = 31 * result + leftCps
        result = 31 * result + rightCps
        result = 31 * result + armorDurability.contentHashCode()
        result = 31 * result + yaw.hashCode()
        return result
    }
}
