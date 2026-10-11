param([string]$Python = "python")
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$packagingRoot = Join-Path $repoRoot 'build/project-creator/packaging'
& $Python -m venv $packagingRoot
if ($LASTEXITCODE -ne 0) { throw 'Cannot create packaging environment' }
$packagingPython = Join-Path $packagingRoot 'Scripts/python.exe'
& $packagingPython -m pip install -r (Join-Path $PSScriptRoot 'requirements-build.txt')
if ($LASTEXITCODE -ne 0) { throw 'Cannot install packaging dependencies' }
& $packagingPython -m PyInstaller --noconfirm --clean --onefile --windowed --name MeowodyProjectCreator --distpath (Join-Path $repoRoot 'build/project-creator/dist') --workpath (Join-Path $repoRoot 'build/project-creator/work') --specpath (Join-Path $repoRoot 'build/project-creator') (Join-Path $PSScriptRoot 'app.py')
if ($LASTEXITCODE -ne 0) { throw 'Cannot build project creator' }
