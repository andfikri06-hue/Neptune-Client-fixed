// Neptune Client - app/build.gradle.kts
// Kotlin DSL build script

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.neptune.client"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.neptune.client"
        minSdk = 26          // Android 8.0 - minimum untuk TYPE_APPLICATION_OVERLAY
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        // NDK config
        externalNativeBuild {
            cmake {
                cppFlags += listOf(
                    "-std=c++17",
                    "-fexceptions",
                    "-frtti"
                )
                abiFilters += listOf(
                    "arm64-v8a",      // Primary target (sebagian besar HP modern)
                    "armeabi-v7a"     // Legacy 32-bit support
                )
            }
        }

        // Hanya build untuk ARM (bukan x86/x86_64 — emulator only)
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
            isMinifyEnabled = false
            // Tambahkan BuildConfig.DEBUG = true
            buildConfigField("boolean", "NEPTUNE_DEBUG", "true")
        }
        release {
            isDebuggable = false
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            buildConfigField("boolean", "NEPTUNE_DEBUG", "false")
        }
    }

    buildFeatures {
        buildConfig = true
        viewBinding = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    // Packaging: exclude file duplikat yang bikin build gagal
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
        jniLibs {
            // Keep Dobby dan libneptune dalam APK
            keepDebugSymbols += listOf("**/libneptune.so")
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")

    // Lifecycle (untuk Service management)
    implementation("androidx.lifecycle:lifecycle-service:2.7.0")

    // Coroutines (untuk async polling jika diperlukan)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
}
