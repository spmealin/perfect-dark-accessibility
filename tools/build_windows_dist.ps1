param(
	[ValidateRange(1, 64)]
	[int]$Jobs = 4,
	[switch]$EnableAccessibility
)

$ErrorActionPreference = 'Stop'

$repositoryPath = [System.IO.Path]::GetFullPath(
	(Join-Path $PSScriptRoot '..'))
$bashPath = 'C:\msys64\usr\bin\bash.exe'

if (-not (Test-Path -LiteralPath $bashPath -PathType Leaf)) {
	throw "MSYS2 bash was not found at $bashPath"
}

$env:MSYSTEM = 'MINGW64'
$env:CHERE_INVOKING = '1'
$accessibilityValue = if ($EnableAccessibility) { 'ON' } else { 'OFF' }
$driveName = $repositoryPath.Substring(0, 1).ToLowerInvariant()
$repositoryMsysPath = '/' + $driveName +
	$repositoryPath.Substring(2).Replace('\', '/')

if ($repositoryMsysPath.Contains("'")) {
	throw "The repository path cannot contain a single quote: $repositoryPath"
}

$command = "cd '$repositoryMsysPath' && " +
	"cmake -GUnix\ Makefiles -Bbuild " +
	"-DPD_PACKAGE_ACCESSIBILITY=$accessibilityValue . && " +
	"cmake --build build --target pd_zip -j$Jobs -- -O"

Push-Location -LiteralPath $repositoryPath

try {
	& $bashPath -lc $command

	if ($LASTEXITCODE -ne 0) {
		throw "MinGW64 package build failed with exit code $LASTEXITCODE"
	}
} finally {
	Pop-Location
}

$archivePath = Join-Path $repositoryPath 'build\dist\pd.zip'

if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
	throw "Package build completed without creating $archivePath"
}

Write-Output "Created $archivePath"
