# Glasspectrum — guia para o Claude

OFX plugin de emulação de lentes para DaVinci Resolve 18.6+. C++17 + GPU. ~44 arquivos.
Backend GPU primário: **Metal** (macOS). CUDA/OpenCL são stubs planejados. CPU é o fallback.

## Mapa do código (onde está o quê — leia direcionado, não o arquivo todo)
- `src/glasspectrum_plugin.cpp` — entry point OFX + definição da UI de parâmetros.
- `src/glasspectrum_processor.cpp/.h` — pipeline de render na CPU (fallback), 11 estágios.
- `src/lens_profile_db.cpp/.h` — 50 presets de lentes embutidos. `static_assert(count == 50)`: ao adicionar/remover preset, ajuste a contagem.
- `src/color_pipeline.cpp/.h` — 11 conversões de color space (sRGB, Rec.709, ACEScc/cct/cg, LogC3, S-Log3, V-Log, Canon Log...). Tudo processado em scene-linear.
- `src/trait_mixer.cpp/.h` — mix-and-match de traits entre lentes + escala de overdrive.
- `src/dof_engine.h` — modelo thin-lens (CoC). `src/sensor_table.h` — 9 presets de sensor.
- `src/gpu/metal_renderer.mm` — dispatch Metal (PRIMÁRIO). `cuda_renderer.cu`, `opencl_renderer.cpp` — stubs.
- `shaders/glasspectrum.metal` — 12 kernels de compute Metal. `.cu`/`.cl` espelham (stubs).
- `include/ofx/` — headers do OFX SDK (terceiros; não editar).
- `tests/` — `test_lens_db.cpp`, `test_pipeline.cpp`. `tools/glasspectrum_qa.cpp` — comparador SSIM/edge-MAE.

## Build / teste (macOS)
```bash
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(sysctl -n hw.ncpu)
./tests/test_lens_db && ./tests/test_pipeline
```
Linux: `cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)`.
Windows: `cmake .. -G "Visual Studio 17 2022" -A x64 -DENABLE_CUDA=ON` (ou `-DENABLE_OPENCL=ON`).

## Ordem do pipeline (scene-linear)
color→linear → distorção (Brown–Conrady) → aberração cromática → coma → DOF bokeh → vinheta+color cast → bloom → fringing → f-stop sharpener → master blend → output.
Alterações na pipeline devem manter essa ordem e refletir igual em CPU (`glasspectrum_processor`) e Metal (`metal_renderer.mm` + `.metal`).

## Convenções / armadilhas
- Mudou um estágio na CPU? Espelhe no shader Metal e vice-versa — eles têm de bater.
- Não fazer clipping de highlight: processamento é linear-light.
- CI (`.github/workflows/build.yml`) só roda em tags `v*` ou manual; builda mac/linux/win e roda os 2 testes.
- Licença MIT.
