# idf-env.ps1 — 在当前 pwsh 里激活 ESP-IDF（每次新开终端都要先执行它）
#
# 用法（从仓库根目录）：
#     . .\scripts\idf-env.ps1
#
# 默认按仓库结构查找 IDF：
#     <repo>\esp-idf-v6.1.1          （或任意 IDF 路径，用 $env:IDF_PATH 覆盖）
#     <repo>\.tools\espressif        （工具链目录，用 $env:IDF_TOOLS_PATH 覆盖）

$repo = Split-Path $PSScriptRoot -Parent

if (-not $env:IDF_PATH) {
    $candidate = Join-Path $repo 'esp-idf-v6.1.1'
    if (Test-Path $candidate) { $env:IDF_PATH = $candidate }
}
if (-not $env:IDF_TOOLS_PATH) {
    $env:IDF_TOOLS_PATH = Join-Path $repo '.tools\espressif'
}

if (-not $env:IDF_PATH) {
    Write-Error "找不到 ESP-IDF。请设置环境变量 IDF_PATH 指向你的 IDF 目录。"
    return
}

# 本机残留的 MSYS 环境会让 idf_tools 直接拒绝运行
$env:MSYSTEM = $null
$env:MSYS = $null

# 临时目录与 pip 缓存放到工作区，避免被系统沙箱拒绝
$tmp = Join-Path $env:IDF_TOOLS_PATH 'tmp'
$pip = Join-Path $env:IDF_TOOLS_PATH 'pip-cache'
New-Item -ItemType Directory -Force -Path $tmp, $pip | Out-Null
$env:TMP = $tmp
$env:TEMP = $tmp
$env:PIP_CACHE_DIR = $pip
$env:IDF_GITHUB_ASSETS = 'dl.espressif.cn/github_assets'

$venv = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf6.1_py3.12_env\Scripts'
if (Test-Path $venv) {
    $env:PATH = "$venv;$env:IDF_PATH\tools;$env:PATH"
}

& "$env:IDF_PATH\export.ps1"
