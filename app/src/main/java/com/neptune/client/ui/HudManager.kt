package com.neptune.client.ui

import android.content.Context
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.neptune.client.features.FeatureManager
import com.neptune.client.hooks.NeptuneHookBridge

/**
 * HudManager
 *
 * Mengelola semua elemen HUD informatif di layar:
 * - FPS counter
 * - CPS counter (Left/Right)
 * - Dynamic Durability Bar
 * - Compass bar
 *
 * Data diambil dari NeptuneHookBridge (native C++ layer) via polling.
 * Polling interval: 100ms (10 updates/detik) — cukup smooth, hemat baterai.
 */
class HudManager(private val context: Context) {

    val rootView: FrameLayout = FrameLayout(context)

    // HUD elements
    private val fpsView = FpsHudView(context)
    private val cpsView = CpsHudView(context)
    private val durabilityView = DurabilityBarView(context)
    private val compassView = CompassHudView(context)

    private val handler = Handler(Looper.getMainLooper())
    private val pollRunnable = object : Runnable {
        override fun run() {
            updateHud()
            handler.postDelayed(this, 100L) // 100ms interval
        }
    }

    init {
        rootView.addView(fpsView)
        rootView.addView(cpsView)
        rootView.addView(compassView)
        rootView.addView(durabilityView)
    }

    fun startPolling() {
        handler.post(pollRunnable)
    }

    fun stopPolling() {
        handler.removeCallbacks(pollRunnable)
    }

    private fun updateHud() {
        // Ambil data dari native bridge
        val data = NeptuneHookBridge.getHudData()

        // Update tiap view jika fiturnya aktif
        fpsView.apply {
            visibility = if (FeatureManager.isEnabled("fps_display")) View.VISIBLE else View.GONE
            update(data.fps)
        }

        cpsView.apply {
            visibility = if (FeatureManager.isEnabled("cps_display")) View.VISIBLE else View.GONE
            update(data.leftCps, data.rightCps)
        }

        durabilityView.apply {
            visibility = if (FeatureManager.isEnabled("durability_viewer")) View.VISIBLE else View.GONE
            update(data.armorDurability)
        }

        compassView.apply {
            visibility = if (FeatureManager.isEnabled("compass")) View.VISIBLE else View.GONE
            update(data.yaw)
        }
    }
}

// ─────────────────────────────────────────────
// FPS HUD View
// ─────────────────────────────────────────────
class FpsHudView(context: Context) : TextView(context) {
    private var fps = 0

    init {
        textSize = 12f
        setTextColor(Color.WHITE)
        setShadowLayer(2f, 1f, 1f, Color.BLACK)
        // Posisi: pojok kiri atas (bisa digeser user)
    }

    fun update(newFps: Int) {
        fps = newFps
        val color = when {
            fps >= 60 -> Color.parseColor("#00FF7F")  // hijau
            fps >= 30 -> Color.parseColor("#FFD700")  // kuning
            else      -> Color.parseColor("#FF4444")  // merah
        }
        setTextColor(color)
        text = "FPS $fps"
    }
}

// ─────────────────────────────────────────────
// CPS HUD View
// ─────────────────────────────────────────────
class CpsHudView(context: Context) : LinearLayout(context) {
    private val leftLabel: TextView
    private val rightLabel: TextView

    init {
        orientation = HORIZONTAL

        leftLabel = TextView(context).apply {
            textSize = 12f
            setTextColor(Color.parseColor("#87CEEB")) // biru muda = left click
            setShadowLayer(2f, 1f, 1f, Color.BLACK)
        }

        rightLabel = TextView(context).apply {
            textSize = 12f
            setTextColor(Color.parseColor("#FFB347")) // oranye = right click
            setShadowLayer(2f, 1f, 1f, Color.BLACK)
            setPadding(12, 0, 0, 0)
        }

        addView(leftLabel)
        addView(rightLabel)
    }

    fun update(leftCps: Int, rightCps: Int) {
        leftLabel.text = "L $leftCps"
        rightLabel.text = "R $rightCps"
    }
}

