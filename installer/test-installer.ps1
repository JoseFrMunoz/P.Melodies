# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 JoseFrMunoz
# This file is part of P.Melodies. See LICENSE for the full license text.
#
# End-to-end test of the Windows installer, run by CI on a clean runner (needs admin rights):
#   1. Silent install: the VST3 bundle lands in the VST3 folder with exactly the built files,
#      the uninstaller and licences go to Program Files\P.Melodies, and the uninstall entry
#      is registered.
#   2. Installing again over an existing installation (the upgrade path) also works.
#   3. Silent uninstall removes the bundle, the program folder and the uninstall entry.

param(
    [Parameter(Mandatory = $true)][string] $Installer,
    [Parameter(Mandatory = $true)][string] $BuildBundle
)

$ErrorActionPreference = 'Stop'

$appId = '{F203E279-8139-4823-879B-18EBF7782815}'
$target = Join-Path $env:CommonProgramFiles 'VST3\P.Melodies.vst3'
$appDir = Join-Path $env:ProgramFiles 'P.Melodies'
$uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${appId}_is1"

function Invoke-Installer([string] $logName) {
    $process = Start-Process -FilePath $Installer -Wait -PassThru `
        -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=$logName"
    if ($process.ExitCode -ne 0) {
        throw "Installer exited with code $($process.ExitCode) (see $logName)"
    }
}

function Assert-Installed {
    $buildRoot = (Resolve-Path $BuildBundle).Path
    $expected = Get-ChildItem -Path $buildRoot -Recurse -File
    if ($expected.Count -eq 0) { throw "The build bundle $buildRoot is empty" }

    foreach ($file in $expected) {
        $relative = $file.FullName.Substring($buildRoot.Length).TrimStart('\')
        $installed = Join-Path $target $relative
        if (-not (Test-Path $installed)) { throw "Missing after install: $installed" }
        if ((Get-FileHash $installed).Hash -ne (Get-FileHash $file.FullName).Hash) {
            throw "Installed file differs from the build: $installed"
        }
    }

    $installedCount = (Get-ChildItem -Path $target -Recurse -File).Count
    if ($installedCount -ne $expected.Count) {
        throw "The bundle has $installedCount files, the build has $($expected.Count)"
    }

    foreach ($path in @((Join-Path $appDir 'unins000.exe'), (Join-Path $appDir 'LICENSE.txt'), (Join-Path $appDir 'licenses'))) {
        if (-not (Test-Path $path)) { throw "Missing after install: $path" }
    }

    if (-not (Test-Path $uninstallKey)) { throw "Uninstall entry not registered: $uninstallKey" }
    $entry = Get-ItemProperty $uninstallKey
    Write-Host "Installed: $($entry.DisplayName) $($entry.DisplayVersion) by $($entry.Publisher)"
    if ($entry.Publisher -ne 'JoseFrMunoz') { throw "Unexpected publisher: $($entry.Publisher)" }
}

Write-Host '1. Fresh install'
if (Test-Path $target) { throw "$target already exists before installing" }
Invoke-Installer 'installer-install.log'
Assert-Installed

Write-Host '2. Install again over the existing installation (upgrade path)'
Invoke-Installer 'installer-reinstall.log'
Assert-Installed

Write-Host '3. Uninstall'
# The uninstaller relaunches itself from a temporary copy, so wait for its effects.
Start-Process -FilePath (Join-Path $appDir 'unins000.exe') -Wait `
    -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'

$deadline = (Get-Date).AddSeconds(120)
while (((Test-Path $target) -or (Test-Path $uninstallKey) -or (Test-Path $appDir)) -and (Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 1
}

if (Test-Path $target) { throw "Bundle still present after uninstall: $target" }
if (Test-Path $uninstallKey) { throw 'Uninstall entry still registered after uninstall' }
if (Test-Path $appDir) { throw "Program folder still present after uninstall: $appDir" }

Write-Host 'Installer test passed.'
