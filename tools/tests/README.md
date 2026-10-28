# Accessibility regression tests

From PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_accessibility.ps1
```

Use an absolute script path when outside the repository. PowerShell 5.1 and 7
are supported. Optional `-Jobs 4` controls build and test parallelism (1–64).
The wrapper initializes MSYS2 MinGW64 automatically and restores its environment
variables on exit. Dependencies are the game's documented MinGW64 GCC, CMake,
Make, Python/header-generation tools, SDL2 and zlib. Compiler or test failures
produce a nonzero exit code, with compiler/CTest output shown for diagnosis.

The command configures a separate Release build in `build/accessibility-tests`,
builds only the `accessibility_tests` target, and runs the `accessibility` CTest
label. It does not build or launch the game, change the developer build's cache,
or modify its executable, `pd.ini`, saves, logs, ROM, or assets. The fixtures use
repository source and generated headers from existing asset metadata, not a ROM
or runtime extracted assets. Generated test headers, executables and reports
remain under the ignored test build directory. Nothing is packaged or uploaded.

## Coverage

Every fixture runs with `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0` and `1`:

| Fixture | Checks |
| --- | --- |
| `cane_result` | Value-only barrier/terrain/drop/crouch/ladder precedence |
| `cane_path` | Connected paths, clearance, edges, bounded tracing and phrases |
| `landmark` | Landmark registry and eligibility rules |
| `glossary` | Catalog/menu behavior, generated PCM, ownership and deadlines |
| `monitor_image` | Native monitor setter/query parity and read-only identity |
| `interaction` | X-Ray switch classification, all 8! assignments, lifecycle and pulses |
| `precision_mapping` | Aim-direction mapping, smoothing and lock-tone semantics |
| `weapon_target` | Explosive weapon classification, lifecycle, damage compatibility and detector-copy rules |

This is **16 tests**, with assertions explicitly enabled even in Release builds.
Every executable also compiles `assertions_enabled.c`, which fails compilation
if `NDEBUG` remains defined. CTest applies a 60-second timeout per test, prints
failed-test output, and rejects an empty test selection.

Adapter fixtures retain their existing private `.c` includes, stubs, and local
GCC `-O2 -fwhole-program` requirement. Pure cane fixtures compile their actual
implementation as a separate source. Game compiler flags are unchanged.
These are semantic, synthetic and PCM regressions—not proof of in-game
visibility, actual shot damage, audibility, performance or blind-user usability.

## CMake/CTest directly (MinGW64)

```sh
cmake -G "Unix Makefiles" -S . -B build/accessibility-tests -DROMID=ntsc-final -DCMAKE_BUILD_TYPE=Release -DACCESSIBILITY_BUILD_TESTS=ON
cmake --build build/accessibility-tests --target accessibility_tests -j4 -- -O
ctest --test-dir build/accessibility-tests -L accessibility --output-on-failure --no-tests=error -j4
```

`ACCESSIBILITY_BUILD_TESTS` defaults off. When enabled, tests are still excluded
from ordinary game/package builds and must be explicitly built. This initial
suite supports only Windows x64 MinGW GCC with `ntsc-final`; configuration fails
clearly for other combinations rather than suggesting unverified portability.
Both diagnostic variants are built regardless of the game's diagnostics option.
Run CTest from MinGW64 so test executables can find matching runtime DLLs.

Results are in `build/accessibility-tests/Testing/Temporary/LastTest.log`;
CTest also records failed-test names in `LastTestsFailed.log` when applicable.
For targeted reruns in MinGW64:

```sh
ctest --test-dir build/accessibility-tests -R weapon_target --output-on-failure --no-tests=error
```

Always rerun the full suite before treating a targeted change as
regression-checked.

## Optional recorded cane replay

Live-log replay is deliberately not automatic: it requires a compatible local
capture and does not replace the fixed fixtures. After building the suite:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\replay_cane_results.ps1 -LogPath C:\path\to\accessibility.log
```

The replay script defaults to the diagnostics-off cane-result executable in
the new test build. `-TestExecutable` still allows a different compatible test
executable. This replays the original value-only decision stage, not the entire
connected-path, corridor, or audio pipeline.

## Adding a fixture

Add `accessibility_NAME_test.c` here and register `NAME` in this directory's
`CMakeLists.txt`. Add separately compiled implementation sources or libraries
there as needed, and update the coverage table. Keep tests deterministic,
bounded, asset-free at runtime, and independent of the player's active session.

## Runner verification

On 2026-10-27, all 16 tests passed under Windows PowerShell 5.1 and PowerShell
7.6.5. Running from outside the checkout also passed; hashes confirmed the
developer game executable, configuration and CMake cache stayed unchanged,
and process environment variables were restored. Deliberate temporary compiler
and test failures returned nonzero status through the wrapper. A direct
`NDEBUG` compilation was rejected by the assertion guard, and an empty CTest
selection returned an error. Temporary failure injections were removed and the
full suite passed again. No game was launched, speech/interaction test performed,
or new accessibility log session created. This verifies developer tooling, not
blind-user acceptance. Reverting the tooling commit restores the prior manual
workflow without a game/save/config migration.
