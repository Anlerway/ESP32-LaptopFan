#requires -Version 5
<#
  组装可发布的烧录器目录（主程序不含固件/驱动，全部放在 exe 旁边的文件夹里）：

    outFanControl-Flasher-<版本>        FanControl-Flasher.exe        主程序
        toolsesptool.exe             实际执行烧录的官方工具
        firmware                     三个镜像（bootloader / partitions / app）
        drivers<芯片>               INF + SYS + CAT，按需安装
        使用说明.txt

  用法：powershell -ExecutionPolicy Bypass -File toolsmake-package.ps1 [-Version 1.0.2]
#>
param(
    [string]$Version = '1.0.2'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$exe = Join-Path $root 'build\FanControl-Flasher.exe'
if (-not (Test-Path $exe)) { throw "先运行 build.bat 生成 build\FanControl-Flasher.exe" }

$out = Join-Path $root ("out\FanControl-Flasher-" + $Version)
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force -Path $out, (Join-Path $out 'tools'), (Join-Path $out 'firmware'), (Join-Path $out 'drivers') | Out-Null

Copy-Item $exe (Join-Path $out 'FanControl-Flasher.exe')

# esptool：优先找仓库里的 tools\esptool.exe，其次找 Arduino15 里装的那份。
$esptool = Join-Path $root 'tools\esptool.exe'
if (-not (Test-Path $esptool)) {
    $local = $env:LOCALAPPDATA
    if ([string]::IsNullOrEmpty($local)) { $local = Join-Path $env:USERPROFILE 'AppData\Local' }
    $esptoolRoot = Join-Path $local 'Arduino15\packages\esp32\tools\esptool_py'
    $found = $null
    if (Test-Path $esptoolRoot) {
        $found = Get-ChildItem -Path $esptoolRoot -Filter esptool.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if ($null -eq $found) { throw '找不到 esptool.exe，请把它放到 flasher\tools\esptool.exe' }
    $esptool = $found.FullName
}
Copy-Item $esptool (Join-Path $out 'tools\esptool.exe')

foreach ($name in 'bootloader', 'partitions', 'app') {
    $bin = Join-Path $root ("assets\" + $name + '.bin')
    if (-not (Test-Path $bin)) { throw ("缺少固件 assets\" + $name + '.bin，先运行 tools\build-firmware.bat') }
    Copy-Item $bin (Join-Path $out ("firmware\" + $name + '.bin'))
}

$driversRoot = Join-Path $root 'drivers'
if (Test-Path $driversRoot) {
    Get-ChildItem $driversRoot -Directory | Where-Object { $_.Name -ne '_raw' } | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $out 'drivers') -Recurse -Force
    }
}

$readme = Join-Path $root 'tools\使用说明.txt'
if (Test-Path $readme) { Copy-Item $readme (Join-Path $out '使用说明.txt') }

Write-Host ("已生成 " + $out)
