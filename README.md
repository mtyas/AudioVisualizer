<div align="center">

# 🎛️ AudioVisualizer
### sound-reactive visuals, by [mtyas](https://mtyas.com)

![banner](docs/img/banner.jpg)

[![Version](https://img.shields.io/badge/version-0.77-40d9ee)](../../releases/latest)
[![Build](https://github.com/mtyas/AudioVisualizer/actions/workflows/build.yml/badge.svg)](../../actions/workflows/build.yml)
![Platforms](https://img.shields.io/badge/Windows%20%7C%20macOS%20%7C%20Linux-a98bfb)
![Formats](https://img.shields.io/badge/VST3%20%7C%20CLAP%20%7C%20AU%20%7C%20Standalone-36d39b)
[![License](https://img.shields.io/badge/license-AGPLv3%20%2B%20MIT%2FCC0-f7b93e)](LICENSE)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20me-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/mtyas)
[![Website](https://img.shields.io/badge/mtyas.com-visit-40d9ee)](https://mtyas.com)

**[⬇️ Download](../../releases/latest)** · **[📖 Manual](docs/MANUAL.md)** · **[☕ Ko-fi](https://ko-fi.com/mtyas)** · **[🌐 mtyas.com](https://mtyas.com)**

</div>

---

## 👀 What is it?

AudioVisualizer is a free plugin that turns **sound into light**. Put it on a track (or run it standalone), and it paints a live, fully editable picture in its own window, which can **stay on top of your DAW**. Stack up to **13 layers**, animate every setting with **audio, MIDI, LFOs, chaos, your webcam or the mouse**, and start from **42 ready-made looks**.

![All 42 presets](docs/img/all_presets.jpg)
<sub>All 42 factory presets, captured straight from the output window.</sub>

## 🚀 Quick start

1. **[Download](../../releases/latest)** the zip for your system and copy the plugin into your plugin folder (or just run the standalone app).
2. Put it on a track so it can hear audio. Add it on a **bus or aux** to react to a whole mix. Sidechain inputs are available too.
3. Click **Open Output**. Leave **Always on top** switched on and the picture floats above your DAW. Press **F11** in the output window for fullscreen.
4. Pick a preset from the top bar, then explore. Press **`?`** for a quick tour and **`Ctrl+F`** to find any setting.

## 🧭 Five steps, left to right

```mermaid
flowchart LR
  A["🎧 Audio, MIDI,<br/>webcam, mouse"] --> B["📈 Sources<br/>(Animate)"]
  B --> C["🔀 Matrix &<br/>node graph"]
  C --> D["🧱 Layers (Compose)<br/>+ effects (Style)"]
  D --> E["🖥️ Output window"]
```

| | Page | What you do there |
|---|---|---|
| 1️⃣ | **Compose** | Stack layers: 3D scenes, shaders, spectrum analyzers, gradients, video, webcam, text |
| 2️⃣ | **Style** | Feedback, bloom, kaleidoscope, twirl, glitch, grain, duotone, colour grade |
| 3️⃣ | **Animate** | Envelopes, LFOs, chaos, sequencers and the modulation matrix |
| 4️⃣ | **Patch** | Node graph with remap, smooth, math, script and WASM nodes *(advanced)* |
| 5️⃣ | **Library** | Browse, save, import and export presets |

![Compose page](docs/img/ui_compose.png)

<details>
<summary><b>📸 See the other pages</b></summary>

| Style | Animate |
|---|---|
| ![Style](docs/img/ui_style.png) | ![Animate](docs/img/ui_animate.png) |
| **Patch** | **Library** |
| ![Patch](docs/img/ui_patch.png) | ![Library](docs/img/ui_library.png) |

</details>

## ✨ Highlights

- 🧱 **Up to 13 layers, you pick the kind:** 3D scene · 2D shader · spectrum analyzer · gradient / fill · video & loop · webcam FX · text
- 🔷 **11 shapes** (sphere, cube, torus, torus knot, cone, pyramid…), **11 patterns** (spiral, helix, galaxy, tunnel…) and **your own glTF models**

  ![Shapes](docs/img/shapes.jpg)
  ![Patterns](docs/img/patterns.jpg)

- 🎆 **12 built-in 2D generators** plus a **live GLSL / ISF shader editor**

  ![Generators](docs/img/generators.jpg)

- 🔤 **Fonts:** every text layer has its own words and **any font installed on your computer**, with a live preview

  ![Layers and fonts](docs/img/ui_layers_text.png)

- 📹 **Webcam as a direct source** for the Video layer, or record it into a loop that follows the host tempo
- 🎚️ **Modulate anything:** right-click any slider and pick a source. There are **100+ sources**: audio levels and spectrum bands, kick / snare / hat triggers, MIDI (including aftertouch and mod wheel), tempo clocks, noise, LFOs, chaos attractors, sequencers, webcam hand / head tracking, mouse position…
- 🔍 **Find any setting** with `Ctrl+F`, undo / redo everything, tooltips everywhere

  ![Search](docs/img/ui_search.png)

- 📌 **Output window that stays on top** of your DAW (press `T` in it to toggle), with fullscreen
- 🧪 **42 factory presets** and a library for your own

## 🖼️ Gallery

| | |
|---|---|
| ![Phyllotaxis](docs/img/preset_phyllotaxis.jpg) | ![Mandala](docs/img/preset_mandala.jpg) |
| ![Torus knot](docs/img/preset_torus-knot.jpg) | ![Pyramid tunnel](docs/img/preset_pyramid-tunnel.jpg) |
| ![Synthwave](docs/img/preset_synthwave.jpg) | ![Outrun](docs/img/preset_outrun.jpg) |
| ![Bass riot](docs/img/preset_bass-riot.jpg) | ![Kinetic type](docs/img/preset_kinetic-type.jpg) |

## 📦 Downloads

Get the latest build from the **[Releases page](../../releases/latest)**.

| System | You get |
|---|---|
| 🪟 **Windows** | VST3 · CLAP · standalone `.exe` |
| 🍎 **macOS** (Apple silicon + Intel) | VST3 · AU · CLAP · standalone `.app` |
| 🐧 **Linux** | VST3 · CLAP · standalone |

> **macOS:** the build is not signed or notarized. Run `xattr -dr com.apple.quarantine <file>` once, or right-click → Open.
>
> **Platform notes:** video *file* playback uses Windows Media Foundation, so on macOS and Linux only still images load. Webcam capture works on Windows and macOS (JUCE has no camera support on Linux).

The **[manual](docs/MANUAL.md)** covers installing, every page and every layer type.

## 🛠️ Build from source

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Needs CMake 3.22+ and a C++20 compiler. On Linux install the usual JUCE dependencies (ALSA, X11, Freetype, Fontconfig, OpenGL dev packages). JUCE, clap-juce-extensions and tinygltf are fetched automatically. GitHub Actions builds all three platforms on every push, and tagging `v*` publishes a release.

## 👋 About me

Hi, I'm **mtyas**. AudioVisualizer is a project I made and give away for free.

- 🌐 **Website:** [mtyas.com](https://mtyas.com)
- ☕ **Ko-fi:** [ko-fi.com/mtyas](https://ko-fi.com/mtyas) · if it's useful to you, a coffee helps me keep building
- 💻 **GitHub:** [@mtyas](https://github.com/mtyas)
- 🐞 **Bugs & ideas:** [open an issue](../../issues)

The plugin has the same links on its **About & support** page (bottom of the left rail, or click the logo).

[![Support me on Ko-fi](https://img.shields.io/badge/Support%20me%20on%20Ko--fi-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/mtyas)

## 📜 License

The core engine is **AGPLv3**. Everything in [`ContentPack/`](ContentPack) (shaders, scripts, WASM modules, glTF models and presets) is **MIT / CC0**. See [LICENSE](LICENSE). Built with [JUCE](https://juce.com), [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) and [tinygltf](https://github.com/syoyo/tinygltf).
