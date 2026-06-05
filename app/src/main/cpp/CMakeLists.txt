# Neptune Client - CMakeLists.txt
# Build system untuk libneptune.so (native hook engine)
#
# Requirements:
#   - Android NDK r25c atau lebih baru
#   - Dobby hook framework (submodule di third_party/Dobby)
#   - Target: ARM64-v8a (primary), armeabi-v7a (opsional)
#
# Build command (dari Android Studio atau CLI):
#   ./gradlew assembleDebug
#   atau: cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#               -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 ..

cmake_minimum_required(VERSION 3.22.1)
project("neptune_client")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Optimization flags ────────────────────────────────────────────────────────
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fvisibility=hidden -ffunction-sections -fdata-sections")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")

# ── Dobby Hook Framework ──────────────────────────────────────────────────────
# Clone via: git submodule add https://github.com/jmpews/Dobby third_party/Dobby
add_subdirectory(${CMAKE_SOURCE_DIR}/../../../third_party/Dobby dobby_build)

# ── Neptune Source Files ──────────────────────────────────────────────────────
add_library(
    neptune          # nama output: libneptune.so
    SHARED

    # Engine utama
    neptune_hook_engine.cpp

    # Hook modules (header-only dengan static members, perlu .cpp stub)
    hooks/fps_hook_impl.cpp
    hooks/fov_hook_impl.cpp
    hooks/armor_hook_impl.cpp
    hooks/player_hook_impl.cpp

    # Utilities
    utils/offset_resolver_impl.cpp
    utils/memory_utils_impl.cpp
)

# ── Include Paths ─────────────────────────────────────────────────────────────
target_include_directories(neptune PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../../../third_party/Dobby/include
)

# ── Linked Libraries ──────────────────────────────────────────────────────────
target_link_libraries(neptune
    dobby                   # Hook framework
    android                 # Android native APIs
    log                     # __android_log_print
    dl                      # dlopen, dlsym, dlclose
)

# ── Build info define ─────────────────────────────────────────────────────────
target_compile_definitions(neptune PRIVATE
    NEPTUNE_VERSION="1.0.0"
    NEPTUNE_BUILD_TYPE="$<CONFIG>"
)

# ── Strip symbols in release (reduce APK size, hide internals) ────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET neptune POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded
                $<TARGET_FILE:neptune>
        COMMENT "Stripping libneptune.so symbols"
    )
endif()
