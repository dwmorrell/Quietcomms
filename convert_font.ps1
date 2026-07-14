param(
    [int]$SizePx = 16,
    [string]$Name = "PressStart2P16pt",
    [string]$TtfPath = "C:\Users\davis\Desktop\Claude\StageLink\PressStart2P-Regular.ttf",
    [int]$SpaceAdvancePct = 100  # shrink the space glyph's advance below the monospace cell width
)

Add-Type -AssemblyName System.Drawing

$ttfPath = $TtfPath
$pfc = New-Object System.Drawing.Text.PrivateFontCollection
$pfc.AddFontFile($ttfPath)
$family = $pfc.Families[0]

$sizePx = $SizePx
$font = New-Object System.Drawing.Font($family, $sizePx, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$sf = [System.Drawing.StringFormat]::GenericTypographic

# Measure true monospace advance at this size
$wide = New-Object System.Drawing.Bitmap 1,1
$gm = [System.Drawing.Graphics]::FromImage($wide)
$measured = $gm.MeasureString("MMMMMMMMMM", $font, [System.Drawing.PointF]::Empty, $sf)
$cellAdvance = [Math]::Round($measured.Width / 10)
$gm.Dispose(); $wide.Dispose()

$canvasSize = [Math]::Max(40, $sizePx * 3)
$first = 32
$last = 126
$ascentPx = $sizePx

Write-Host "Font: $($family.Name) size=$sizePx advance=$cellAdvance"

$glyphBitmaps = @{}
$glyphBounds = @{}
$globalMinRow = 999; $globalMaxRow = -999
$globalMinCol = 999; $globalMaxCol = -999

for ($c = $first; $c -le $last; $c++) {
    $ch = [char]$c
    $bmp = New-Object System.Drawing.Bitmap $canvasSize, $canvasSize
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $g.DrawString([string]$ch, $font, [System.Drawing.Brushes]::White, (New-Object System.Drawing.PointF(0,0)), $sf)

    $data = $bmp.LockBits((New-Object System.Drawing.Rectangle(0,0,$canvasSize,$canvasSize)), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($data.Stride * $canvasSize)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $bmp.UnlockBits($data)

    $minRow=999; $maxRow=-999; $minCol=999; $maxCol=-999
    $ink = New-Object 'bool[,]' $canvasSize,$canvasSize
    for ($y=0; $y -lt $canvasSize; $y++) {
        $rowStart = $y * $data.Stride
        for ($x=0; $x -lt $canvasSize; $x++) {
            $px = $rowStart + $x*4
            $r = $bytes[$px+2]
            if ($r -gt 127) {
                $ink[$y,$x] = $true
                if ($y -lt $minRow) { $minRow = $y }
                if ($y -gt $maxRow) { $maxRow = $y }
                if ($x -lt $minCol) { $minCol = $x }
                if ($x -gt $maxCol) { $maxCol = $x }
            }
        }
    }

    if ($minRow -eq 999) {
        $glyphBounds[$c] = @{blank=$true}
    } else {
        $glyphBounds[$c] = @{blank=$false; minRow=$minRow; maxRow=$maxRow; minCol=$minCol; maxCol=$maxCol}
        $glyphBitmaps[$c] = $ink
        if ($minRow -lt $globalMinRow) { $globalMinRow = $minRow }
        if ($maxRow -gt $globalMaxRow) { $globalMaxRow = $maxRow }
        if ($minCol -lt $globalMinCol) { $globalMinCol = $minCol }
        if ($maxCol -gt $globalMaxCol) { $globalMaxCol = $maxCol }
    }
    $g.Dispose(); $bmp.Dispose()
}

Write-Host "Global row range: $globalMinRow to $globalMaxRow"
Write-Host "Global col range: $globalMinCol to $globalMaxCol"

$bmW = $globalMaxCol - $globalMinCol + 1
$bmH = $globalMaxRow - $globalMinRow + 1
$yOffset = $globalMinRow - $ascentPx
$xOffset = $globalMinCol
Write-Host "Uniform glyph bitmap size: ${bmW}x${bmH}, xOffset=$xOffset yOffset=$yOffset xAdvance=$cellAdvance"

$bitmapBytes = New-Object System.Collections.Generic.List[byte]
$glyphTable = @()

for ($c = $first; $c -le $last; $c++) {
    $b = $glyphBounds[$c]
    $bitmapOffset = $bitmapBytes.Count
    if ($b.blank) {
        $adv = $cellAdvance
        if ($c -eq 32) { $adv = [Math]::Round($cellAdvance * $SpaceAdvancePct / 100) }
        $glyphTable += [PSCustomObject]@{ code=$c; bitmapOffset=$bitmapOffset; w=0; h=0; xAdvance=$adv; xOffset=0; yOffset=0 }
        continue
    }
    $ink = $glyphBitmaps[$c]
    $bitBuf = 0
    $bitCount = 0
    for ($y=0; $y -lt $bmH; $y++) {
        for ($x=0; $x -lt $bmW; $x++) {
            $srcY = $globalMinRow + $y
            $srcX = $globalMinCol + $x
            $bit = 0
            if ($ink[$srcY, $srcX]) { $bit = 1 }
            $bitBuf = ($bitBuf -shl 1) -bor $bit
            $bitCount++
            if ($bitCount -eq 8) {
                [void]$bitmapBytes.Add([byte]$bitBuf)
                $bitBuf = 0; $bitCount = 0
            }
        }
    }
    if ($bitCount -gt 0) {
        $bitBuf = $bitBuf -shl (8 - $bitCount)
        [void]$bitmapBytes.Add([byte]$bitBuf)
    }
    $glyphTable += [PSCustomObject]@{ code=$c; bitmapOffset=$bitmapOffset; w=$bmW; h=$bmH; xAdvance=$cellAdvance; xOffset=$xOffset; yOffset=$yOffset }
}

Write-Host "Total bitmap bytes: $($bitmapBytes.Count)"

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// Auto-generated from Press Start 2P (SIL Open Font License) at ${sizePx}px.")
[void]$sb.AppendLine("// Generated for StageLink's retro 8-bit UI; do not hand-edit, regenerate instead.")
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("#include <Arduino.h>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const uint8_t ${Name}Bitmaps[] PROGMEM = {")
$lineVals = @()
foreach ($byte in $bitmapBytes) {
    $lineVals += ("0x{0:X2}" -f $byte)
    if ($lineVals.Count -eq 16) {
        [void]$sb.AppendLine("  " + ($lineVals -join ", ") + ",")
        $lineVals = @()
    }
}
if ($lineVals.Count -gt 0) {
    [void]$sb.AppendLine("  " + ($lineVals -join ", "))
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const GFXglyph ${Name}Glyphs[] PROGMEM = {")
foreach ($gl in $glyphTable) {
    [void]$sb.AppendLine("  { $($gl.bitmapOffset), $($gl.w), $($gl.h), $($gl.xAdvance), $($gl.xOffset), $($gl.yOffset) },   // 0x$('{0:X2}' -f $gl.code) '$([char]$gl.code)'")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const GFXfont ${Name} PROGMEM = {")
[void]$sb.AppendLine("  (uint8_t  *)${Name}Bitmaps,")
[void]$sb.AppendLine("  (GFXglyph *)${Name}Glyphs,")
[void]$sb.AppendLine("  0x$('{0:X2}' -f $first), 0x$('{0:X2}' -f $last), $bmH")
[void]$sb.AppendLine("};")

$outPath = "C:\Users\davis\Desktop\Claude\StageLink\StageLink\${Name}.h"
[System.IO.File]::WriteAllText($outPath, $sb.ToString())
Write-Host "Wrote $outPath"
