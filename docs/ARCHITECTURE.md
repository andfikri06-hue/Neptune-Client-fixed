# Neptune Client — Dokumentasi Arsitektur
## Versi 1.0.0

---

## 📐 Gambaran Besar Sistem

```
┌─────────────────────────────────────────────────────────────────┐
│                        ANDROID OS                               │
│                                                                 │
│  ┌──────────────────────┐    ┌──────────────────────────────┐  │
│  │   Neptune Client APK  │    │    Minecraft Bedrock APK     │  │
│  │                       │    │                              │  │
│  │  ┌─────────────────┐  │    │  ┌────────────────────────┐ │  │
│  │  │  MainActivity   │  │    │  │   libminecraftpe.so     │ │  │
│  │  │  (Permission +  │  │    │  │                        │ │  │
│  │  │   Service Start)│  │    │  │  • RenderDragon         │ │  │
│  │  └────────┬────────┘  │    │  │  • LocalPlayer          │ │  │
│  │           │            │    │  │  • Level / World        │ │  │
│  │  ┌────────▼────────┐  │    │  │  • ArmorContainer       │ │  │
│  │  │ModMenuOverlay   │  │    │  └────────────────────────┘ │  │
│  │  │Service          │◄─┼────┼──── dlopen(RTLD_NOLOAD)     │  │
│  │  │                 │  │    │                              │  │
│  │  │ ┌─────────────┐ │  │    │                              │  │
│  │  │ │  HudManager  │ │  │    └──────────────────────────────┘  │
│  │  │ │  (100ms poll)│ │  │                                      │
│  │  │ └──────┬───────┘ │  │    ┌──────────────────────────────┐  │
│  │  │        │JNI      │  │    │         libneptune.so         │  │
│  │  │ ┌──────▼───────┐ │  │    │                              │  │
│  │  │ │NeptuneHook   │─┼──┼───►│  ┌──────────┐ ┌──────────┐ │  │
│  │  │ │Bridge (JNI)  │ │  │    │  │ FpsHook  │ │ FovHook  │ │  │
│  │  │ └──────────────┘ │  │    │  └────┬─────┘ └────┬─────┘ │  │
│  │  └─────────────────┘  │    │       │              │       │  │
│  │                        │    │  ┌────▼──────────────▼────┐ │  │
│  │  ┌─────────────────┐  │    │  │  Dobby Hook Engine      │ │  │
│  │  │  ModMenuPanel   │  │    │  │  (ARM64 inline hook)    │ │  │
│  │  │  (Overlay UI)   │  │    │  └─────────────────────────┘ │  │
│  │  └─────────────────┘  │    └──────────────────────────────┘  │
│  └──────────────────────┘                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📁 Struktur File

```
NeptuneClient/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/neptune/client/
│       │   ├── MainActivity.kt               ← Entry point, permission handler
│       │   ├── ui/
│       │   │   ├── ModMenuOverlayService.kt  ← Foreground service, overlay root
│       │   │   ├── ModMenuPanel.kt           ← Panel UI dengan kategori & toggle
│       │   │   ├── HudManager.kt             ← FPS/CPS/Durability/Compass views
│       │   │   └── NeptuneToggleButton.kt    ← Tombol buka/tutup menu
│       │   ├── features/
│       │   │   └── FeatureManager.kt         ← Registry semua fitur
│       │   └── hooks/
│       │       └── NeptuneHookBridge.kt      ← JNI declarations + HudData
│       └── cpp/
│           ├── CMakeLists.txt
│           ├── neptune_hook_engine.cpp       ← JNI implementations + entry point
│           ├── hooks/
│           │   ├── fps_hook.h                ← Hook RenderDragon frame tick
│           │   ├── fov_hook.h                ← Hook getFov untuk Zoom
│           │   ├── armor_hook.h              ← Hook Player::tick, baca NBT armor
│           │   └── player_hook.h             ← Yaw, CPS, Sprint, Freelook, Fullbright
│           └── utils/
│               ├── offset_resolver.h         ← Tabel offset per versi MCPE
│               └── memory_utils.h            ← Safe ptr read/write, pattern scan
└── third_party/
    └── Dobby/                                ← ARM64 inline hook framework
