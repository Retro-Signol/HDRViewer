cmake_minimum_required(VERSION 3.28)
project(HDRViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(HDRViewer WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/ImageDecoder.cpp
    src/ColorPipeline.cpp
    src/HDRDisplay.cpp
    src/Renderer.cpp
    src/RegistrySetup.cpp
    src/SlideShow.cpp
    src/Resource.rc
)

target_include_directories(HDRViewer PRIVATE
    src
    thirdparty
)

# stb_image
set(STB_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
target_sources(HDRViewer PRIVATE "${STB_IMAGE_DIR}/stb_image.cpp")

target_compile_definitions(HDRViewer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_compile_options(HDRViewer PRIVATE
    /O2 /GL /GR- /EHs-c-
    /utf-8
    $<$<CONFIG:Debug>:/Od /Zi>
)

target_link_options(HDRViewer PRIVATE
    /LTCG /OPT:REF /OPT:ICF /MANIFEST:NO
    $<$<CONFIG:Debug>:/DEBUG>
)

target_link_libraries(HDRViewer PRIVATE
    d2d1
    d3d11
    dxgi
    dwrite
    windowscodecs
)

# --- vcpkg installed libs ---
if(EXISTS "$ENV{VCPKG_ROOT}" OR DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_BASE "$ENV{VCPKG_ROOT}")
else()
    set(VCPKG_BASE "D:/Tools/_System/vcpkg")
endif()
set(VCPKG_LIB "${VCPKG_BASE}/installed/x64-windows-static/lib")
set(VCPKG_INC "${VCPKG_BASE}/installed/x64-windows-static/include")

target_include_directories(HDRViewer PRIVATE
    "${VCPKG_INC}"
    "${VCPKG_INC}/Imath"
)

# libjxl 及其传递依赖
find_library(JXL_LIB        jxl             PATHS "${VCPKG_LIB}")
find_library(JXL_THREADS    jxl_threads     PATHS "${VCPKG_LIB}")
find_library(JXL_CMS        jxl_cms         PATHS "${VCPKG_LIB}")
find_library(BROTLI_DEC     brotlidec       PATHS "${VCPKG_LIB}")
find_library(BROTLI_COMMON  brotlicommon    PATHS "${VCPKG_LIB}")
find_library(HWY_LIB        hwy             PATHS "${VCPKG_LIB}")

if(JXL_LIB)
    target_link_libraries(HDRViewer PRIVATE
        "${JXL_LIB}" "${JXL_THREADS}" "${JXL_CMS}"
        "${BROTLI_DEC}" "${BROTLI_COMMON}"
        "${HWY_LIB}"
    )
    target_compile_definitions(HDRViewer PRIVATE HAS_JXL)
    message(STATUS "Found libjxl (with transitive deps)")
endif()

# OpenEXR 及其传递依赖
find_library(OPENEXR_MAIN   OpenEXR-3_4     PATHS "${VCPKG_LIB}")
find_library(OPENEXR_CORE   OpenEXRCore-3_4 PATHS "${VCPKG_LIB}")
find_library(OPENEXR_UTIL   OpenEXRUtil-3_4 PATHS "${VCPKG_LIB}")
find_library(IEX_LIB        Iex-3_4         PATHS "${VCPKG_LIB}")
find_library(ILMTHREAD_LIB  IlmThread-3_4   PATHS "${VCPKG_LIB}")
find_library(DEFLATE_LIB    deflatestatic   PATHS "${VCPKG_LIB}")
find_library(IMATH_LIB      Imath-3_2       PATHS "${VCPKG_LIB}")
find_library(OPENJPH_LIB    openjph.0.27    PATHS "${VCPKG_LIB}")

if(OPENEXR_MAIN)
    target_link_libraries(HDRViewer PRIVATE
        "${OPENEXR_MAIN}" "${OPENEXR_CORE}" "${OPENEXR_UTIL}"
        "${IEX_LIB}" "${ILMTHREAD_LIB}"
        "${DEFLATE_LIB}"
        "${IMATH_LIB}"
        "${OPENJPH_LIB}"
    )
    target_compile_definitions(HDRViewer PRIVATE HAS_OPENEXR)
    message(STATUS "Found OpenEXR (with transitive deps)")
endif()
