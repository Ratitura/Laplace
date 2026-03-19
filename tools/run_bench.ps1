param(
    [ValidateSet('release', 'debug')]
    [string]$Mode = 'release',
    [ValidateSet('none', 'address', 'undefined')]
    [string]$Sanitizer = 'none',
    [switch]$Ispc
)

$ErrorActionPreference = 'Stop'

Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    $configArgs = @('-m', $Mode, '--toolchain=clang', "--sanitizer=$Sanitizer")
    if ($Ispc) {
        $configArgs += '--ispc=y'
        Write-Host "[run_bench] ISPC backend enabled" -ForegroundColor Cyan
    } else {
        Write-Host "[run_bench] scalar-only backend" -ForegroundColor Cyan
    }
    xmake f @configArgs
    xmake
    xmake run laplace_bench
}
finally {
    Pop-Location
}
