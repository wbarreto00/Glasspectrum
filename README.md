# Glasspectum

**Lens emulation OFX plugin for DaVinci Resolve 18.6+**

Glasspectum emulates optical lens characteristics in post-production. 50 cinema lens presets with trait-level mixing, GPU-accelerated processing (Metal primary, CPU fallback), and an ACES-compatible color pipeline.

## Features

- **50 lens presets** — from modern cinema primes (Master Prime, Signature Prime, Supreme Prime) to vintage character lenses (Helios 44-2, Super Takumar, Canon K35) to anamorphics (Cooke Ana/i, Panavision T/C/E/G-Series, Hawk, Kowa, LOMO)
- **11-stage processing pipeline** — distortion, chromatic aberration, coma, depth-of-field bokeh blur, vignette & color cast, bloom/veiling glare, fringing, f-stop sharpener, master blend
- **GPU-accelerated** — Metal compute shaders (macOS primary), CUDA/OpenCL (Windows, planned)
- **Mix-and-match** — override any trait group with a different lens preset
- **Color-managed** — 11 input color spaces (sRGB, Rec.709, HLG, ACEScc/cct/cg, LogC3, S-Log3, V-Log, Canon Log), all processing in scene-linear, no highlight clipping
- **Physically grounded DOF** — thin-lens CoC model with 9 sensor size presets

## Build (macOS)

```bash
cd Glasspectum
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

This produces:
- `build/Glasspectum.ofx.bundle/` — ready to install
- `build/tests/test_lens_db` — lens DB validation
- `build/tests/test_pipeline` — pipeline unit tests
- `build/tools/glasspectum_qa` — QA comparison tool

## Install

```bash
# macOS
sudo cp -R build/Glasspectum.ofx.bundle /Library/OFX/Plugins/

# Or via CMake:
sudo cmake --install build
```

Restart DaVinci Resolve. Find **Glasspectum** under OpenFX effects.

## Build (Windows)

```powershell
mkdir build && cd build
cmake .. -DENABLE_CUDA=ON   # NVIDIA GPU
# or
cmake .. -DENABLE_OPENCL=ON # Non-NVIDIA GPU
cmake --build . --config Release
```

Install to `C:\Program Files\Common Files\OFX\Plugins\Glasspectum.ofx.bundle\Contents\Win64\`.

## Parameter UI Order

| # | Parameter | Type |
|---|-----------|------|
| 1 | Input Color Space | Dropdown (11 options) |
| 2 | Lens | Dropdown (50 presets) |
| 3 | Master Blend | Slider 0–1 |
| 4 | f-Stop Sharpener | Slider |
| 5 | Sensor Size | Dropdown (9 presets) |
| 6 | Overdrive | Slider |
| 7 | **Individual Controls** | Group |
| 8 | **Settings & Quality** | Group |
| 9 | Scale to Frame | Checkbox |
| 10 | Aberration Max Steps | Int slider 1–128 |
| 11 | Depth Gain | Slider |
| 12 | Bokeh Swirl | Slider |
| — | **Advanced** | Group (10 controls) |

## Processing Pipeline

All processing in scene-linear order:

1. Input color space → linear
2. Distortion warp (Brown–Conrady k1/k2/k3 + tangential p1/p2)
3. Lateral chromatic aberration (radial RGB displacement)
4. Coma (directional anisotropic blur, radially oriented)
5. Depth-of-field bokeh blur (CoC-based, golden-angle spiral sampling)
6. Vignette (exposure falloff in stops) + edge color cast
7. Bloom / veiling glare (multi-scale thresholded blur)
8. Fringing (edge-detected colored fringe)
9. f-Stop sharpener (MTF microcontrast with highlight protection)
10. Master blend (linear-light mix with original)
11. Output conversion

## QA Tool

```bash
# List presets
./build/tools/glasspectum_qa list

# Compare two images (SSIM + edge MAE)
./build/tools/glasspectum_qa compare reference.raw test.raw 3840 2160

# Calibrate against reference
./build/tools/glasspectum_qa calibrate reference.raw 3840 2160 0
```

## Architecture

```
src/
├── glasspectum_plugin.cpp      # OFX entry point + parameter UI
├── glasspectum_processor.cpp   # CPU rendering pipeline (fallback)
├── lens_profile_db.cpp         # 50 embedded presets (static_assert count==50)
├── color_pipeline.cpp          # 11 color space conversions
├── trait_mixer.cpp             # Mix-and-match + overdrive scaling
├── dof_engine.h                # Thin-lens CoC computation
├── sensor_table.h              # 9 sensor size presets
└── gpu/
    ├── metal_renderer.mm       # Metal GPU dispatch (PRIMARY)
    ├── cuda_renderer.cu        # CUDA (stub, planned)
    └── opencl_renderer.cpp     # OpenCL (stub, planned)
shaders/
└── glasspectum.metal           # 12 Metal compute kernels
```

## License

MIT — see [LICENSE](LICENSE).
