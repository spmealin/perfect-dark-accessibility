# Perfect Dark Accessibility

This repository is an accessibility-focused branch of the open-source
[Perfect Dark PC port](https://github.com/fgsfdsfgs/perfect_dark). It adds
screen-reader output, spatial audio navigation, object and character scanners,
aiming feedback, and other nonvisual interfaces while preserving the original
gameplay.

The project is being developed and tested primarily with blind players. It is
already possible to navigate many menus, training activities, campaign levels,
and Combat Simulator matches without sight, but this remains a work in
progress. Expect occasional inaccessible objects, unclear audio cues, and
level-specific gaps. Testing and detailed reports are welcome.

The accessibility work does not include Perfect Dark or any extracted game
assets. You must provide your own legally obtained supported ROM.

## Start here

Blind and screen-reader users should read the
[practical accessibility guide](ACCESSIBILITY.md) before playing. It explains
setup, the major speech and audio systems, all accessibility keyboard
shortcuts, configuration, and how to capture a useful diagnostic when
something goes wrong.

For the easiest Windows setup:

1. Download an accessibility-enabled Windows package from this fork's Releases
   page, or ask the project maintainer for the latest test package.
2. Extract the whole archive. Keep the executable and all supplied DLL files
   together.
3. Put a supported US revision 1 ROM in the package's `data` directory and
   name it `pd.ntsc-final.z64`.
4. Start your screen reader before launching `pd.x86_64.exe`.
5. Headphones are strongly recommended because most navigation and targeting
   cues are spatial.

If the main menu is not spoken, see
[Troubleshooting](ACCESSIBILITY.md#troubleshooting).

## What the accessibility build adds

- Screen-reader narration for menus, mission briefings, objectives, HUD
  messages, weapon changes, results screens, and other important text.
- Spatial scanners for doors, interactable objects, pickups, friendly or
  neutral characters, enemies, hazards, and supported mission targets.
- A virtual cane that sweeps the space ahead and describes barriers, openings,
  drops, stairs, ramps, crouch passages, and ladders with sound.
- Responsive targeting feedback, including direction, elevation, body region,
  protected or breakable targets, special devices, scopes, and the FarSight.
- A spoken and sonified compass, four player-placeable audible markers, and
  authored mission landmarks.
- Accessible support for devices such as the CamSpy, R-Tracker, IR Scanner,
  X-Ray Scanner, Threat Detector, Data Uplink, Door Decoder, ECM Mine, and
  Reprogrammer.
- Combat Simulator radar queries, contact alerts, King of the Hill guidance,
  player and team status queries, and accessible scenario menus.

These systems communicate information already available through the game's
visual interface or established gameplay rules. They are intended to provide
equal access, not automated play, extra health, or hidden information.

## ROM requirements

The recommended ROM is:

- `ntsc-final`, also known as US V1.1 or US Rev 1
- Filename: `pd.ntsc-final.z64`
- MD5: `e03b088b6ac9e0080440efed07c1e40f`

The upstream port also supports these ROMs, though this accessibility branch is
developed and tested mainly with `ntsc-final`:

| Region/version | ROM filename | MD5 | Notes |
| --- | --- | --- | --- |
| US V1.0 | `pd.ntsc-1.0.z64` | `7f4171b0c8d17815be37913f535e4e93` | Supported but not recommended |
| Japanese final | `pd.jpn-final.z64` | `538d2b75945eae069b29c46193e74790` | Requires the matching custom executable |
| PAL final | `pd.pal-final.z64` | `d9b5cd305d228424891ce38e71bc9213` | Requires the matching custom executable |

Never commit or redistribute a ROM, extracted ROM data, save data, or generated
game assets with this project.

## Standard controls

The port supports keyboard and mouse, Xbox-style controllers, and N64-style
controllers. Controls can be rebound in `pd.ini`.

| Action | Keyboard and mouse | Xbox controller |
| --- | --- | --- |
| Fire / accept | Left mouse button or Space | Right trigger |
| Aim mode | Right mouse button or Z | Left trigger |
| Use / cancel | E | B for previous/cancel; A for use/accept |
| Reload | R | X |
| Previous weapon | Mouse wheel forward | B |
| Next weapon | Mouse wheel back | Y |
| Weapon radial menu | Q | Left bumper |
| Alternate fire mode | F | Right bumper |
| Reset vertical view | End | Right stick click |

See the [accessibility shortcut table](ACCESSIBILITY.md#keyboard-shortcuts) for
F1 through F12 commands.

## Building on Windows

The accessibility build currently targets Windows because its speech backend
uses [Tolk](https://github.com/dkager/tolk). Install MSYS2 and the MinGW64
toolchain, SDL2, zlib, CMake, Python, Make, and Git. Then configure and build
from the MSYS2 MinGW64 environment:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

The executable is `build/pd.x86_64.exe`. The build copies its required MinGW,
SDL2, zlib, Tolk, and NVDA Controller runtime DLLs beside the executable so it
can be launched from Windows Explorer.

To create a clean accessibility-enabled Windows package from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_windows_dist.ps1 -EnableAccessibility
```

The archive is written to `build/dist/pd.zip`. It intentionally excludes the
ROM, extracted assets, saves, logs, diagnostics, and personal `pd.ini`. Omitting
`-EnableAccessibility` creates a vanilla comparison package with accessibility
disabled.

For other platforms, ROM versions, dependencies, and upstream port details,
refer to the [upstream Perfect Dark port](https://github.com/fgsfdsfgs/perfect_dark).

## Project status and contributing

Current scope and progress are tracked in the
[accessibility roadmap](ACCESSIBILITY_ROADMAP.md). Developers should also read
the [architecture](ACCESSIBILITY_ARCHITECTURE.md),
[testing guide](ACCESSIBILITY_TESTING.md), and
[detailed feature reference](documentation/ACCESSIBILITY_FEATURE_REFERENCE.md).

The most valuable contribution is a precise blind-player test report: include
the level, difficulty, task, what cue or speech was expected, what happened,
and a Shift+F2 diagnostic capture number when possible. The diagnostic log is
local and comprehensive; review it before sharing if that matters to you.

## Credits and license notices

This branch builds on the work of the Perfect Dark decompilation and PC port
communities. Upstream credits include the
[Perfect Dark decompilation](https://github.com/n64decomp/perfect_dark), the
[Perfect Dark PC port](https://github.com/fgsfdsfgs/perfect_dark), `pd-extract`,
sm64-port audio work, libultraship/fast3d contributors, minimp3, 1964GEPD,
Mouse Injector, and everyone who has contributed fixes and testing.

Third-party notices for the accessibility speech components are provided in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and in packaged license files.
