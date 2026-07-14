Add-Type -AssemblyName System.Drawing

$src = [System.Drawing.Bitmap]::FromFile("C:\Users\davis\Desktop\Claude\StageLink\images\ReelWorks Logo.png")

# 8-bit look: quantize to a low-res grid, then blow it back up with
# nearest-neighbor so the display bitmap has chunky NxN pixel blocks.
$lowW = 42; $lowH = 36          # quantization grid (aspect ~ 172/147)
$scale = 2                       # block size on the display
$dispW = $lowW * $scale          # 84
$dispH = $lowH * $scale          # 72

# Sample the source at the low-res grid, threshold to 1-bit (bright = the
# white reel shape = foreground; dark = transparent).
$grid = New-Object 'bool[,]' $lowH, $lowW
for ($gy = 0; $gy -lt $lowH; $gy++) {
  for ($gx = 0; $gx -lt $lowW; $gx++) {
    $sx = [int](($gx + 0.5) * $src.Width / $lowW)
    $sy = [int](($gy + 0.5) * $src.Height / $lowH)
    if ($sx -ge $src.Width) { $sx = $src.Width - 1 }
    if ($sy -ge $src.Height) { $sy = $src.Height - 1 }
    $p = $src.GetPixel($sx, $sy)
    $lum = 0.299 * $p.R + 0.587 * $p.G + 0.114 * $p.B
    $grid[$gy, $gx] = ($p.A -gt 128 -and $lum -gt 110)
  }
}
$src.Dispose()

# --- deliverable 1: transparent-background pixel-art PNG (upscaled 6x for viewing) ---
$viewScale = 6
$png = New-Object System.Drawing.Bitmap ($lowW * $viewScale), ($lowH * $viewScale)
for ($gy = 0; $gy -lt $lowH; $gy++) {
  for ($gx = 0; $gx -lt $lowW; $gx++) {
    $col = if ($grid[$gy, $gx]) { [System.Drawing.Color]::FromArgb(255, 240, 240, 240) } else { [System.Drawing.Color]::FromArgb(0, 0, 0, 0) }
    for ($by = 0; $by -lt $viewScale; $by++) {
      for ($bx = 0; $bx -lt $viewScale; $bx++) {
        $png.SetPixel($gx * $viewScale + $bx, $gy * $viewScale + $by, $col)
      }
    }
  }
}
$png.Save("C:\Users\davis\Desktop\Claude\StageLink\images\ReelWorks-8bit.png", [System.Drawing.Imaging.ImageFormat]::Png)
$png.Dispose()

# --- deliverable 2: 1-bit bitmap header for tft.drawBitmap on the boot screen ---
# Row-major, MSB-first, ceil(dispW/8) bytes per row. Set bit = draw in fg color,
# clear bit = leave the pixel untouched (transparent over the black boot screen).
$bytesPerRow = [Math]::Ceiling($dispW / 8)
$bytes = New-Object System.Collections.Generic.List[byte]
for ($y = 0; $y -lt $dispH; $y++) {
  for ($b = 0; $b -lt $bytesPerRow; $b++) {
    $val = 0
    for ($bit = 0; $bit -lt 8; $bit++) {
      $x = $b * 8 + $bit
      if ($x -lt $dispW) {
        $gx = [int]($x / $scale); $gy = [int]($y / $scale)
        if ($grid[$gy, $gx]) { $val = $val -bor (1 -shl (7 - $bit)) }
      }
    }
    [void]$bytes.Add([byte]$val)
  }
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// Auto-generated from images/ReelWorks Logo.png - 8-bit/quantized,")
[void]$sb.AppendLine("// 1 bit/pixel for tft.drawBitmap (set bits drawn in fg, clear = transparent).")
[void]$sb.AppendLine("// Regenerate with logo_convert.ps1, do not hand-edit.")
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("#include <Arduino.h>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#define REELWORKS_LOGO_W $dispW")
[void]$sb.AppendLine("#define REELWORKS_LOGO_H $dispH")
[void]$sb.AppendLine("const uint8_t reelWorksLogo[] PROGMEM = {")
$line = "  "
for ($i = 0; $i -lt $bytes.Count; $i++) {
  $line += ("0x{0:X2}, " -f $bytes[$i])
  if (($i + 1) % 12 -eq 0) { [void]$sb.AppendLine($line); $line = "  " }
}
if ($line.Trim().Length -gt 0) { [void]$sb.AppendLine($line) }
[void]$sb.AppendLine("};")

[System.IO.File]::WriteAllText("C:\Users\davis\Desktop\Claude\StageLink\StageLink\ReelWorksLogo.h", $sb.ToString())
Write-Output "logo: ${dispW}x${dispH} display bitmap, $($bytes.Count) bytes; PNG saved."
