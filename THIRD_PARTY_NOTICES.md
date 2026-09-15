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
