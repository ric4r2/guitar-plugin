# Joe Garcia Rig: one-command build for Windows.
#
#   build.cmd                  install what's missing, then build the standalone app and the VST3
#   build.cmd -Install         also copy the VST3 to Program Files\Common Files\VST3 (asks for admin) and put a
#                              shortcut to the app on the desktop
#   build.cmd -Tests           also build and run the DSP tests
#   build.cmd -Clean           start from an empty build folder
#
# It installs only what's missing, with winget: Git, CMake and the Visual Studio 2022 C++ build tools (the last one
# shows a Windows admin prompt and takes a while the first time). JUCE, the NAM engine and Eigen are downloaded by
# CMake. The recorded IRs and NAM captures are already in the repository, packed and ready (Assets/private-ir/*.bin):
# nothing has to be unzipped.

param(
    [switch] $Install,
    [switch] $Tests,
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root 'build'

function Step ($text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Ok ($text) { Write-Host "   $text" -ForegroundColor Green }
function Fail ($text) { Write-Host "`n$text" -ForegroundColor Red; exit 1 }

function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}

function Have ($command) { return [bool] (Get-Command $command -ErrorAction SilentlyContinue) }

function Winget-Install ($id, $extra) {
    if (-not (Have 'winget')) {
        Fail "winget isn't available, so $id can't be installed automatically. Install 'App Installer' from the Microsoft Store (it provides winget), or install $id yourself, then run this again."
    }
    $wingetArgs = @('install', '--id', $id, '-e', '--accept-source-agreements', '--accept-package-agreements', '--silent')
    if ($extra) { $wingetArgs += $extra }
    & winget @wingetArgs
    # winget returns non-zero when the package is already installed; check the result by looking for the tool instead
    Refresh-Path
}

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($path) { return $path.Trim() } else { return $null }
}

Refresh-Path

# ---------------------------------------------------------------- tools
Step 'Checking tools'

if (-not (Have 'git')) {
    Write-Host '   Git is missing: installing it'
    Winget-Install 'Git.Git'
    if (-not (Have 'git')) { Fail 'Git still isn''t available. Open a new terminal and run this again.' }
}
Ok ('Git: ' + (git --version))

$vs = Find-VisualStudio
if (-not $vs) {
    Write-Host '   The Visual Studio 2022 C++ build tools are missing: installing them (a Windows admin prompt will appear; this can take 10-20 minutes)'
    Winget-Install 'Microsoft.VisualStudio.2022.BuildTools' @('--override', '--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
    $vs = Find-VisualStudio
    if (-not $vs) { Fail 'The C++ build tools still aren''t found. Restart the computer if the installer asked for it, then run this again.' }
}
Ok "C++ build tools: $vs"

$cmake = $null
if (Have 'cmake') {
    $cmake = (Get-Command cmake).Source
} else {
    $bundled = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'   # comes with the build tools
    if (Test-Path $bundled) {
        $cmake = $bundled
    } else {
        Write-Host '   CMake is missing: installing it'
        Winget-Install 'Kitware.CMake'
        if (Have 'cmake') { $cmake = (Get-Command cmake).Source }
    }
}
if (-not $cmake) { Fail 'CMake still isn''t available. Open a new terminal and run this again.' }
Ok ('CMake: ' + ((& $cmake --version) | Select-Object -First 1))

# ---------------------------------------------------------------- sounds
Step 'Checking the sounds'
$packs = 'irs.bin', 'nam.bin' | ForEach-Object { Join-Path $root "Assets\private-ir\$_" }
$missing = $packs | Where-Object { -not (Test-Path $_) }
if ($missing) {
    # remake the packs from the original downloads (needs Python); otherwise the plugin builds with its built-in sounds
    $python = if (Have 'py') { 'py' } elseif (Have 'python') { 'python' } else { $null }
    if ($python -and (Test-Path (Join-Path $root 'Assets\private-ir\source'))) {
        Write-Host '   Packing the IRs and captures from Assets\private-ir\source'
        & $python -I (Join-Path $root 'tools\pack_irs.py')
        & $python -I (Join-Path $root 'tools\pack_nam.py')
    } else {
        Write-Host '   The packed sounds are missing and Python isn''t installed to remake them: building with the built-in cabinet models and circuit pedals instead' -ForegroundColor Yellow
    }
}
foreach ($p in $packs) { if (Test-Path $p) { Ok ("{0} ({1:N1} MB)" -f (Split-Path $p -Leaf), ((Get-Item $p).Length / 1MB)) } }

# ---------------------------------------------------------------- build
if ($Clean -and (Test-Path $buildDir)) {
    Step 'Removing the old build folder'
    Remove-Item -Recurse -Force $buildDir
}

Step 'Configuring (the first time, this downloads JUCE, the NAM engine and Eigen)'
# JUCE has files deep enough to pass Windows' old 260-character path limit when the repository sits in a deep folder:
# let git handle long paths (for this build only, nothing is changed globally) and keep the downloads in a short folder,
# which also survives -Clean, so JUCE isn't downloaded again.
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'core.longpaths'
$env:GIT_CONFIG_VALUE_0 = 'true'
$deps = Join-Path $env:LOCALAPPDATA 'JoeGarciaRig\deps'
$configure = @('-S', $root, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DFETCHCONTENT_BASE_DIR=$deps")
if ($Tests) { $configure += '-DGUITARDRIVE_BUILD_TESTS=ON' }
& $cmake @configure
if ($LASTEXITCODE -ne 0) { Fail 'Configuring failed (see above). If a download failed, check the internet connection and run this again.' }

Step 'Building (the first build takes several minutes)'
$targets = @('GuitarDrive_Standalone', 'GuitarDrive_VST3')
if ($Tests) { $targets += 'GuitarDriveTests' }
& $cmake --build $buildDir --config Release --parallel --target @targets
if ($LASTEXITCODE -ne 0) { Fail 'The build failed (see above).' }

$release = Join-Path $buildDir 'GuitarDrive_artefacts\Release'
$app = Join-Path $release 'Standalone\Joe Garcia Rig.exe'
$vst3 = Join-Path $release 'VST3\Joe Garcia Rig.vst3'

if ($Tests) {
    Step 'Running the tests'
    Push-Location $root
    & (Join-Path $buildDir 'GuitarDriveTests_artefacts\Release\GuitarDriveTests.exe')
    $testResult = $LASTEXITCODE
    Pop-Location
    if ($testResult -ne 0) { Fail 'Some tests failed (see above).' }
}

if ($Install) {
    Step 'Installing'
    $vst3Folder = Join-Path $env:CommonProgramFiles 'VST3'
    $copy = "New-Item -ItemType Directory -Force '$vst3Folder' | Out-Null; Copy-Item -Recurse -Force '$vst3' '$vst3Folder'"
    Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile', '-Command', $copy
    Ok "VST3 copied to $vst3Folder (rescan plugins in Ableton)"
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Joe Garcia Rig.lnk'))
    $link.TargetPath = $app
    $link.WorkingDirectory = Split-Path $app
    $link.Save()
    Ok 'Desktop shortcut created'
}

Step 'Done'
Ok "App:  $app"
Ok "VST3: $vst3"
