package com.neptune.client.ui

import android.content.Context
import android.view.View
import android.view.animation.DecelerateInterpolator
import android.widget.*
import com.neptune.client.features.Feature
import com.neptune.client.features.FeatureManager

/**
 * ModMenuPanel
 *
 * Panel overlay utama Neptune Client.
 * Menampilkan kategori fitur dengan toggle masing-masing.
 *
 * Kategori:
 * - PERFORMANCE (FPS Display)
 * - COMBAT (CPS Display, Freelook)
 * - VISUAL (Fullbright, Freecam, Zoom, Compass)
 * - UTILITIES (Durability Viewer, Shortcuts)
 */
class ModMenuPanel(
    context: Context,
    private val onFeatureToggled: (String, Boolean) -> Unit,
    private val onClose: () -> Unit
) : FrameLayout(context) {

    init {
        inflate(context, R.layout.mod_menu_panel, this)
        setupCategories()
        setupCloseButton()
    }

    private fun setupCategories() {
        val categoryContainer = findViewById<LinearLayout>(R.id.category_container)
        categoryContainer.removeAllViews()

        FeatureManager.getCategories().forEach { category ->
            val categoryView = CategoryView(context, category) { featureId, enabled ->
                onFeatureToggled(featureId, enabled)
            }
            categoryContainer.addView(categoryView)
        }
    }

    private fun setupCloseButton() {
        findViewById<View>(R.id.btn_close).setOnClickListener { onClose() }
    }

    fun animateIn() {
        alpha = 0f
        translationY = -30f
        animate()
            .alpha(1f)
            .translationY(0f)
            .setDuration(200)
            .setInterpolator(DecelerateInterpolator())
            .start()
    }

    fun animateOut(onEnd: () -> Unit) {
        animate()
            .alpha(0f)
            .translationY(-30f)
            .setDuration(150)
            .setInterpolator(DecelerateInterpolator())
            .withEndAction(onEnd)
            .start()
    }
}

/**
 * Satu baris kategori beserta list featurenya
 */
class CategoryView(
    context: Context,
    private val category: FeatureManager.Category,
    private val onToggle: (String, Boolean) -> Unit
) : LinearLayout(context) {

    private var isExpanded = true
    private lateinit var featureContainer: LinearLayout

    init {
        orientation = VERTICAL
        setupHeader()
        setupFeatures()
    }

    private fun setupHeader() {
        val header = TextView(context).apply {
            text = category.name
            textSize = 11f
            // Styling dihandle via style resources
        }
        header.setOnClickListener { toggleExpand() }
        addView(header)
    }

    private fun setupFeatures() {
        featureContainer = LinearLayout(context).apply {
            orientation = VERTICAL
        }

        category.features.forEach { feature ->
            val row = FeatureRow(context, feature) { enabled ->
                onToggle(feature.id, enabled)
            }
            featureContainer.addView(row)
        }

        addView(featureContainer)
    }

    private fun toggleExpand() {
        isExpanded = !isExpanded
        featureContainer.visibility = if (isExpanded) VISIBLE else GONE
    }
}

/**
 * Satu baris fitur: nama + toggle switch
 */
class FeatureRow(
    context: Context,
    private val feature: Feature,
    private val onToggle: (Boolean) -> Unit
) : LinearLayout(context) {

    private val toggle: Switch

    init {
        orientation = HORIZONTAL

        val label = TextView(context).apply {
            text = feature.displayName
            textSize = 13f
            layoutParams = LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
        }

        toggle = Switch(context).apply {
            isChecked = feature.isEnabled
            setOnCheckedChangeListener { _, checked ->
                feature.isEnabled = checked
                onToggle(checked)
            }
        }

        addView(label)
        addView(toggle)

        // Buka settings detail jika ada config
        if (feature.hasConfig) {
            val gear = ImageButton(context).apply {
                setOnClickListener { openFeatureConfig(feature) }
            }
            addView(gear)
        }
    }

    private fun openFeatureConfig(feature: Feature) {
        // Buka FeatureConfigSheet untuk fitur ini
        FeatureConfigSheet(context, feature).show()
    }

    fun updateState(enabled: Boolean) {
        toggle.isChecked = enabled
    }
}
