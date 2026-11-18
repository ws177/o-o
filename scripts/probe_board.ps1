<#
  probe_board.ps1 — 串口探测脚本（只读，不写任何东西到板子）
  用途：枚举串口 / 尝试读 ESP32 启动日志，确认芯片型号与 boot 模式。
  用法：pwsh -NoProfile -File probe_board.ps1 -Port COM12 -Baud 115200 -Seconds 3
#>
param(
  [string]$Port = "COM12",
  [int]$Baud = 115200,
  [double]$Seconds = 3.0,
  [switch]$NoReset
)

function Show-Line($t) { Write-Host $t }

Write-Host "=== 1. 串口列表 ==="
try {
  [System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { Show-Line ("  " + $_) }
} catch { Show-Line ("  ENUM FAILED: " + $_.Exception.Message) }

Write-Host ""
Write-Host "=== 2. 设备信息 (pnputil) ==="
try {
  $raw = & pnputil.exe /enum-devices /class Ports 2>&1
  $raw | Select-Object -First 40 | ForEach-Object { Show-Line ("  " + $_) }
} catch { Show-Line ("  pnputil FAILED: " + $_.Exception.Message) }

Write-Host ""
Write-Host ("=== 3. 打开 {0} @ {1} ===" -f $Port, $Baud)
$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName = $Port
$sp.BaudRate = $Baud
$sp.Parity = [System.IO.Ports.Parity]::None
$sp.DataBits = 8
$sp.StopBits = [System.IO.Ports.StopBits]::One
$sp.ReadTimeout = 150
$sp.WriteTimeout = 150
$sp.Handshake = [System.IO.Ports.Handshake]::None
$sp.DtrEnable = $false
$sp.RtsEnable = $false

try { $sp.Open() } catch { Show-Line ("  OPEN FAILED: " + $_.Exception.Message); exit 2 }
Show-Line "  opened OK"

if (-not $NoReset) {
  # 经典自动复位时序：EN 拉低 -> 释放
  try { $sp.RtsEnable = $true } catch {}
  Start-Sleep -Milliseconds 150
  try { $sp.RtsEnable = $false } catch {}
  Show-Line "  reset pulse sent"
}

# 收集字节
$buf = New-Object System.Collections.Generic.List[byte]
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
  try {
    $b = $sp.ReadByte()
    if ($b -ge 0) { $buf.Add([byte]$b) }
  } catch [System.TimeoutException] { }
  catch { break }
}
try { $sp.Close() } catch {}

Show-Line ("  received {0} bytes" -f $buf.Count)
Show-Line ""
Show-Line "=== 4. 原始内容 (ASCII, 非打印字符显示为 .) ==="
if ($buf.Count -eq 0) {
  Show-Line "  <无数据：板上没有输出程序，或串口不是调试口，或需要更长的复位等待>"
} else {
  $sb = New-Object System.Text.StringBuilder
  foreach ($b in $buf) {
    if ($b -eq 10) { [void]$sb.Append("`n") }
    elseif ($b -eq 13) { }
    elseif ($b -ge 32 -and $b -lt 127) { [void]$sb.Append([char]$b) }
    else { [void]$sb.Append(".") }
  }
  ($sb.ToString() -split "`n") | Select-Object -First 60 | ForEach-Object { Show-Line ("  " + $_) }
}
