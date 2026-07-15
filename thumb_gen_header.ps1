Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile("C:\Users\davis\Desktop\Claude\StageLink\images\thumb-cutout.png")
$srcW = $src.Width; $srcH = $src.Height

# Downsample the alpha channel only (color is applied at draw time per-theme)
# via simple box averaging, to a small embeddable resolution.
$dstH = 96
$dstW = [int][math]::Round($srcW * $dstH / $srcH)

$alpha = New-Object 'byte[]' ($dstW * $dstH)
for ($dy = 0; $dy -lt $dstH; $dy++) {
  $sy0 = [int]($dy * $srcH / $dstH)
  $sy1 = [int](($dy+1) * $srcH / $dstH); if ($sy1 -le $sy0) { $sy1 = $sy0 + 1 }
  for ($dx = 0; $dx -lt $dstW; $dx++) {
    $sx0 = [int]($dx * $srcW / $dstW)
    $sx1 = [int](($dx+1) * $srcW / $dstW); if ($sx1 -le $sx0) { $sx1 = $sx0 + 1 }
    $sum = 0; $cnt = 0
    for ($sy = $sy0; $sy -lt $sy1 -and $sy -lt $srcH; $sy++) {
      for ($sx = $sx0; $sx -lt $sx1 -and $sx -lt $srcW; $sx++) {
        $sum += $src.GetPixel($sx, $sy).A
        $cnt++
      }
    }
    $alpha[$dy * $dstW + $dx] = [byte][int]($sum / $cnt)
  }
}
$src.Dispose()

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("/*")
[void]$sb.AppendLine("  thumb_icon.h - alpha mask for the thumbs-up glyph, generated from")
[void]$sb.AppendLine("  images/thumbs up.jpg via thumb_cutout.ps1 + thumb_gen_header.ps1.")
[void]$sb.AppendLine("  Don't hand-edit; regenerate from the source image instead.")
[void]$sb.AppendLine("*/")
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const uint16_t THUMB_ICON_W = $dstW;")
[void]$sb.AppendLine("const uint16_t THUMB_ICON_H = $dstH;")
[void]$sb.AppendLine("const uint8_t THUMB_ICON_ALPHA[$($dstW * $dstH)] = {")
for ($row = 0; $row -lt $dstH; $row++) {
  $vals = for ($col = 0; $col -lt $dstW; $col++) { $alpha[$row*$dstW+$col] }
  [void]$sb.AppendLine("  " + ($vals -join ",") + ",")
}
[void]$sb.AppendLine("};")

[System.IO.File]::WriteAllText("C:\Users\davis\Desktop\Claude\StageLink\StageLink\thumb_icon.h", $sb.ToString())
Write-Output "wrote thumb_icon.h : ${dstW}x${dstH}"
