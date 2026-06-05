package com.neptune.client.features

/**
 * Data class untuk satu fitur mod
 */
data class Feature(
    val id: String,
    val displayName: String,
    val description: String,
    var isEnabled: Boolean = false,
    val hasConfig: Boolean = false,
    val category: String
)

/**
 * FeatureManager
 *
 * Registry pusat semua fitur Neptune Client.
 * Menyimpan state tiap fitur dan menghubungkannya ke native layer.
 */
object FeatureManager {

    data class Category(val name: String, val features: List<Feature>)

    private val features: Map<String, Feature> = buildFeatureMap()

    private fun buildFeatureMap(): Map<String, Feature> {
        val list = listOf(
            // ── PERFORMANCE ───────────────────────────────────
            Feature(
                id = "fps_display",
                displayName = "FPS Display",
                description = "Tampilkan Frames Per Second di layar",
                hasConfig = true,
                category = "PERFORMANCE"
            ),

            // ── COMBAT ────────────────────────────────────────
            Feature(
                id = "cps_display",
                displayName = "CPS Display",
                description = "Tampilkan Click Per Second (Left & Right)",
                hasConfig = true,
                category = "COMBAT"
            ),
            Feature(
                id = "freelook",
                displayName = "Freelook (360°)",
                description = "Lihat sekeliling tanpa mengubah arah jalan",
                hasConfig = false,
                category = "COMBAT"
            ),

            // ── VISUAL ────────────────────────────────────────
            Feature(
                id = "zoom",
                displayName = "Zoom",
                description = "Perbesar pandangan dengan memperkecil FOV",
                hasConfig = true,
                category = "VISUAL"
            ),
            Feature(
                id = "fullbright",
                displayName = "Fullbright",
                description = "Pencahayaan maksimum tanpa torch",
                hasConfig = false,
                category = "VISUAL"
            ),
            Feature(
                id = "freecam",
                displayName = "Freecam",
                description = "Kamera bisa bergerak bebas dari karakter",
                hasConfig = false,
                category = "VISUAL"
            ),
            Feature(
                id = "compass",
                displayName = "Compass Bar",
                description = "Kompas digital di bagian atas layar",
                hasConfig = true,
                category = "VISUAL"
            ),

            // ── UTILITIES ─────────────────────────────────────
            Feature(
                id = "durability_viewer",
                displayName = "Durability Viewer",
                description = "Bar durabilitas armor di atas EXP bar",
                hasConfig = false,
                category = "UTILITIES"
            ),
            Feature(
                id = "toggle_sprint",
                displayName = "Toggle Sprint",
                description = "Sekali ketuk untuk sprint terus-menerus",
                hasConfig = false,
                category = "UTILITIES"
            ),
            Feature(
                id = "keystrokes",
                displayName = "Keystrokes Display",
                description = "Visualisasi tombol gerakan (W/A/S/D)",
                hasConfig = true,
                category = "UTILITIES"
            )
        )
        return list.associateBy { it.id }
    }

    fun getCategories(): List<Category> {
        val grouped = features.values.groupBy { it.category }
        val order = listOf("PERFORMANCE", "COMBAT", "VISUAL", "UTILITIES")
        return order.mapNotNull { cat ->
            grouped[cat]?.let { Category(cat, it) }
        }
    }

    fun isEnabled(id: String): Boolean = features[id]?.isEnabled ?: false

    fun toggle(id: String, enabled: Boolean) {
        features[id]?.let { feature ->
            feature.isEnabled = enabled
            // Kirim ke native layer via JNI
            NativeFeatureApplier.apply(id, enabled)
        }
    }

    fun getFeature(id: String): Feature? = features[id]
}

/**
 * Menghubungkan toggle fitur ke C++ native layer
 */
object NativeFeatureApplier {
    fun apply(featureId: String, enabled: Boolean) {
        when (featureId) {
            "fps_display"       -> { /* FPS dibaca pasif, tidak perlu toggle native */ }
            "fullbright"        -> applyFullbright(enabled)
            "zoom"              -> { /* Zoom diaktifkan via input event, bukan toggle */ }
            "freelook"          -> applyFreelook(enabled)
            "freecam"           -> applyFreecam(enabled)
            "toggle_sprint"     -> applyToggleSprint(enabled)
            "durability_viewer" -> { /* Durability dibaca pasif */ }
        }
    }

    private external fun applyFullbright(enabled: Boolean)
    private external fun applyFreelook(enabled: Boolean)
    private external fun applyFreecam(enabled: Boolean)
    private external fun applyToggleSprint(enabled: Boolean)
}
