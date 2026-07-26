[CmdletBinding()]
param(
    [string]$GamePath = '',
    [string]$OutputRoot = '',
    [ValidateRange(250, 5000)]
    [int]$SampleIntervalMs = 500,
    [ValidateRange(10, 3600)]
    [int]$WaitTimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($GamePath)) {
    $GamePath = Join-Path $PSScriptRoot '..\..\build\pd.x86_64.exe'
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $PSScriptRoot '..\..\build\diagnostics'
}

$resolvedGamePath = [System.IO.Path]::GetFullPath($GamePath)
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$gameDirectory = Split-Path -Parent $resolvedGamePath
$gameName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedGamePath)
$captureStart = Get-Date
$captureName = $captureStart.ToString('yyyyMMdd-HHmmss')
$captureDirectory = Join-Path $resolvedOutputRoot $captureName

New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null

$manifest = [ordered]@{
    schema = 1
    capture_start = $captureStart.ToString('o')
    game_path = $resolvedGamePath
    sample_interval_ms = $SampleIntervalMs
    machine = $env:COMPUTERNAME
    user = $env:USERNAME
    git_hash = ''
    git_branch = ''
    power_plan = ''
    game_pid = 0
    capture_stop = ''
    notes = @()
}

try {
    $repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $manifest.git_hash = (& git -C $repoRoot rev-parse HEAD 2>$null).Trim()
    $manifest.git_branch = (& git -C $repoRoot branch --show-current 2>$null).Trim()
} catch {
    $manifest.notes += "Git identity unavailable: $($_.Exception.Message)"
}

try {
    $manifest.power_plan = (& powercfg.exe /getactivescheme 2>$null) -join ' '
} catch {
    $manifest.notes += "Power plan unavailable: $($_.Exception.Message)"
}

$deadline = (Get-Date).AddSeconds($WaitTimeoutSeconds)
$gameProcess = $null

Write-Host "Waiting for $resolvedGamePath"

while (-not $gameProcess -and (Get-Date) -lt $deadline) {
    $candidates = Get-Process -Name $gameName -ErrorAction SilentlyContinue

    foreach ($candidate in $candidates) {
        try {
            if ([System.IO.Path]::GetFullPath($candidate.Path) -eq $resolvedGamePath) {
                $gameProcess = $candidate
                break
            }
        } catch {
            # Some protected processes do not expose Path. They are not the game.
        }
    }

    if (-not $gameProcess) {
        Start-Sleep -Milliseconds 500
    }
}

if (-not $gameProcess) {
    throw "Timed out waiting for the game after $WaitTimeoutSeconds seconds."
}

$manifest.game_pid = $gameProcess.Id
$processCsv = Join-Path $captureDirectory 'process_samples.csv'
$gpuCsv = Join-Path $captureDirectory 'nvidia_samples.csv'
$gpuRaw = Join-Path $captureDirectory 'nvidia_samples.raw.csv'
$gpuError = Join-Path $captureDirectory 'nvidia_smi_error.txt'
$windowsGpuCsv = Join-Path $captureDirectory 'windows_gpu_samples.csv'
$processRows = [System.Collections.Generic.List[object]]::new()
$windowsGpuRows = [System.Collections.Generic.List[object]]::new()
$sampleIndex = 0
$nvidiaProcess = $null

function Write-CsvBuffer {
    param(
        [System.Collections.IList]$Buffer,
        [string]$Path
    )

    if (-not $Buffer.Count) {
        return
    }

    if (Test-Path -LiteralPath $Path) {
        $Buffer | Export-Csv -Path $Path -NoTypeInformation -Append
    } else {
        $Buffer | Export-Csv -Path $Path -NoTypeInformation
    }

    $Buffer.Clear()
}

Write-Host "Capturing PID $($gameProcess.Id) to $captureDirectory"

if (Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue) {
    try {
        $nvidiaArguments = @(
            '--query-gpu=timestamp,utilization.gpu,utilization.memory,clocks.gr,clocks.mem,pstate,power.draw,temperature.gpu,memory.used,memory.total',
            '--format=csv,noheader,nounits',
            "--loop-ms=$SampleIntervalMs"
        )
        $nvidiaProcess = Start-Process -FilePath (
            Get-Command nvidia-smi.exe).Source `
            -ArgumentList $nvidiaArguments `
            -RedirectStandardOutput $gpuRaw `
            -RedirectStandardError $gpuError `
            -WindowStyle Hidden -PassThru
    } catch {
        $manifest.notes += "nvidia-smi collector failed: $($_.Exception.Message)"
    }
}

