# Third-party notices

## Prism

This project distributes the official x64 Windows runtime from
[Prism](https://github.com/ethindp/prism) version 0.18.2. Prism provides the
Windows screen-reader, OneCore, and SAPI speech backends used by the
accessibility layer.

`prism.dll` is a replaceable shared library loaded dynamically from beside the
game executable. Prism is licensed under the Mozilla Public License version
2.0. The upstream notice and complete license collection from the official
release archive are preserved under `third_party/prism/NOTICE` and
`third_party/prism/LICENSES`. Artifact provenance and checksums are recorded in
`third_party/prism/README.md`.

The repository's MIT license covers project-owned source. Prism and its bundled
third-party components retain their respective licenses.

## Experimental Steam Audio backend

The uncommitted binaural-audio experiment downloads the official Steam Audio
SDK 4.8.1 from Valve's release page, verifies its archive SHA-256, and uses its
Windows `phonon.dll` and public C headers from the ignored build dependency
directory. Steam Audio is licensed under Apache-2.0:
https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/LICENSE.md

The SDK and built-in HRTF are third-party audio technology, not Perfect Dark
ROM assets. Public ZIP creation is blocked while this experimental build
option is enabled. A release would require a complete SDK dependency/notice
audit and packaging integration first. No SDK binary or header is vendored
or committed by this experiment.
