# Runs each backend from its own private directory. Never modifies the
# rexruntime.dll/rexgpu-xenos.dll that another process may already be using.
param([ValidateSet("d3d12", "vulkan")][string]$Backend = "d3d12")

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$pairDir = Join-Path $root "renderers\$Backend"
$manifestPath = Join-Path $pairDir "renderer-pair.json"
$baseManifest = Join-Path $root "renderers\d3d12\renderer-pair.json"
$session = $null

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing required renderer file: $Path"
    }
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($hash -ne $Expected) { throw "Renderer checksum mismatch: $Path" }
}

try {
    if (-not (Test-Path -LiteralPath (Join-Path $root "fable_2.exe"))) {
        throw "fable_2.exe is missing next to the launcher."
    }
    if (-not (Test-Path -LiteralPath $manifestPath) -or
        -not (Test-Path -LiteralPath $baseManifest)) {
        throw "Renderer pair is not staged. Rebuild Release; for Vulkan first run tools\build_sdk_vulkan.cmd."
    }
    $pair = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $base = Get-Content -LiteralPath $baseManifest -Raw | ConvertFrom-Json
    if ($pair.backend -ne $Backend -or $pair.configuration -ne "Release") {
        throw "Incorrect renderer backend/configuration in $manifestPath"
    }
    foreach ($field in @("sdk_commit", "patch_sha256", "compiler", "compiler_version")) {
        if (-not $pair.$field -or $pair.$field -ne $base.$field) {
            throw "Incompatible SDK $field for $Backend. Rebuild both SDK renderer variants."
        }
    }
    foreach ($entry in @(@("rexruntime.dll", "runtime_sha256"),
                         @("rexgpu-xenos.dll", "plugin_sha256"))) {
        Assert-Hash (Join-Path $pairDir $entry[0]) $pair.($entry[1])
    }

    # GUID directories prevent concurrent runs from swapping DLLs underneath
    # one another. Game data, saves, cache, and logs remain in the normal root.
    $sessions = Join-Path $root ".renderer-sessions"
    New-Item -ItemType Directory -Path $sessions -Force | Out-Null
    $session = Join-Path $sessions ([guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $session | Out-Null

    Copy-Item -LiteralPath (Join-Path $root "fable_2.exe") -Destination $session -ErrorAction Stop
    Copy-Item -LiteralPath (Join-Path $pairDir "rexruntime.dll") -Destination $session -ErrorAction Stop
    $pluginName = if ($Backend -eq "vulkan") { "rexgpu-xenos-vulkan.dll" } else { "rexgpu-xenos.dll" }
    Copy-Item -LiteralPath (Join-Path $pairDir "rexgpu-xenos.dll") -Destination (Join-Path $session $pluginName) -ErrorAction Stop
    Assert-Hash (Join-Path $session "rexruntime.dll") $pair.runtime_sha256
    Assert-Hash (Join-Path $session $pluginName) $pair.plugin_sha256

    if ($Backend -eq "vulkan" -and (Test-Path -LiteralPath (Join-Path $pairDir "vulkan-1.dll"))) {
        Copy-Item -LiteralPath (Join-Path $pairDir "vulkan-1.dll") -Destination $session -ErrorAction Stop
    }
    foreach ($file in @("fable2_build.json", "app-icon.png")) {
        $source = Join-Path $root $file
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination $session -ErrorAction Stop
        }
    }
    foreach ($file in @("fable_2.toml", "fable2_config.toml")) {
        $source = Join-Path $root $file
        if (Test-Path -LiteralPath $source) {
            New-Item -ItemType HardLink -Path (Join-Path $session $file) -Target $source | Out-Null
        }
    }
    foreach ($folder in @("saves", "cache", "logs")) {
        $source = Join-Path $root $folder
        New-Item -ItemType Directory -Path $source -Force | Out-Null
        New-Item -ItemType Junction -Path (Join-Path $session $folder) -Target $source | Out-Null
    }
    $gpu = if ($Backend -eq "vulkan") { "xenos-vulkan" } else { "xenos" }
    Write-Host "Fable 2: $Backend renderer, verified private runtime/plugin pair."
    $args = @("--gpu_plugin=$gpu", "--game_data_root=`"$root`"")
    $process = Start-Process -FilePath (Join-Path $session "fable_2.exe") -WorkingDirectory $session -ArgumentList $args -Wait -PassThru
    $code = $process.ExitCode

    # Logs written directly beside the exe are copied out before cleanup.
    foreach ($name in @("fable2_func_trace.log", "fable2_func_summary.log",
                         "fps_probe.log", "fable2_state_probe.log")) {
        $source = Join-Path $session $name
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $root $name) -Force
        }
    }
    exit $code
} catch {
    [Console]::Error.WriteLine("Fable 2 launcher error: " + $_.Exception.Message)
    exit 1
} finally {
    if ($session -and (Test-Path -LiteralPath $session)) {
        # Remove the *junctions*, not their shared contents. rmdir without /s
        # on a Windows junction unlinks it safely; only then delete the tree.
        foreach ($folder in @("saves", "cache", "logs")) {
            $junction = Join-Path $session $folder
            if (Test-Path -LiteralPath $junction) {
                & cmd.exe /d /c "rmdir `"$junction`"" 2>$null | Out-Null
            }
        }
        Remove-Item -LiteralPath $session -Recurse -Force -ErrorAction SilentlyContinue
    }
}
