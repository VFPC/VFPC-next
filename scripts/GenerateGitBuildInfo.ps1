param(
    [Parameter(Mandatory = $true)][string]$RepositoryDir,
    [Parameter(Mandatory = $true)][string]$OutputFile
)

$ErrorActionPreference = 'Stop'
$commit = 'unknown'
$dirty = 'unknown'

# Missing Git, a source archive, or failed Git queries must never reuse old data.
if (Get-Command git -ErrorAction SilentlyContinue) {
    $sourceRoot = (Resolve-Path -LiteralPath $RepositoryDir).Path.TrimEnd('\', '/')
    # Do not accidentally identify an enclosing repository for a source archive.
    if (Test-Path -LiteralPath (Join-Path $sourceRoot '.git')) {
        # Windows PowerShell can turn native stderr into terminating errors.
        $ErrorActionPreference = 'Continue'
        $hashOutput = @(& git -C $sourceRoot rev-parse --verify HEAD 2>$null)
        $hashResult = $LASTEXITCODE
        $statusOutput = @(& git -C $sourceRoot status --porcelain=v1 --untracked-files=all --ignore-submodules=none 2>$null)
        $statusResult = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($hashResult -eq 0 -and $hashOutput.Count -eq 1 -and
            $hashOutput[0] -match '^[0-9a-fA-F]{40,64}$') {
            $commit = $hashOutput[0].ToLowerInvariant()
        }
        if ($statusResult -eq 0) {
            $dirty = if ($statusOutput.Count -gt 0) { 'true' } else { 'false' }
        }
    }
}

if ($commit -eq 'unknown' -or $dirty -eq 'unknown') {
    Write-Warning 'Git build identity is incomplete; unavailable fields are recorded as unknown.'
}

$content = @"
// Generated at build time. Do not edit or commit.
#ifndef VFPC_GIT_BUILD_INFO_H
#define VFPC_GIT_BUILD_INFO_H
#define VFPC_GIT_COMMIT "$commit"
#define VFPC_GIT_DIRTY "$dirty"
#endif
"@ + "`r`n"

$outputPath = [System.IO.Path]::GetFullPath($OutputFile)
$directory = [System.IO.Path]::GetDirectoryName($outputPath)
[System.IO.Directory]::CreateDirectory($directory) | Out-Null
if (!(Test-Path -LiteralPath $outputPath) -or
    [System.IO.File]::ReadAllText($outputPath) -cne $content) {
    [System.IO.File]::WriteAllText($outputPath, $content, [System.Text.UTF8Encoding]::new($false))
}