```

---

## 🔧 Cara Kerja Setiap Fitur

### FPS Display
```
RenderDragon::_doRender() ─── [HOOK] ──► hookedFrameTick()
                                              │
                                    Hitung delta time antar frame
                                              │
                                    Update atomic<int> g_fps setiap 0.5s
                                              │
                                    HudManager baca via JNI getHudData()
                                              │
                                    FpsHudView.update(fps) → tampil di layar
```

### Zoom (FOV)
```
User tekan tombol Zoom virtual
        │
NeptuneHookBridge.setZoomFov(30.0f)
        │
        ▼ JNI ▼
g_features.zoom_fov.store(30.0f)
        │
LevelRenderer::getFov() ─── [HOOK] ──► hookedGetFov()
                                              │
                                    Lerp s_current_fov → 30.0f (smooth)
                                              │
                                    Return s_current_fov ke MCPE
                                    (kamera zoom in mulus)
```

### Durability Bar
```
Player::tick() ─── [HOOK] ──► hookedPlayerTick()
                                    │
                         Baca ArmorContainer dari Player offset
                                    │
                         Loop 4 slot armor:
                           • Baca ItemStack.damage (kerusakan)
                           • Panggil Item::getMaxDamage() via vtable
                           • Hitung percent = (max - damage) / max
                                    │
                         Simpan ke atomic<float>[4]
                                    │
                         HudManager baca 100ms sekali
                                    │
                         DurabilityBarView render warna sesuai %
```

### Compass
```
LocalPlayer::moveRelative() ─── [HOOK] ──► hookedPlayerMove()
                                                │
                                      Baca float yaw dari Player+0x1B0
                                                │
                                      g_hud.yaw.store(yaw)
                                                │
                                      CompassHudView.update(yaw)
                                                │
                                      onDraw(): render tick + label N/E/S/W
                                      sesuai offset dari yaw saat ini
```

---

## ⚙️ Setup & Build

### Prerequisites
```bash
# 1. Android Studio Hedgehog atau lebih baru
# 2. NDK r25c+
# 3. CMake 3.22.1+
# 4. Clone Dobby sebagai submodule

git submodule add https://github.com/jmpews/Dobby third_party/Dobby
git submodule update --init --recursive
```

### Build
```bash
# Debug build
./gradlew assembleDebug

# Release build (termasuk strip symbols)
./gradlew assembleRelease

# Install ke device
./gradlew installDebug
```

### Update Offset Tabel (saat MCPE update)
1. Download `libminecraftpe.so` dari APK versi baru
2. Buka di **Ghidra** atau **IDA Pro**
3. Cari fungsi target via nama simbol atau byte signature
4. Catat offset baru
5. Tambahkan entry di `OffsetResolver::getTableFor()` dengan versi baru

---

## 🔒 Keamanan & Etika

Neptune Client dirancang sebagai **utilitas informatif**, bukan cheat:

| Fitur            | Tipe Data    | Asal Data        | Efek pada Server |
|-----------------|--------------|------------------|-----------------|
| FPS Display     | Read-only    | Frame timer lokal | ❌ Tidak ada    |
| CPS Display     | Read-only    | Input event lokal | ❌ Tidak ada    |
| Zoom            | Visual lokal | FOV kamera lokal  | ❌ Tidak ada    |
| Fullbright      | Visual lokal | Gamma/light lokal | ❌ Tidak ada    |
| Durability Bar  | Read-only    | NBT lokal         | ❌ Tidak ada    |
| Compass         | Read-only    | Yaw lokal         | ❌ Tidak ada    |
| Freelook        | Visual lokal | Rotasi kamera     | ❌ Tidak ada    |

Semua fitur hanya memodifikasi **tampilan lokal** atau membaca data yang
sudah ada di memori client. Tidak ada modifikasi packet, damage, atau
movement yang dikirim ke server.

---

## 📌 Catatan Pengembangan Selanjutnya

- [ ] **Shortcut System**: floating button yang bisa dikonfigurasi
- [ ] **Keystrokes Display**: visualisasi W/A/S/D
- [ ] **Motion Blur**: post-processing visual saat kamera berputar
- [ ] **Config persistence**: simpan pengaturan ke SharedPreferences
- [ ] **HUD Editor**: drag-and-drop posisi tiap elemen HUD
- [ ] **Auto offset detection**: pattern scanning sebagai fallback saat offset tidak diketahui
- [ ] **Multi-version offset updater**: script Python untuk otomatis extract offset dari APK baru
