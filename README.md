# AudioVisualizer

![version](https://img.shields.io/badge/version-0.77-blue) ![build](https://github.com/mtyas/AudioVisualizer/actions/workflows/build.yml/badge.svg)

Open-source audio-visualizer plugin (**VST3 / CLAP / AU / Standalone**) for Windows, macOS, and Linux.
Audio (Main + two sidechains), MIDI, video files, and webcam tracking drive a stack of up to 13 layers (3D scenes, shaders, spectrum analyzers, gradients, video, webcam, text), a modulation matrix and a node graph, shown in a floating, fullscreen-capable output window that can stay on top of your DAW.

License: **AGPLv3** (core engine). **MIT / CC0** for `ContentPack/` (shaders, scripts, WASM modules, glTF models, and presets).

## Download
Grab the latest zip for your system from the [Releases page](../../releases): Windows, macOS (universal) or Linux. Copy the `.vst3` / `.clap` (and `.component` on macOS) into your plugin folder, or just run the standalone app.

- macOS builds are unsigned: run `xattr -dr com.apple.quarantine <file>` once, or right-click > Open.
- Video **file** playback is Windows-only (Media Foundation); on macOS/Linux only still images load. Webcam capture works on Windows and macOS.

## Workflow
The app is organised as five steps: **Compose** (layers) > **Style** (effects) > **Animate** (modulation) > **Patch** (node graph, advanced) > **Library** (presets). Press `?` in the top bar for a short tour, `Ctrl+F` to search every setting, and right-click any slider to modulate it.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
Requires CMake 3.22+ and a C++20 compiler. On Linux install the usual JUCE dependencies (ALSA, X11, Freetype, Fontconfig, OpenGL dev packages). JUCE, clap-juce-extensions and tinygltf are fetched automatically.

## Features (Milestones M0 – M4 Complete)
- **M0 – Plugin & Floating Output**: VST3, CLAP, AU (macOS), and Standalone with detachable fullscreen OpenGL 3.2+ output window (`F11` / `Alt+Enter`).
- **M1 – Audio Analysis, Node Graph & Shaders**:
  - Lock-free audio analyzer: Input matrix (`Mid`, `Side`, `L`, `R`, `Sidechain 1`, `Sidechain 2`), Pre-Filter (`LP`, `HP`, `BP`), RMS, Peak, Envelope, Transient detector, 16-band FFT, Spectral Centroid, Spectral Flux, Stereo Width, and Host Tempo/Beat sync.
  - Interactive visual Node Graph Editor with Bezier patch cables.
  - Hot-reloaded Shadertoy (`.glsl`) and **ISF 2.0 (`.fs` / `.isf`)** fragment shaders.
- **M2 – MIDI, 3D Instanced Scene, Multi-Layer Compositor & Post-FX**:
  - Polyphonic MIDI tracker (`midi.gate`, `midi.note`, `midi.velocity`, `midi.pitchbend`, `midi.trigger`, per-note & CC signals).
  - 5 reorderable visual layers (`3D Scene`, `2D Shader Generator`, `Video & Live Looper`, `Webcam FX`, `3D Kinetic Text`) with 6 blend modes (`Add`, `Screen`, `Multiply`, `Overlay`, `Alpha`, `Difference`).
  - 5 geometries (`Sphere`, `Cube`, `Torus`, `Icosahedron`, `Custom glTF/GLB`) and 5 instanced spatial layouts (`Phyllotaxis Spiral`, `3D Grid`, `Torus Vortex`, `DNA Helix`, `Fibonacci Shell`).
  - 5-unit Post-FX Rack: Recursive Feedback Tunnel, HDR Bloom & ACES Tonemap, Kaleidoscope & Mirror, Glitch/RGB Split/CRT Scanlines, Master Color & Strobe.
- **M3 – Webcam Vision (Pose/Hands/Face) & Hardware Video / Tempo-Synced Live Loops**:
  - Dedicated vision worker thread publishing `pose.headX/Y`, `pose.leftHandX/Y`, `pose.rightHandX/Y`, `pose.handSpread`, `pose.bodyLean`, `pose.gestureWave`, `pose.proximity`, and `cam.motion/x/y` + on-demand `.onnx` model loader.
  - Hardware-accelerated video file playback (Windows Media Foundation `MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS` + image decoder) and GPU texture-array live loop recorder with Host Tempo Sync, Reverse, In/Out points, and Beat Stutter.
- **M4 – Ecosystem: Luau Scripting, Sandboxed WASM Nodes, Presets & Content Pack**:
  - Stateful **Luau/Lua-style Script Nodes** and sandboxed **WebAssembly (`.wasm`) Binary Nodes** inside the Node Graph.
  - Full **Undo / Redo** (`Ctrl+Z` / `Ctrl+Y`), **Detachable Modulation Matrix Window**, **42 Factory Presets**, User Preset Browser, and `ContentPack/` examples.
