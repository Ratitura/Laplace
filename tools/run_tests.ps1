param(
    [ValidateSet('debug', 'release')]
    [string]$Mode = 'debug',
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
        Write-Host "[run_tests] ISPC backend enabled" -ForegroundColor Cyan
    } else {
        Write-Host "[run_tests] scalar-only backend" -ForegroundColor Cyan
    }
    xmake f @configArgs
    xmake
    xmake run laplace_tests
}
finally {
    Pop-Location
}
