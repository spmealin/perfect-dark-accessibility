param(
    [string]$LogPath = (Join-Path $PSScriptRoot '../../build/accessibility.log'),
    [string]$TestExecutable = (Join-Path $PSScriptRoot '../../build/cane_result_test.exe')
)

$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$rows = [Collections.Generic.List[string]]::new()
$cueNames = @{ hit = 1; terrain = 2; drop = 3; crouch = 4; ladder = 5; miss = 0; error = 0 }
foreach ($line in [IO.File]::ReadLines([IO.Path]::GetFullPath($LogPath))) {
    if (-not $line.Contains('"category":"cane","event":"sweep"')) { continue }
    $record = $line | ConvertFrom-Json
    foreach ($sample in [regex]::Matches($record.message, 's\d+=\{(.*?) probes=\[')) {
        $fields = @{}
        foreach ($field in [regex]::Matches($sample.Groups[1].Value, '(\w+):([^ ]+)')) {
            $fields[$field.Groups[1].Value] = $field.Groups[2].Value
        }
        if (-not $cueNames.ContainsKey($fields.state)) { continue }
        $barrier = switch ([int]$fields.result) { 0 { 2 } 1 { 1 } default { 0 } }
        $barrierDistance = -1.0
        if ($barrier -eq 2) {
            $origin = @($fields.origin.Split(',') | ForEach-Object { [double]::Parse($_, $culture) })
            $raw = @($fields.raw.Split(',') | ForEach-Object { [double]::Parse($_, $culture) })
            $barrierDistance = [math]::Sqrt([math]::Pow($raw[0] - $origin[0], 2) + [math]::Pow($raw[2] - $origin[2], 2))
        }
        $terrain = [int]$fields.terrain
        $terrainFound = [int]($terrain -ne 0)
        $terrainDistance = [double]::Parse($fields.terrain_distance, $culture)
        $radius = [double]::Parse($fields.bbox.Split(',')[0], $culture)
        $lastClearance = switch ([int]$fields.clearance_result) { 0 { 2 } 1 { 1 } default { 0 } }
        $source = 0
        if ($terrainFound -and -not [int]$fields.terrain_suppressed -and
            ($barrierDistance -lt 0 -or $terrainDistance -lt $barrierDistance) -and
            -not [int]$fields.crouch_terrain_merge) { $source = 2 }
        elseif ($barrier -eq 2) { $source = 1 }
        $values = @(
            $barrier, $barrierDistance.ToString('R', $culture), $terrainFound,
            $terrain, $fields.terrain_distance, $fields.terrain_height, $fields.drop,
            $fields.traversal_tested, $fields.clearance_queries, $lastClearance,
            $fields.clearance_distance, $fields.grade_continuous, $fields.plateau,
            $radius.ToString('R', $culture), ($radius * 4).ToString('R', $culture),
            $radius.ToString('R', $culture), $fields.crouch, $fields.ladder,
            $cueNames[$fields.state], $source, $fields.terrain_suppressed, $fields.blocked_rise
        )
        $rows.Add($values -join ' ')
    }
}
$rows | & $TestExecutable --replay
if ($LASTEXITCODE -ne 0) { throw "Cane replay failed (exit $LASTEXITCODE)." }