// ─────────────────────────────────────────────
// Durability Bar View
// ─────────────────────────────────────────────
class DurabilityBarView(context: Context) : LinearLayout(context) {
    // Slot: 0=Helm, 1=Chest, 2=Leggings, 3=Boots
    private val bars = Array(4) { DurabilityBar(context) }

    init {
        orientation = VERTICAL
        // Posisi: tepat di atas EXP bar (diset via LayoutParams dari luar)
        bars.forEach { addView(it) }
    }

    fun update(durabilities: Array<Float>) {
        // durabilities[i] = -1 jika slot kosong (tidak pakai armor)
        bars.forEachIndexed { i, bar ->
            if (i < durabilities.size && durabilities[i] >= 0f) {
                bar.visibility = View.VISIBLE
                bar.setPercent(durabilities[i])
            } else {
                bar.visibility = View.GONE
            }
        }
    }
}

class DurabilityBar(context: Context) : View(context) {
    private var percent: Float = 1f
    private var isBlinking = false

    fun setPercent(value: Float) {
        percent = value.coerceIn(0f, 1f)
        isBlinking = percent < 0.25f
        invalidate()
    }

    override fun onDraw(canvas: android.graphics.Canvas) {
        super.onDraw(canvas)
        val paint = android.graphics.Paint()

        // Background bar (abu gelap)
        paint.color = Color.parseColor("#44000000")
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), paint)

        // Foreground bar (warna sesuai kondisi)
        paint.color = when {
            percent >= 0.75f -> Color.parseColor("#4CAF50") // hijau
            percent >= 0.50f -> Color.parseColor("#FFEB3B") // kuning
            percent >= 0.25f -> Color.parseColor("#FF9800") // oranye
            else             -> if (shouldBlinkOn()) Color.parseColor("#F44336")
                                else Color.TRANSPARENT       // merah berkedip
        }
        canvas.drawRect(0f, 0f, width * percent, height.toFloat(), paint)
    }

    private fun shouldBlinkOn(): Boolean {
        // Kedip setiap 500ms
        return (System.currentTimeMillis() / 500) % 2 == 0L
    }
}

// ─────────────────────────────────────────────
// Compass HUD View
// ─────────────────────────────────────────────
class CompassHudView(context: Context) : View(context) {
    private var yaw: Float = 0f

    fun update(newYaw: Float) {
        yaw = newYaw
        invalidate()
    }

    override fun onDraw(canvas: android.graphics.Canvas) {
        super.onDraw(canvas)
        val paint = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG)

        val cx = width / 2f
        val cy = height / 2f

        // Background strip
        paint.color = Color.parseColor("#88000000")
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), 8f, 8f, paint)

        // Ticks & Labels
        paint.color = Color.WHITE
        paint.textSize = 14f
        paint.textAlign = android.graphics.Paint.Align.CENTER

        val directions = mapOf(0f to "S", 45f to "SW", 90f to "W", 135f to "NW",
                               180f to "N", 225f to "NE", 270f to "E", 315f to "SE")

        directions.forEach { (angle, label) ->
            val offset = normalizeAngle(angle - yaw)
            val x = cx + (offset / 90f) * (width / 2f)
            if (x in 0f..width.toFloat()) {
                val isCardinal = label.length == 1
                paint.color = if (label == "N") Color.parseColor("#FF4444")
                              else if (isCardinal) Color.WHITE
                              else Color.parseColor("#AAAAAA")
                paint.textSize = if (isCardinal) 14f else 11f
                canvas.drawText(label, x, cy + 5f, paint)
            }
        }

        // Center indicator (segitiga bawah)
        paint.color = Color.parseColor("#FFD700")
        val path = android.graphics.Path().apply {
            moveTo(cx, cy + 10f)
            lineTo(cx - 5f, cy)
            lineTo(cx + 5f, cy)
            close()
        }
        canvas.drawPath(path, paint)
    }

    private fun normalizeAngle(angle: Float): Float {
        var a = angle % 360f
        if (a > 180f) a -= 360f
        if (a < -180f) a += 360f
        return a
    }
}
