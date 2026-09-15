# Prism Windows runtime

This directory contains the official x64 Windows release of
[Prism](https://github.com/ethindp/prism) version 0.18.2. The runtime was taken
from `prism-windows-x64.zip` attached to the upstream `v0.18.2` release.

- Release: https://github.com/ethindp/prism/releases/tag/v0.18.2
- Archive SHA-256: `31c02e3ef2260b4d3b11fb00132f8eb12bc147b5fb17031b580670b117ba7d23`
- `bin/prism.dll` SHA-256: `cb9712e11af9ebe96457dbf8f5daad4a6c359ae1f59cdf2663282b3a9cc9759c`

The DLL is loaded dynamically by the Windows accessibility speech backend. It
is not linked into the game executable. `NOTICE` and `LICENSES` are preserved
from the official archive and must accompany redistributed copies.
