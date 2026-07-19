# Third-party notices

## Tolk

This project uses [Tolk](https://github.com/dkager/tolk), a screen-reader abstraction library by Davy Kager and contributors, pinned at commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe`.

Tolk is built as a replaceable shared library and loaded dynamically by the Perfect Dark accessibility backend. Tolk remains licensed under the GNU Lesser General Public License version 3. The complete upstream license is available at `third_party/tolk/LICENSE.txt`, and the pinned corresponding source is the `third_party/tolk` Git submodule.

## NVDA controller client

The architecture-matching NVDA controller client DLL distributed in Tolk's `libs` directory is copied beside the game executable for development builds. It remains licensed under the GNU Lesser General Public License version 2.1. Its license is available at `third_party/tolk/LICENSE-NVDA.txt`.

The repository's MIT license covers project-owned source. Tolk and the NVDA controller client retain their respective licenses. Official binary distribution requires a final review confirming that all applicable notices, corresponding source access, and replacement/relinking obligations are satisfied.
