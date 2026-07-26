[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$LogPath,
    [string]$CollectorDirectory,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$resolvedLogPath = [System.IO.Path]::GetFullPath($LogPath)

if (-not $OutputPath) {
    $OutputPath = Join-Path (Split-Path -Parent $resolvedLogPath) `
        'graphics_diagnostics_report.md'
}

$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)

function Convert-MessageFields {
    param([string]$Message)
    $fields = @{}
    foreach ($match in [regex]::Matches(
        $Message, '(?<key>[A-Za-z0-9_]+)=(?<value>[^ ]*)')) {
        $fields[$match.Groups['key'].Value] =
            $match.Groups['value'].Value
    }
    return $fields
}

$sessions = @{}
$performance = [System.Collections.Generic.List[object]]::new()
$graphicsWindows = [System.Collections.Generic.List[object]]::new()
$episodes = [System.Collections.Generic.List[object]]::new()
$worstFrames = [System.Collections.Generic.List[object]]::new()
$metadata = $null

Get-Content -LiteralPath $resolvedLogPath | ForEach-Object {
    try {
        $row = $_ | ConvertFrom-Json -ErrorAction Stop
    } catch {
        return
    }

    $sessions[$row.session] = $true
    $fields = Convert-MessageFields $row.message

    if ($row.category -eq 'performance' -and
            $row.event -eq 'frame_window') {
        $performance.Add([pscustomobject]@{
            session = $row.session
            t_us = [int64]$row.t_us
            fps = [double]$fields.render_fps
            max_gap_us = [int64]$fields.max_frame_gap_us
            stage = $fields.stage
            menu_count = $fields.menu_count
            working_set_bytes = [int64]$fields.working_set_bytes
            private_bytes = [int64]$fields.private_bytes
            mixer_active_delta = $fields.mixer_active_delta
            combat_slots = $fields.combat_enabled_slots
            tracker_slots = $fields.tracker_enabled_slots
        })
    } elseif ($row.category -eq 'graphics' -and
            $row.event -eq 'frame_window') {
        $graphicsWindows.Add([pscustomobject]@{
            session = $row.session
            t_us = [int64]$row.t_us
            fps = [double]$fields.render_fps
            interval_max_us = [int64]$fields.interval_max_us
            video_start_max_us = [int64]$fields.video_start_max_us
            video_submit_max_us = [int64]$fields.video_submit_max_us
            display_list_max_us = [int64]$fields.display_list_max_us
            framebuffer_setup_max_us =
                [int64]$fields.framebuffer_setup_max_us
            composite_max_us = [int64]$fields.composite_max_us
            frame_limit_max_us = [int64]$fields.frame_limit_max_us
            swap_max_us = [int64]$fields.swap_max_us
            video_end_max_us = [int64]$fields.video_end_max_us
        })
    } elseif ($row.category -eq 'graphics' -and
            $row.event -eq 'stall_episode') {
        $episodes.Add([pscustomobject]@{
            session = $row.session
            episode = $fields.episode
            reason = $fields.reason
            records = $fields.records
            pretrigger_frames = $fields.pretrigger_frames
            duration_us = $fields.duration_us
            maximum_interval_us = $fields.maximum_interval_us
            truncated = $fields.truncated
        })
    } elseif ($row.category -eq 'graphics' -and
            $row.event -eq 'stall_worst_frame') {
        $worstFrames.Add([pscustomobject]@{
            session = $row.session
            episode = $fields.episode
            relative_to_trigger = $fields.relative_to_trigger
            interval_us = $fields.interval_us
            dominant = $fields.dominant
            dominant_us = $fields.dominant_us
            stage = $fields.stage
            menu_count = $fields.menu_count
        })
    } elseif ($row.category -eq 'graphics' -and
            $row.event -eq 'metadata') {
        $metadata = $fields
    }
}

$report = [System.Collections.Generic.List[string]]::new()
$report.Add('# Graphics diagnostics report')
$report.Add('')
$report.Add("- Log: ``$resolvedLogPath``")
$report.Add("- Sessions: $($sessions.Keys -join ', ')")
$report.Add("- Generated: $((Get-Date).ToString('o'))")
$report.Add('')

if ($metadata) {
    $report.Add('## Graphics context')
    $report.Add('')
    $report.Add("- API: $($metadata.api)")
    $report.Add("- SDL driver: $($metadata.video_driver)")
    $report.Add("- GPU renderer: $($metadata.renderer)")
    $report.Add("- OpenGL: $($metadata.version)")
    $report.Add("- Refresh/swap: $($metadata.refresh_rate) Hz, interval $($metadata.swap_interval)")
    $report.Add("- Window: $($metadata.drawable_width)x$($metadata.drawable_height), fullscreen $($metadata.fullscreen), exclusive $($metadata.exclusive_fullscreen)")
    $report.Add("- Framebuffer effects/MSAA: $($metadata.framebuffer_effects)/$($metadata.msaa)")
    $report.Add('')
}

if ($performance.Count) {
    $lowest = $performance | Sort-Object fps | Select-Object -First 10
    $largestGap = $performance |
        Sort-Object max_gap_us -Descending | Select-Object -First 1
    $firstMemory = $performance | Select-Object -First 1
    $lastMemory = $performance | Select-Object -Last 1

    $report.Add('## Aggregate performance')
    $report.Add('')
    $report.Add("- Windows: $($performance.Count)")
    $report.Add("- Lowest FPS: $([math]::Round(($lowest | Select-Object -First 1).fps, 3))")
    $report.Add("- Largest frame gap: $($largestGap.max_gap_us) us")
    $report.Add("- Working-set change: $($lastMemory.working_set_bytes - $firstMemory.working_set_bytes) bytes")
    $report.Add("- Private-byte change: $($lastMemory.private_bytes - $firstMemory.private_bytes) bytes")
    $report.Add('')
    $report.Add('| t_us | FPS | max gap us | stage | menu | mixer active |')
    $report.Add('| ---: | ---: | ---: | ---: | ---: | ---: |')
    foreach ($row in $lowest) {
        $report.Add("| $($row.t_us) | $($row.fps) | $($row.max_gap_us) | $($row.stage) | $($row.menu_count) | $($row.mixer_active_delta) |")
    }
    $report.Add('')
}

if ($graphicsWindows.Count) {
    $slowGraphics = $graphicsWindows |
        Sort-Object fps | Select-Object -First 10
    $report.Add('## Frame-phase windows')
    $report.Add('')
    $report.Add('| FPS | interval | start | submit | display list | framebuffer | composite | limiter | swap | end |')
    $report.Add('| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |')
    foreach ($row in $slowGraphics) {
        $report.Add("| $($row.fps) | $($row.interval_max_us) | $($row.video_start_max_us) | $($row.video_submit_max_us) | $($row.display_list_max_us) | $($row.framebuffer_setup_max_us) | $($row.composite_max_us) | $($row.frame_limit_max_us) | $($row.swap_max_us) | $($row.video_end_max_us) |")
    }
    $report.Add('')
}

$report.Add('## Captured stall episodes')
$report.Add('')
if (-not $episodes.Count) {
    $report.Add('No `graphics/stall_episode` records were present.')
} else {
    $report.Add('| Episode | reason | records | pre-trigger | duration us | max interval us | truncated |')
    $report.Add('| ---: | --- | ---: | ---: | ---: | ---: | ---: |')
    foreach ($episode in $episodes) {
        $report.Add("| $($episode.episode) | $($episode.reason) | $($episode.records) | $($episode.pretrigger_frames) | $($episode.duration_us) | $($episode.maximum_interval_us) | $($episode.truncated) |")
    }
}
$report.Add('')

if ($worstFrames.Count) {
    $report.Add('### Worst retained frames')
    $report.Add('')
    $report.Add('| Episode | relative frame | interval us | dominant phase | phase us | stage | menu |')
    $report.Add('| ---: | ---: | ---: | --- | ---: | ---: | ---: |')
    foreach ($row in $worstFrames) {
        $report.Add("| $($row.episode) | $($row.relative_to_trigger) | $($row.interval_us) | $($row.dominant) | $($row.dominant_us) | $($row.stage) | $($row.menu_count) |")
    }
    $report.Add('')
}

if ($CollectorDirectory) {
    $resolvedCollector = [System.IO.Path]::GetFullPath($CollectorDirectory)
    $processPath = Join-Path $resolvedCollector 'process_samples.csv'
    $gpuPath = Join-Path $resolvedCollector 'nvidia_samples.csv'
    $report.Add('## External collector')
    $report.Add('')
    $report.Add("- Directory: ``$resolvedCollector``")

    if (Test-Path -LiteralPath $processPath) {
        $process = Import-Csv -LiteralPath $processPath
        $report.Add("- Process samples: $($process.Count)")
        if ($process.Count -ge 2) {
            $report.Add("- Collector working-set change: $([int64]$process[-1].working_set_bytes - [int64]$process[0].working_set_bytes) bytes")
            $report.Add("- Collector private-byte change: $([int64]$process[-1].private_bytes - [int64]$process[0].private_bytes) bytes")
        }
    }

    if (Test-Path -LiteralPath $gpuPath) {
        $gpu = Import-Csv -LiteralPath $gpuPath
        $report.Add("- NVIDIA samples: $($gpu.Count)")
        if ($gpu.Count) {
            $minimumClock = $gpu |
                Measure-Object graphics_clock_mhz -Minimum
            $maximumUtil = $gpu |
                Measure-Object utilization_gpu_percent -Maximum
            $report.Add("- GPU clock minimum: $($minimumClock.Minimum) MHz")
            $report.Add("- GPU utilization maximum: $($maximumUtil.Maximum)%")
        }
    }
    $report.Add('')
}

$report | Set-Content -LiteralPath $resolvedOutputPath -Encoding UTF8
Write-Host "Report written to $resolvedOutputPath"
