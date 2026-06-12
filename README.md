<p align="center">
  <img src="resources/icons/pixora-256.png" width="128" alt="Pixora logo"/>
</p>

<h1 align="center">Pixora</h1>

<p align="center">
  Screenshot · Annotate · Pin · <b>Scrolling capture</b> — a fast, no-fuss screenshot tool that treats scrolling capture as a first-class citizen
</p>

<p align="center">
  <a href="https://github.com/Yuilona/Pixora/releases/latest"><img src="https://img.shields.io/github/v/release/Yuilona/Pixora" alt="Release"/></a>
  <a href="https://github.com/Yuilona/Pixora/actions/workflows/ci.yml"><img src="https://github.com/Yuilona/Pixora/actions/workflows/ci.yml/badge.svg" alt="CI"/></a>
  <img src="https://img.shields.io/badge/platform-Windows-0078D6" alt="Platform"/>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20"/>
  <img src="https://img.shields.io/badge/Qt-6-41CD52" alt="Qt 6"/>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/Yuilona/Pixora" alt="License"/></a>
</p>

---

## Features

**Capture**
- Global `F1` hotkey freezes the screen — what you see is what you get; hover to snap to windows, hold `Shift` to snap down to **individual UI elements** (buttons, inputs — via UIAutomation)
- Free selection with 8-way resize handles; arrow keys nudge by 1px, `Ctrl+arrows` resize by 1px, `Shift` for ×10 steps; `Ctrl+A` (or clicking empty desktop) grabs the whole screen
- Magnifier with live coordinates and pixel color; press `C` to copy HEX (`Shift+C` for RGB) — a color picker built right in

**Annotate**
- Rectangle / ellipse / arrow / pen / marker / text / numbered badge / mosaic / blur — a nine-tool icon toolbar
- Every item stays editable: drag to resize, restyle color and width, double-click text to rewrite — with full undo/redo

**Pin**
- Pin a capture to the screen in one click, or press `F3` to pin the clipboard; wheel to zoom, drag edges to resize, `Ctrl+wheel` for opacity
- `Space` folds into a slim bar, `R` rotates, `H` flips, click-through mode available
- Pins **survive restarts** — position, scale, opacity and folded state are all remembered

**Scrolling capture**
- One toolbar click switches a selection into scroll-stitching mode; manual and auto-scroll, 30fps frame grabbing keeps up with fast scrolling
- Multi-band NCC matching with sticky header/footer detection; a live preview strip sits beside the capture area so misalignment is visible immediately
- Falls back to PageDown injection when wheel events are ignored; copy / pin / save outlets

**Extract text & translate in place**
- **Extract text**: OCR the selection straight to your clipboard
- **Translate**: OCR each line with its position, translate, then redraw the translations over the original text — the result is pinned exactly over the source region, as if the page translated itself; press `Esc` to restore
- Bring your own services, configured in Settings: OpenAI-compatible vision/chat models, Umi-OCR (local), DeepL, or Baidu Translate

**Output & history**
- Filename templates (`{yyyy}{MM}{dd}{HH}{mm}{ss}`), PNG/JPEG with quality control, optional auto-save on copy
- Capture history keeps recent shots — browse from the tray to re-copy, pin or save them again

## Install

Download `Pixora-x.y.z-win64-portable.zip` from [Releases](https://github.com/Yuilona/Pixora/releases/latest), unzip and run — no installation required.

> Builds are not code-signed yet. If Windows SmartScreen objects, choose "More info → Run anyway".

## Default hotkeys

| Key | Action |
|---|---|
| `F1` | Capture (while stitching: finish & copy) |
| `F3` | Pin clipboard image |
| `Ctrl+A` | Select the entire desktop (hovering empty desktop snaps to the screen too) |
| `Enter` / double-click | Copy selection and finish |
| `Ctrl+S` | Save as… |
| `C` / `Shift+C` | Pick color (HEX / RGB) |
| `Esc` | Step back (tool → selection → session) |

Hotkeys, save directory, history size and autostart are all configurable via tray → Settings.

## Build from source

```bash
git clone https://github.com/Yuilona/Pixora.git
cd Pixora
cmake --preset win-debug   # requires Qt 6.5+, vcpkg (VCPKG_ROOT), MSVC 2022
cmake --build --preset win-debug
ctest --preset win-debug
```

Tech stack: C++20 · Qt 6 (Widgets) · CMake + vcpkg · OpenCV (template matching for stitching)

## License

[GPL-3.0](LICENSE) — free to use, study and modify; derivative works must remain open source.
