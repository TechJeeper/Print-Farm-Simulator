<#
.SYNOPSIS
  Builds Print Farm Simulator on Windows using the Pebble SDK in a container.

.EXAMPLE
  ./build.ps1
  ./build.ps1 -Phone 192.168.1.23
  ./build.ps1 -Zip
  ./build.ps1 -Shell
#>
param(
    [string]$Phone,
    [switch]$Zip,
    [switch]$Shell
)
$engine = if (Get-Command podman -ErrorAction SilentlyContinue) { 'podman' }
          elseif (Get-Command docker -ErrorAction SilentlyContinue) { 'docker' }
          else { throw 'Install Podman Desktop or Docker Desktop first.' }

$root = $PSScriptRoot
$image = 'pebble-sdk'
$volume = 'pebble-sdk'

& $engine image inspect $image *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Building the $image image (first run only)..."
    & $engine build -t $image (Join-Path $root 'build-env')
    if ($LASTEXITCODE -ne 0) { throw 'Image build failed.' }
}

$run = @('run', '--rm', '-v', "${root}:/PrintFarmSimulator", '-w', '/PrintFarmSimulator', '-v', "${volume}:/root/.pebble-sdk")
$setup = 'pebble sdk list 2>/dev/null | grep -q active || pebble sdk install latest'

if ($Shell) {
    & $engine @run -it $image bash -c "$setup; exec bash"
    exit $LASTEXITCODE
}

$cmd = "$setup && pebble build"
if ($Zip) { $cmd += " && python3 build-env/make_zip.py" }
if ($Phone) { $cmd += " && pebble install --phone $Phone" }
& $engine @run $image bash -c $cmd
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
Write-Host "Built $(Join-Path $root 'build\PrintFarmSimulator.pbw')"
