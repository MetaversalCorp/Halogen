$erroractionpreference = "stop"

# Find Visual Studio installation
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if(-not (Test-Path $vsWhere)) {
    $vsWhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
}

if(Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
} else {
    # Fallback to common location
    $vcvarsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
}

if(-not (Test-Path $vcvarsPath)) {
    Write-Error "vcvars64.bat not found at: $vcvarsPath"
    exit 1
}

Write-Host "Loading MSVC environment from: $vcvarsPath"

cmd.exe /c "`"$vcvarsPath`" & set" |
foreach {
    if ($_ -match "^([^=]+)=(.*)$") {
        $name = $Matches[1]
        $value = $Matches[2]
        [Environment]::SetEnvironmentVariable($name, $value)
    }
}
