Set-Location $PSScriptRoot

cmake -S . -B build -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } # Exit if encounters error

cmake --build build --config Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }