# HDRViewer

*Vibe coding Project  
  
HDR image viewer — native C++ / WinAPI desktop application, with native Windows 11 HDR display pipeline support (scRGB / ST.2084 / HLG), inspired by Jpegview.

## Features

- **Native HDR**: DXGI `R16G16B16A16_FLOAT` swap chain with automatic display HDR detection and ST.2084 color space configuration
- **Color pipeline**: sRGB / PQ (ST.2084) / HLG / Scene Linear → scRGB conversion, preserving full dynamic range  
*Triggers system Auto HDR on SDR displays via registry configuration

## Keyboard & Mouse

**Open images via**: drag & drop onto the window, Ctrl+O file dialog, command-line argument, or dropping a file onto the EXE icon

| Input | Action |
|-------|--------|
| Scroll wheel | Zoom |
| Left drag | Pan image |
| Double-click / F11 | Toggle fullscreen |
| Ctrl+O | Open file |
| 0 | Reset to 1:1 |
| ← ↑ / → ↓ | Previous / next image |
| Esc | Exit |
| Right-click | Context menu |

## Building

### Prerequisites

- Visual Studio 2022 (MSVC, CMake 3.28+)
- Windows 10/11 SDK
- [vcpkg](https://github.com/microsoft/vcpkg)

```powershell
# Install third-party libraries (static triplet recommended)
vcpkg install --triplet x64-windows-static libjxl openexr

# stb_image.h is bundled in thirdparty/ — no extra install needed
```

### Compile

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build\Release\HDRViewer.exe` (≤ 3 MB, statically linked)

### Run

```powershell
.\build\Release\HDRViewer.exe
.\build\Release\HDRViewer.exe "C:\path\to\image.jxr"
```

## Format Support

| Format | Decoder | HDR |
|--------|---------|-----|
| .jxl | libjxl | ✅ PQ / HLG |
| .exr | OpenEXR | ✅ Scene Linear |
| .jxr / .wdp / .hdp | WIC (HD Photo codec) | ✅ FP16 |
| .png / .jpg / .bmp / .tga / .gif | stb_image | SDR → HDR up |

## Architecture

```
src/
├── main.cpp              # WinMain, command-line parsing, message loop
├── MainWindow.h/cpp      # Window class: message dispatch, zoom/pan/fullscreen logic
├── ImageDecoder.h/cpp    # Unified decode interface, per-format adapters
├── ColorPipeline.h/cpp   # sRGB/PQ/HLG → scRGB color conversion
├── HDRDisplay.h/cpp      # DXGI HDR detection, FP16 swap chain
├── Renderer.h/cpp        # Direct2D/3D rendering, DWrite text overlay
├── SlideShow.h/cpp       # Slideshow (directory enumeration, sorting)
├── RegistrySetup.h/cpp   # Registry settings for Auto HDR / GPU preference
├── Resource.rc           # Icon, manifest
└── app.manifest          # DPI / Win10+ compatibility
```

## License

GPL-2.0 — consistent with JPEGView

## References

- [sylikc/jpegview](https://github.com/sylikc/jpegview) — interaction style blueprint
- [microsoft/DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples) — D3D12HDR reference
