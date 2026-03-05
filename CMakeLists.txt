cmake_minimum_required(VERSION 3.16)
project(Glasspectum VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Platform detection ─────────────────────────────────────────
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
endif()

# ── Include paths ──────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# ── Source files ───────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/glasspectum_plugin.cpp
    src/glasspectum_processor.cpp
    src/lens_profile_db.cpp
    src/color_pipeline.cpp
    src/trait_mixer.cpp
)

# GPU sources
if(APPLE)
    list(APPEND PLUGIN_SOURCES src/gpu/metal_renderer.mm)
endif()

# ── CUDA support (Windows) ────────────────────────────────────
option(ENABLE_CUDA "Enable CUDA GPU support" OFF)
if(ENABLE_CUDA AND NOT APPLE)
    enable_language(CUDA)
    list(APPEND PLUGIN_SOURCES src/gpu/cuda_renderer.cu)
    add_definitions(-DGLASSPECTU_HAS_CUDA)
endif()

# ── OpenCL support (Windows fallback) ─────────────────────────
option(ENABLE_OPENCL "Enable OpenCL GPU support" OFF)
if(ENABLE_OPENCL AND NOT APPLE)
    find_package(OpenCL)
    if(OpenCL_FOUND)
        list(APPEND PLUGIN_SOURCES src/gpu/opencl_renderer.cpp)
        add_definitions(-DGLASSPECTU_HAS_OPENCL)
    endif()
endif()

# ── Build the OFX plugin (shared library) ─────────────────────
add_library(Glasspectum SHARED ${PLUGIN_SOURCES})

# OFX plugins are .ofx files (renamed .dylib/.dll)
set_target_properties(Glasspectum PROPERTIES
    PREFIX ""
    SUFFIX ".ofx"
    OUTPUT_NAME "Glasspectum"
)

# ── Platform-specific linking ──────────────────────────────────
if(APPLE)
    target_link_libraries(Glasspectum PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework Foundation"
        "-framework CoreGraphics"
    )

    # Compile Metal shaders to metallib (optional — requires Xcode metal CLI)
    find_program(METAL_COMPILER xcrun)
    option(ENABLE_METAL_SHADERS "Compile Metal shaders (requires Xcode)" ON)

    if(METAL_COMPILER AND ENABLE_METAL_SHADERS)
        set(METAL_SHADER ${CMAKE_SOURCE_DIR}/shaders/glasspectum.metal)
        set(METAL_AIR ${CMAKE_BINARY_DIR}/glasspectum.air)
        set(METAL_LIB ${CMAKE_BINARY_DIR}/glasspectum.metallib)

        add_custom_command(
            OUTPUT ${METAL_AIR}
            COMMAND xcrun -sdk macosx metal -c ${METAL_SHADER} -o ${METAL_AIR}
            DEPENDS ${METAL_SHADER}
            COMMENT "Compiling Metal shaders"
        )
        add_custom_command(
            OUTPUT ${METAL_LIB}
            COMMAND xcrun -sdk macosx metallib ${METAL_AIR} -o ${METAL_LIB}
            DEPENDS ${METAL_AIR}
            COMMENT "Building Metal library"
        )
        add_custom_target(MetalShaders ALL DEPENDS ${METAL_LIB})
        add_dependencies(Glasspectum MetalShaders)
        set(HAS_METAL_SHADERS TRUE)
    else()
        message(STATUS "Metal shader compiler not found — plugin will use runtime shader compilation")
        set(HAS_METAL_SHADERS FALSE)
    endif()

    # Visibility
    set_target_properties(Glasspectum PROPERTIES
        CXX_VISIBILITY_PRESET default
    )
elseif(WIN32)
    if(ENABLE_CUDA AND TARGET CUDA::cudart)
        target_link_libraries(Glasspectum PRIVATE CUDA::cudart)
    endif()
    if(ENABLE_OPENCL AND OpenCL_FOUND)
        target_link_libraries(Glasspectum PRIVATE OpenCL::OpenCL)
    endif()
endif()

# ── OFX Bundle creation (macOS) ───────────────────────────────
if(APPLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle")
    set(BUNDLE_CONTENTS "${BUNDLE_DIR}/Contents")
    set(BUNDLE_MACOS "${BUNDLE_CONTENTS}/MacOS")
    set(BUNDLE_RESOURCES "${BUNDLE_CONTENTS}/Resources")

    add_custom_command(TARGET Glasspectum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_MACOS}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_RESOURCES}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Glasspectum> ${BUNDLE_MACOS}/Glasspectum.ofx
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/shaders/glasspectum.metal ${BUNDLE_RESOURCES}/glasspectum.metal
        COMMENT "Creating OFX bundle"
    )

    if(HAS_METAL_SHADERS)
        add_custom_command(TARGET Glasspectum POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${METAL_LIB} ${BUNDLE_RESOURCES}/glasspectum.metallib
            COMMENT "Copying metallib to bundle"
        )
    endif()

    # Generate Info.plist
    file(WRITE "${CMAKE_BINARY_DIR}/Info.plist"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>English</string>
    <key>CFBundleExecutable</key><string>Glasspectum.ofx</string>
    <key>CFBundleIdentifier</key><string>com.glasspectum.lensemulator</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>Glasspectum</string>
    <key>CFBundlePackageType</key><string>BNDL</string>
    <key>CFBundleVersion</key><string>${PROJECT_VERSION}</string>
</dict>
</plist>")

    add_custom_command(TARGET Glasspectum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/Info.plist ${BUNDLE_CONTENTS}/Info.plist
        COMMENT "Copying Info.plist to bundle"
    )
endif()

# ── Linux bundle creation ─────────────────────────────────────
if(UNIX AND NOT APPLE)
    set_target_properties(Glasspectum PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle")
    set(BUNDLE_CONTENTS "${BUNDLE_DIR}/Contents")
    set(BUNDLE_LINUX "${BUNDLE_CONTENTS}/Linux-x86-64")

    add_custom_command(TARGET Glasspectum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_LINUX}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Glasspectum> ${BUNDLE_LINUX}/Glasspectum.ofx
        COMMENT "Creating Linux OFX bundle"
    )
endif()

# ── Windows bundle creation ───────────────────────────────────
if(WIN32)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle")
    set(BUNDLE_CONTENTS "${BUNDLE_DIR}/Contents")
    set(BUNDLE_WIN64 "${BUNDLE_CONTENTS}/Win64")

    add_custom_command(TARGET Glasspectum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_WIN64}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Glasspectum> ${BUNDLE_WIN64}/Glasspectum.ofx
        COMMENT "Creating Windows OFX bundle"
    )
endif()

# ── Install target ─────────────────────────────────────────────
if(APPLE)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle"
            DESTINATION "/Library/OFX/Plugins"
            USE_SOURCE_PERMISSIONS)
elseif(WIN32)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle"
            DESTINATION "C:/Program Files/Common Files/OFX/Plugins"
            USE_SOURCE_PERMISSIONS)
elseif(UNIX)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/Glasspectum.ofx.bundle"
            DESTINATION "/usr/OFX/Plugins"
            USE_SOURCE_PERMISSIONS)
endif()

# ── Tests ──────────────────────────────────────────────────────
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ── QA Tools ───────────────────────────────────────────────────
option(BUILD_TOOLS "Build QA tools" ON)
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()
