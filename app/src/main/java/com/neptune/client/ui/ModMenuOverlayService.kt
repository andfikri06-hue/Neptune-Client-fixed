package com.neptune.client.ui

import android.app.Service
import android.content.Intent
import android.graphics.PixelFormat
import android.os.IBinder
import android.view.Gravity
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import com.neptune.client.features.FeatureManager
import com.neptune.client.hooks.NeptuneHookBridge

/**
 * Neptune Client - Mod Menu Overlay Service
 *
 * Berjalan sebagai foreground service, menampilkan overlay transparan
 * di atas semua aplikasi (termasuk Minecraft). Tidak mengubah gameplay,
 * hanya membaca data dan menampilkan UI tambahan.
 */
class ModMenuOverlayService : Service() {

    private lateinit var windowManager: WindowManager
    private lateinit var overlayRoot: FrameLayout

    // Tombol toggle kecil (selalu tampil)
    private lateinit var toggleButton: NeptuneToggleButton

    // Panel menu utama (tampil saat toggle ditekan)
    private lateinit var modMenuPanel: ModMenuPanel

    // HUD elements (FPS, CPS, Compass, Durability)
    private lateinit var hudManager: HudManager

    private var isMenuVisible = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        windowManager = getSystemService(WINDOW_SERVICE) as WindowManager
        initOverlay()
        initFeatures()
    }

    private fun initOverlay() {
        overlayRoot = FrameLayout(this)

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            // FLAG_NOT_FOCUSABLE: sentuhan diteruskan ke Minecraft di bawahnya
            // FLAG_NOT_TOUCH_MODAL: hanya area UI neptune yg intersep sentuhan
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        )
        params.gravity = Gravity.TOP or Gravity.START

        // Toggle button (logo Neptune kecil di pojok)
        toggleButton = NeptuneToggleButton(this) {
            toggleMenu()
        }

        // Panel menu utama
        modMenuPanel = ModMenuPanel(this,
            onFeatureToggled = { featureId, enabled ->
                FeatureManager.toggle(featureId, enabled)
            },
            onClose = { hideMenu() }
        )

        // HUD manager (overlay informatif)
        hudManager = HudManager(this)

        overlayRoot.addView(toggleButton)
        overlayRoot.addView(modMenuPanel.apply { visibility = View.GONE })
        overlayRoot.addView(hudManager.rootView)

        windowManager.addView(overlayRoot, params)
    }

    private fun initFeatures() {
        // Init bridge ke C++ native layer
        NeptuneHookBridge.initialize()

        // Mulai polling data dari native (FPS, CPS, dll)
        hudManager.startPolling()
    }

    private fun toggleMenu() {
        if (isMenuVisible) hideMenu() else showMenu()
    }

    private fun showMenu() {
        isMenuVisible = true
        modMenuPanel.visibility = View.VISIBLE
        modMenuPanel.animateIn()
        // Saat menu terbuka, intercept sentuhan di area panel
        updateWindowFlags(interceptTouch = true)
    }

    private fun hideMenu() {
        isMenuVisible = false
        modMenuPanel.animateOut {
            modMenuPanel.visibility = View.GONE
        }
        // Setelah menu tutup, kembalikan sentuhan ke Minecraft
        updateWindowFlags(interceptTouch = false)
    }

    private fun updateWindowFlags(interceptTouch: Boolean) {
        val view = overlayRoot
        val params = view.layoutParams as WindowManager.LayoutParams
        if (interceptTouch) {
            params.flags = params.flags and WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE.inv()
        } else {
            params.flags = params.flags or WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
        }
        windowManager.updateViewLayout(view, params)
    }

    override fun onDestroy() {
        super.onDestroy()
        hudManager.stopPolling()
        NeptuneHookBridge.shutdown()
        if (::overlayRoot.isInitialized) {
            windowManager.removeView(overlayRoot)
        }
    }
}
