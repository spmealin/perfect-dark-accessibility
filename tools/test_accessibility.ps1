param(
    [ValidateRange(1, 64)]
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
$repositoryPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bashPath = 'C:\msys64\usr\bin\bash.exe'

if (-not (Test-Path -LiteralPath $bashPath -PathType Leaf)) {
    throw "MSYS2 bash was not found at $bashPath"
}

# Pass paths as environment data, not interpolated shell code. This also works
# from another current directory and with spaces or quotes in the checkout path.
$environmentNames = @('MSYSTEM', 'CHERE_INVOKING',
    'PD_ACCESSIBILITY_TEST_ROOT', 'PD_ACCESSIBILITY_TEST_JOBS')
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

try {
    $env:MSYSTEM = 'MINGW64'
    $env:CHERE_INVOKING = '1'
    $env:PD_ACCESSIBILITY_TEST_ROOT = $repositoryPath
    $env:PD_ACCESSIBILITY_TEST_JOBS = [string]$Jobs

    # A separate cache leaves the developer game's executable, settings,
    # packaging choice, diagnostics flags, saves and session logs untouched.
    $command = @'
set -e
cd "$(cygpath -u "$PD_ACCESSIBILITY_TEST_ROOT")"
cmake -G "Unix Makefiles" -S . -B build/accessibility-tests -DROMID=ntsc-final -DCMAKE_BUILD_TYPE=Release -DACCESSIBILITY_BUILD_TESTS=ON
cmake --build build/accessibility-tests --target accessibility_tests -j "$PD_ACCESSIBILITY_TEST_JOBS" -- -O
ctest --test-dir build/accessibility-tests -L accessibility --output-on-failure --no-tests=error -j "$PD_ACCESSIBILITY_TEST_JOBS"
'@
    # Explicit Windows argument quoting avoids PowerShell 5.1 dropping bash's
    # embedded quotes; it also behaves consistently under PowerShell 7.
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $bashPath
    $startInfo.Arguments = '-lc "' + $command.Replace("`r", '').Replace('"', '\"') + '"'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    try {
        # Drain both pipes concurrently so verbose compiler output cannot
        # deadlock. Emit the transcript on success as well as on failure.
        $outputRead = $process.StandardOutput.ReadToEndAsync()
        $errorRead = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        Write-Output ($outputRead.GetAwaiter().GetResult())
        [Console]::Error.Write($errorRead.GetAwaiter().GetResult())
        if ($process.ExitCode -ne 0) {
            throw "Accessibility regression suite failed (exit $($process.ExitCode))."
        }
    } finally {
        $process.Dispose()
    }
} finally {
    foreach ($name in $environmentNames) {
        if ($null -eq $previousEnvironment[$name]) {
            Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        } else {
            [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
        }
    }
}

Write-Output 'Accessibility regression suite passed (diagnostics off and on).'
Write-Output "CTest results: $(Join-Path $repositoryPath 'build\accessibility-tests\Testing\Temporary\LastTest.log')"
