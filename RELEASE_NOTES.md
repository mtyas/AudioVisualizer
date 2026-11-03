# AudioVisualizer 0.77

First public release. A sound-reactive visual plugin (VST3 / CLAP / AU / Standalone) with its own output window.

## Highlights
- **Up to 13 layers, you choose the kind:** 3D scene, 2D shader, spectrum analyzer, gradient / fill, video & loop, webcam FX and text.
- **Webcam as a direct source** for the Video & Loop layer, as well as the recorded live loop.
- **11 shapes, 11 3D patterns, 12 built-in 2D generators**, plus a live GLSL / ISF shader editor.
- **Fonts:** every text layer has its own text and any installed font.
- **Modulation:** envelopes, LFOs, chaos, sequencers, tempo clocks, noise, drum triggers (kick / snare / hat), MIDI, mouse, webcam pose and more can drive any setting. Right-click any slider to assign one.
- **Style page:** feedback, bloom, kaleidoscope, twirl, ripple, zoom blur, glitch, grain, posterize, duotone and a colour grade.
- **42 factory presets**, search across every setting (Ctrl+F), undo / redo, and a node graph with script and WASM nodes.
- The output window can be pinned **always on top** (press T in it).

## Downloads
- `AudioVisualizer-0.77-windows.zip` - VST3, CLAP, standalone `.exe`
- `AudioVisualizer-0.77-macos.zip` - VST3, AU, CLAP, standalone `.app` (universal: Apple silicon + Intel)
- `AudioVisualizer-0.77-linux.zip` - VST3, CLAP, standalone

## Notes
- The macOS build is **not signed or notarized**. After copying it, run `xattr -dr com.apple.quarantine <file>` on the plugin or app, or right-click > Open.
- Video **file** playback uses Windows Media Foundation, so on macOS and Linux only still images load. Webcam capture works on Windows and macOS (JUCE has no camera API on Linux).
- Only the Windows build has been tested by hand so far; macOS and Linux are built by CI and reports are welcome.
