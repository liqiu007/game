# 在已安装 Visual Studio C++ 与 EasyX 的 Windows 上构建、验证并打包。
param([string]$EasyXRoot = '')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw '请安装 Visual Studio 的使用 C++ 的桌面开发工作负载。' }
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw '找不到 MSBuild，请安装 Visual Studio C++ 工作负载。' }
$argsList = @('game.sln', '/m', '/p:Configuration=Release', '/p:Platform=x64')
if ($EasyXRoot) { $argsList += "/p:EasyXRoot=$EasyXRoot" }
& $msbuild @argsList
if ($LASTEXITCODE -ne 0) { throw '编译失败，请确认 EasyX 已安装到所选 Visual Studio，或传入 -EasyXRoot。' }
& '.\bin\x64\Release\game.exe' --self-test
if ($LASTEXITCODE -ne 0) { throw '规则测试失败，停止打包。' }
New-Item -ItemType Directory -Force 'dist' | Out-Null
Copy-Item 'bin\x64\Release\game.exe' 'dist\game.exe' -Force
Compress-Archive -Path 'dist\game.exe','README.md','THIRD_PARTY_NOTICES.md' -DestinationPath 'dist\game-windows-x64.zip' -Force
Get-FileHash 'dist\game.exe' -Algorithm SHA256
Write-Host '完成：dist\game.exe；分发包：dist\game-windows-x64.zip'
