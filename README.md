# P.Melodies

![P.Melodies — diseño de referencia](docs/design/png/01-vista-general-eq.png)

**ES** · Plugin VST3 para FL Studio que reúne en una sola ventana un EQ paramétrico, un compresor, una saturación multibanda y una distorsión, en una cadena que se reordena arrastrando los módulos.

**EN** · VST3 plugin for FL Studio that combines a parametric EQ, a compressor, multiband saturation and distortion in a single window, in a signal chain you reorder by dragging modules.

> **En desarrollo / Work in progress** — Fase 0 de 6: base común (motor de audio, cadena reordenable, sección global, interfaz base, CI). Los módulos todavía no procesan el audio. / Phase 0 of 6: shared foundation. The modules do not process audio yet.

## Español

### Módulos

| Módulo | Qué hace | Fase |
|---|---|---|
| EQ | Hasta 8 bandas, edición directa sobre la curva, analizador de espectro, latencia cero | 1 |
| Compresor | 8 estilos, lookahead, hold, range, sidechain externo con EQ | 2 |
| Saturación multibanda | Hasta 6 bandas con 16 estilos analógicos, drive, feedback, dynamics y tono por banda | 3 |
| Distorsión | Hard clip, foldback, rectificador, fuzz, bitcrusher y downsample | 4 |

Además hay una sección global: ganancia de entrada y salida con medidores, mix global, comparación a igual volumen, clipper de seguridad y calidad (oversampling).

### Descargar y probar el build de Windows

1. Ve a la pestaña **Actions** del repositorio y abre la última ejecución en verde del workflow **Build** de la rama que quieras probar.
2. En **Artifacts**, descarga **`P.Melodies-windows-installer`**, descomprímelo y ejecuta `P.Melodies-<versión>-windows-setup.exe`.
   - El instalador no está firmado, así que Windows mostrará «Windows protegió su PC»: pulsa *Más información → Ejecutar de todas formas*.
   - Copia el plugin a `C:\Program Files\Common Files\VST3` y se desinstala desde *Configuración → Aplicaciones → P.Melodies (VST3)*.
3. Sin instalador: descarga `P.Melodies-windows-vst3`, descomprímelo y copia la carpeta `P.Melodies.vst3` a `C:\Program Files\Common Files\VST3`.
4. En FL Studio: *Options → Manage plugins → Find installed plugins* y busca **P.Melodies** (fabricante: JoseFrMunoz).

### Compilar desde el código

Requisitos: CMake 3.25 o superior y un compilador C++20 (MSVC 2022, GCC 13 o Clang 17). JUCE 8 y Catch2 se descargan solos.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

En Linux hacen falta además las cabeceras de ALSA, FreeType, Fontconfig y X11 (ver `.github/workflows/build.yml`).

### Documentación

- [`docs/ESPECIFICACION.md`](docs/ESPECIFICACION.md): especificación completa, fases y decisiones aprobadas.
- [`docs/arquitectura.md`](docs/arquitectura.md): cómo está organizado el código, latencia, hilos y tests.
- [`docs/dsp/`](docs/dsp/): decisiones de DSP y referencias.
- [`docs/design/`](docs/design/): referencias visuales de la interfaz.
- [`CLAUDE.md`](CLAUDE.md): reglas de trabajo del proyecto.

## English

### Download and try the Windows build

1. Open the **Actions** tab, then the latest green run of the **Build** workflow for the branch you want.
2. Under **Artifacts**, download **`P.Melodies-windows-installer`**, unzip it and run the setup `.exe`. It is not code-signed, so Windows SmartScreen asks for confirmation (*More info → Run anyway*). Uninstall from *Settings → Apps*.
3. Or download `P.Melodies-windows-vst3` and copy the `P.Melodies.vst3` folder to `C:\Program Files\Common Files\VST3` by hand.
4. In FL Studio, rescan plugins (*Options → Manage plugins*) and look for **P.Melodies** (vendor: JoseFrMunoz).

### Build from source

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake ≥ 3.25 and a C++20 compiler. JUCE 8 (pinned tag) and Catch2 are fetched automatically. Every push is built and tested on Windows and Linux. The Windows build is also validated with pluginval at strictness level 10.

### Tech stack

C++20 · [JUCE 8](https://juce.com) · CMake · Catch2 · GitHub Actions · pluginval.

## Licencia / License

- Código / Code: [GNU AGPLv3](LICENSE).
- JUCE: AGPLv3 (licencia de código abierto de JUCE 8).
- Fuentes / Fonts: Inter, JetBrains Mono y Space Grotesk, bajo SIL Open Font License 1.1 ([`licenses/`](licenses/)).