try {
    while (-not $gameProcess.HasExited) {
        $sampleTime = Get-Date
        $gameProcess.Refresh()
        $dwm = Get-Process -Name dwm -ErrorAction SilentlyContinue |
            Select-Object -First 1

        $processRows.Add([pscustomobject]@{
            timestamp = $sampleTime.ToString('o')
            elapsed_seconds = [math]::Round(
                ($sampleTime - $captureStart).TotalSeconds, 3)
            pid = $gameProcess.Id
            cpu_seconds = [math]::Round($gameProcess.CPU, 6)
            working_set_bytes = $gameProcess.WorkingSet64
            private_bytes = $gameProcess.PrivateMemorySize64
            virtual_bytes = $gameProcess.VirtualMemorySize64
            paged_bytes = $gameProcess.PagedMemorySize64
            handles = $gameProcess.HandleCount
            threads = $gameProcess.Threads.Count
            dwm_cpu_seconds = if ($dwm) { [math]::Round($dwm.CPU, 6) } else { '' }
            dwm_working_set_bytes = if ($dwm) { $dwm.WorkingSet64 } else { '' }
        })

        if (($sampleIndex % 4) -eq 0) {
            try {
                $counterPaths = @(
                    '\GPU Engine(*)\Utilization Percentage',
                    '\GPU Process Memory(*)\Dedicated Usage',
                    '\GPU Process Memory(*)\Shared Usage'
                )
                $counter = Get-Counter -Counter $counterPaths -ErrorAction Stop
                foreach ($sample in $counter.CounterSamples) {
                    if ($sample.InstanceName -match "pid_$($gameProcess.Id)_") {
                        $windowsGpuRows.Add([pscustomobject]@{
                            timestamp = $sampleTime.ToString('o')
                            elapsed_seconds = [math]::Round(
                                ($sampleTime - $captureStart).TotalSeconds, 3)
                            path = $sample.Path
                            instance = $sample.InstanceName
                            value = [math]::Round($sample.CookedValue, 3)
                        })
                    }
                }
            } catch {
                if (-not ($manifest.notes -match 'Windows GPU counters unavailable')) {
                    $manifest.notes +=
                        "Windows GPU counters unavailable: $($_.Exception.Message)"
                }
            }
        }

        if ($processRows.Count -ge 20) {
            Write-CsvBuffer -Buffer $processRows -Path $processCsv
            Write-CsvBuffer -Buffer $windowsGpuRows -Path $windowsGpuCsv
        }

        $sampleIndex++
        Start-Sleep -Milliseconds $SampleIntervalMs
    }
} finally {
    $manifest.capture_stop = (Get-Date).ToString('o')

    Write-CsvBuffer -Buffer $processRows -Path $processCsv
    Write-CsvBuffer -Buffer $windowsGpuRows -Path $windowsGpuCsv

    if ($nvidiaProcess -and -not $nvidiaProcess.HasExited) {
        Stop-Process -Id $nvidiaProcess.Id -ErrorAction SilentlyContinue
        $nvidiaProcess.WaitForExit(5000)
    }

    if (Test-Path -LiteralPath $gpuRaw) {
        $gpuHeaders = @(
            'timestamp',
            'utilization_gpu_percent',
            'utilization_memory_percent',
            'graphics_clock_mhz',
            'memory_clock_mhz',
            'performance_state',
            'power_watts',
            'temperature_c',
            'memory_used_mib',
            'memory_total_mib'
        )
        Get-Content -LiteralPath $gpuRaw |
            Where-Object { $_ -match ',' } |
            ConvertFrom-Csv -Header $gpuHeaders |
            Export-Csv -LiteralPath $gpuCsv -NoTypeInformation
    }

    foreach ($name in @('pd.ini', 'accessibility.log', 'pd.crash.log')) {
        $source = Join-Path $gameDirectory $name
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination $captureDirectory -Force
        }
    }

    try {
        $events = Get-WinEvent -FilterHashtable @{
            LogName = 'System'
            StartTime = $captureStart.AddMinutes(-1)
            EndTime = (Get-Date).AddMinutes(1)
        } -ErrorAction Stop | Where-Object {
            $_.ProviderName -match
                'Display|DxgKrnl|nvlddmkm|amdkmdag|igfx|WHEA|Kernel-Power'
        } | Select-Object TimeCreated, ProviderName, Id,
            LevelDisplayName, Message
        $events | Export-Clixml -Path (
            Join-Path $captureDirectory 'graphics_system_events.clixml')
    } catch {
        $manifest.notes += "System event capture failed: $($_.Exception.Message)"
    }

    $manifest | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (
            Join-Path $captureDirectory 'capture_manifest.json') -Encoding UTF8
}

Write-Host "Capture complete: $captureDirectory"
