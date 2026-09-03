param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace('-', '')
    }
    finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

$resolvedRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
$files = @(
    Get-Item -LiteralPath (Join-Path $resolvedRoot 'Demo\AlgorithmDemo.cpp')
    Get-Item -LiteralPath (Join-Path $resolvedRoot 'Demo\RunAlgorithmDemo.cmd')
    Get-Item -LiteralPath (Join-Path $resolvedRoot 'Demo\GetAlgorithmDemoFingerprint.ps1')
)
$files += Get-ChildItem -LiteralPath `
    (Join-Path $resolvedRoot 'ElevatorSimulation\Core'), `
    (Join-Path $resolvedRoot 'ElevatorSimulation\Statistics') `
    -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h' }

$entries = foreach ($file in ($files | Sort-Object FullName)) {
    $relativePath = $file.FullName.Substring($resolvedRoot.Length).ToLowerInvariant()
    $fileHash = Get-Sha256 $file.FullName
    "$relativePath|$fileHash"
}

$content = [Text.Encoding]::UTF8.GetBytes(($entries -join [Environment]::NewLine))
$aggregateHash = [BitConverter]::ToString(
    ([Security.Cryptography.SHA256]::Create()).ComputeHash($content)
).Replace('-', '')

[IO.File]::WriteAllText(
    $OutputPath,
    $aggregateHash,
    [Text.UTF8Encoding]::new($false)
)
