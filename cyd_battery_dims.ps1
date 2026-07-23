Add-Type -AssemblyName System.Drawing

# Dimensioned reference sheet for the ESP32-2432S028R (CYD) board + battery.
# Matches the user's "mockup test" Paint sketch (images/mockup_test_reference.png)
# for the overall concept: battery holder mounted sideways across the case
# width, its round cross-section forming the kickstand. This revision:
#  - Case face extended to 100mm tall (was 86mm, matching the bare board) -
#    7mm added above and below the board, split evenly.
#  - Battery compartment reshaped: a HALF-cylinder (D-shape) whose flat cut
#    face sits FLUSH against the case's own back wall (a real mating
#    surface, not a single tangent point), and only the round half pokes
#    out the back as the kickstand contact.
#  - Power button (20mm dia) and antenna port (6.5mm dia) both moved to the
#    RIGHT side wall (off the battery compartment), stacked vertically.
#  - USB-C charging port for the power control unit added on the bottom
#    edge, centered.
#  - Body depth bumped from 19mm to 21mm so the 20mm power button hole
#    actually fits through the wall with a 0.5mm buffer on each face.
#
# Sources:
#  - Board outline, hole spacing/diameter: official Shenzhen Jingcai spec
#    sheet (see chat).
#  - Screen 43.2 x 69.0mm, centered top-to-bottom: caliper-measured by the
#    user, overriding the datasheet's smaller "active area" figure.
#  - Battery: single 18650 holder w/ wires, 76 x 20 x 21mm, oriented with
#    its 76mm length running ACROSS the case width - rough diagram, not a
#    CAD export, adjust to taste.

# ---- case + board dims (mm) ----
$caseW = 65.0
$caseH = 100.0
$boardW = 50.0; $boardH = 86.0
$topMargin = ($caseH - $boardH) / 2      # 7.0
$botMargin = $caseH - $boardH - $topMargin  # 7.0
$boardMarginX = ($caseW - $boardW) / 2   # 7.5 - board centered in the case width
$holeInset = 4.0; $holeDia = 3.2
$dispW = 43.2; $dispH = 69.0
$dispOffTop = ($boardH - $dispH) / 2      # offset from the BOARD's own top edge
$boardT = 1.6
$glowGap = 1.8   # light-leak channel around the bezel frame, matching stagelink_case.scad's own glow_gap convention

# ---- RGB LED back-viewing hole ----
# The CYD's RGB alert LED sits on the component side of the board, near the
# landscape-left edge just above the ESP-WROOM-32 module (per the official
# spec sheet's interface diagram, page 5). Rotating that position into our
# portrait layout (matching the known micro-USB-at-bottom rotation already
# used elsewhere in this project) puts it roughly in the board's upper-right
# area. This is a rough estimate from a schematic, NOT a measurement -
# VERIFY against your actual board before drilling.
$ledX = 14.0   # from the board's left edge (mirrored from an earlier 36mm estimate -
               # user confirmed the micro-USB rotation had the X-axis flipped, so this
               # LED position, derived the same way, gets the same correction)
$ledY = 72.0   # from the board's bottom edge
$ledDia = 6.0   # viewing hole diameter - rough guess, size to taste
$bodyDepth = 21.0   # flat case depth throughout - was 19mm, +2mm so the 20mm power
                     # button fits with a 0.5mm buffer on each face (20 + 0.5 + 0.5 = 21)

# ---- power button + antenna, both on the RIGHT side wall (not the battery end) ----
$btnDia = 20.0
$btnSideY = 30.0        # button center height on the right wall, from the case bottom
$antDia = 6.5             # RP-SMA-style bulkhead, matching stagelink_case.scad's ant_hole_d
$antSideY = 55.0          # antenna center height on the right wall, from the case bottom
$sidePortBore = 4.0        # how far these holes bore into the wall, for the top-view notch

# ---- USB-C charging port for the power control unit, bottom edge ----
$usbW = 9.0; $usbH = 3.5
$usbBore = 4.0              # how far the port pocket reaches up into the case, for the top-view notch

# ---- CYD board's own micro-USB (flashing/serial), bottom edge ----
# Micro-USB sits on the portrait bottom edge (matches stagelink_case.scad's
# own placement of this port), offset toward the LEFT rather than centered -
# confirmed against the physical board. APPROXIMATE otherwise - the
# schematic-based rotation this was first derived from had the X-axis
# flipped (fixed here, and in the LED position below, which used the same
# derivation).
$uusbW = 12.0; $uusbH = 6.5
$uusbX = 12.0    # from the board's left edge (off-center, unlike the USB-C charge port) -
                 # confirmed by the user: bottom-LEFT, not bottom-right
$uusbBore = 4.0

# ---- battery (single 18650 holder, mounted sideways across the width) ----
$holderL = 76.0; $holderCross = 21.0   # holder length; cross-section dia (~holder thickness)
$clear = 3.0
$cylDia = $holderCross + $clear        # 24 - full cylinder dia (battery itself)
$cylLen = $holderL + 2                  # 78 - a hair over the holder itself for wire clearance
$overhang = ($cylLen - $caseW) / 2      # how far it pokes out past each side edge of the CASE
$cylMarginTop = 4.0                      # cylinder center inset from the case's top edge
$cylY = $caseH - $cylMarginTop - $cylDia/2   # cylinder center, from the case bottom edge

$bmp = New-Object System.Drawing.Bitmap 1900, 1250
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::White)

$black = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 2
$dim = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,30,90,200)), 1
$dash = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,150,150,150)), 1
$dash.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
$holeBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,220,60,50))
$btnBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,90,170,90))
$glowPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,255,160,60)), 1.5
$glowPen.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dot
$ledBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(180,180,80,220))
$ledPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,140,40,190)), 2
$ledPen.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
$caseFill = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,225,232,238))
$boardFill = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,245,235,180))
$cylFill = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,250,215,170))

$fontB = New-Object System.Drawing.Font("Segoe UI", 16, [System.Drawing.FontStyle]::Bold)
$font = New-Object System.Drawing.Font("Segoe UI", 11)
$fontSm = New-Object System.Drawing.Font("Segoe UI", 9)
$dimBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,30,90,200))
$footBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,210,120,20))
$btnTextBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,60,130,60))
$ledTextBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,140,40,190))
$glowTextBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,200,120,30))
$textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Black)
$mutedBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,110,110,110))

function DimH($g, $pen, $brush, $font, $x0, $x1, $y, $label) {
    $g.DrawLine($pen, $x0, $y, $x1, $y)
    $g.DrawLine($pen, $x0, $y-5, $x0, $y+5)
    $g.DrawLine($pen, $x1, $y-5, $x1, $y+5)
    $sz = $g.MeasureString($label, $font)
    $g.FillRectangle([System.Drawing.Brushes]::White, (($x0+$x1)/2 - $sz.Width/2 - 2), ($y - $sz.Height - 2), ($sz.Width+4), $sz.Height)
    $g.DrawString($label, $font, $brush, (($x0+$x1)/2 - $sz.Width/2), ($y - $sz.Height - 2))
}
function DimV($g, $pen, $brush, $font, $y0, $y1, $x, $label) {
    $g.DrawLine($pen, $x, $y0, $x, $y1)
    $g.DrawLine($pen, $x-5, $y0, $x+5, $y0)
    $g.DrawLine($pen, $x-5, $y1, $x+5, $y1)
    $sz = $g.MeasureString($label, $font)
    $g.FillRectangle([System.Drawing.Brushes]::White, ($x+6), (($y0+$y1)/2 - $sz.Height/2), ($sz.Width+4), $sz.Height)
    $g.DrawString($label, $font, $brush, ($x+8), (($y0+$y1)/2 - $sz.Height/2))
}

$g.DrawString("ESP32-2432S028R (CYD) board + battery - glow gap + LED viewing hole added", $fontB, $textBrush, 24, 20)
$g.DrawString("Face 100mm tall, body 21mm deep. Power button + antenna on the right wall, USB-C on the bottom edge. Glow gap around the bezel + a back-side hole over the RGB LED (approximate position). Rough diagram, verify before cutting.", $fontSm, $mutedBrush, 24, 52)

# ================= PANEL 1: TOP VIEW =================
$scale1 = 6.0
$p1x = 260; $p1y = 200
$cw1 = $caseW * $scale1; $ch1 = $caseH * $scale1
$bw1 = $boardW * $scale1; $bh1 = $boardH * $scale1

$g.DrawString("TOP VIEW", $fontB, $textBrush, $p1x, $p1y - 65)

# battery cylinder (drawn first, so the case sits visually "on top" of it)
$cylY0px = $p1y + ($caseH - $cylY - $cylDia/2) * $scale1
$caseCenterX = $p1x + $cw1/2
$cylXstart = $caseCenterX - ($cylLen/2)*$scale1
$cylWpx = $cylLen * $scale1
$cylHpx = $cylDia * $scale1
$g.FillRectangle($cylFill, $cylXstart, $cylY0px, $cylWpx, $cylHpx)
$g.DrawEllipse($black, $cylXstart, $cylY0px, $cylHpx, $cylHpx)
$g.DrawEllipse($black, ($cylXstart+$cylWpx-$cylHpx), $cylY0px, $cylHpx, $cylHpx)
$g.DrawLine($black, ($cylXstart+$cylHpx/2), $cylY0px, ($cylXstart+$cylWpx-$cylHpx/2), $cylY0px)
$g.DrawLine($black, ($cylXstart+$cylHpx/2), ($cylY0px+$cylHpx), ($cylXstart+$cylWpx-$cylHpx/2), ($cylY0px+$cylHpx))

# case outline (65mm x 100mm)
$g.FillRectangle($caseFill, $p1x, $p1y, $cw1, $ch1)
$g.DrawRectangle($black, $p1x, $p1y, $cw1, $ch1)

# board outline (50mm x 86mm, centered in width, 7mm margin top+bottom)
$bx1 = $p1x + $boardMarginX*$scale1
$by1 = $p1y + $topMargin*$scale1
$g.FillRectangle($boardFill, $bx1, $by1, $bw1, $bh1)
$g.DrawRectangle($black, $bx1, $by1, $bw1, $bh1)

# display (relative to the board's own top edge)
$dx0 = $bx1 + ($bw1 - $dispW*$scale1)/2
$dy0 = $by1 + $dispOffTop*$scale1
$g.DrawRectangle($dash, $dx0, $dy0, ($dispW*$scale1), ($dispH*$scale1))

# glow gap: thin bezel-frame channel between the bezel's outer edge and the
# case's own inner wall, letting the board's RGB LED light leak out around
# all four edges (same concept as stagelink_case.scad's glow_gap)
$glowInset = $glowGap * $scale1
$g.DrawRectangle($glowPen, ($p1x+$glowInset), ($p1y+$glowInset), ($cw1-2*$glowInset), ($ch1-2*$glowInset))

# RGB LED back-viewing hole - APPROXIMATE position, see comment above
$ledCx = $bx1 + $ledX*$scale1
$ledCy = $by1 + ($boardH - $ledY)*$scale1
$ledRpx = ($ledDia/2) * $scale1
$g.FillEllipse($ledBrush, ($ledCx-$ledRpx), ($ledCy-$ledRpx), (2*$ledRpx), (2*$ledRpx))
$g.DrawEllipse($ledPen, ($ledCx-$ledRpx), ($ledCy-$ledRpx), (2*$ledRpx), (2*$ledRpx))

# holes (relative to the board)
$holeR = ($holeDia/2) * $scale1
foreach ($hx in @(($holeInset), ($boardW - $holeInset))) {
    foreach ($hy in @(($holeInset), ($boardH - $holeInset))) {
        $cx = $bx1 + $hx*$scale1; $cy = $by1 + $hy*$scale1
        $g.FillEllipse($holeBrush, ($cx-$holeR), ($cy-$holeR), (2*$holeR), (2*$holeR))
        $g.DrawEllipse($black, ($cx-$holeR), ($cy-$holeR), (2*$holeR), (2*$holeR))
    }
}

# power button + antenna, notches on the RIGHT side wall (X = p1x+cw1)
$rightX = $p1x + $cw1
$btnCy1 = $p1y + ($caseH - $btnSideY) * $scale1
$btnHalfY1 = ($btnDia/2) * $scale1
$g.FillRectangle($btnBrush, $rightX, ($btnCy1-$btnHalfY1), ($sidePortBore*$scale1), (2*$btnHalfY1))
$g.DrawRectangle($black, $rightX, ($btnCy1-$btnHalfY1), ($sidePortBore*$scale1), (2*$btnHalfY1))

$antCy1 = $p1y + ($caseH - $antSideY) * $scale1
$antHalfY1 = ($antDia/2) * $scale1
$g.FillRectangle($footBrush, $rightX, ($antCy1-$antHalfY1), ($sidePortBore*$scale1), (2*$antHalfY1))
$g.DrawRectangle($black, $rightX, ($antCy1-$antHalfY1), ($sidePortBore*$scale1), (2*$antHalfY1))

# USB-C charging port, bottom edge (Y=0 face), centered in X
$usbCx1 = $caseCenterX
$usbY1 = $p1y + $ch1
$usbHalfW1 = ($usbW/2) * $scale1
$g.FillRectangle($btnBrush, ($usbCx1-$usbHalfW1), ($usbY1 - $usbBore*$scale1), (2*$usbHalfW1), ($usbBore*$scale1))
$g.DrawRectangle($black, ($usbCx1-$usbHalfW1), ($usbY1 - $usbBore*$scale1), (2*$usbHalfW1), ($usbBore*$scale1))

# micro-USB (flashing), bottom edge, offset right of the USB-C charge port
$uusbCx1 = $bx1 + $uusbX*$scale1
$uusbHalfW1 = ($uusbW/2) * $scale1
$g.FillRectangle($ledBrush, ($uusbCx1-$uusbHalfW1), ($usbY1 - $uusbBore*$scale1), (2*$uusbHalfW1), ($uusbBore*$scale1))
$g.DrawRectangle($ledPen, ($uusbCx1-$uusbHalfW1), ($usbY1 - $uusbBore*$scale1), (2*$uusbHalfW1), ($uusbBore*$scale1))

DimH $g $dim $dimBrush $font $p1x ($p1x+$cw1) ($p1y - 14) "65.0 mm case"
DimH $g $dim $dimBrush $fontSm $bx1 ($bx1+$bw1) ($p1y - 32) "50.0 mm board"
DimV $g $dim $dimBrush $font $p1y ($p1y+$ch1) ($p1x + $cw1 + 20) "100.0 mm case"
DimV $g $dim $dimBrush $fontSm ($by1+10) ($by1+$bh1-10) ($p1x + $cw1 + 140) "86.0 mm board"
DimH $g $dim $dimBrush $fontSm $cylXstart ($cylXstart+$cylWpx) ($cylY0px - 14) ([string]::Format("{0:N0} mm holder", $cylLen))

$sz1 = $g.MeasureString("Power button (20mm dia), 30mm up the right wall", $fontSm)
$g.FillRectangle([System.Drawing.Brushes]::White, ($rightX+$sidePortBore*$scale1+4), ($btnCy1-$sz1.Height/2), ($sz1.Width+4), $sz1.Height)
$g.DrawString("Power button (20mm dia), 30mm up the right wall", $fontSm, $btnTextBrush, ($rightX+$sidePortBore*$scale1+6), ($btnCy1-$sz1.Height/2))
$sz2 = $g.MeasureString("Antenna port (6.5mm dia), 55mm up the right wall", $fontSm)
$g.FillRectangle([System.Drawing.Brushes]::White, ($rightX+$sidePortBore*$scale1+4), ($antCy1-$sz2.Height/2), ($sz2.Width+4), $sz2.Height)
$g.DrawString("Antenna port (6.5mm dia), 55mm up the right wall", $fontSm, $footBrush, ($rightX+$sidePortBore*$scale1+6), ($antCy1-$sz2.Height/2))
$g.DrawLine($btnTextBrush, $usbCx1, ($usbY1+2), ($p1x+$cw1+20), ($p1y+$ch1-46))
$g.DrawString("USB-C charging (power unit)", $fontSm, $btnTextBrush, ($p1x+$cw1+20), ($p1y+$ch1-58))
$g.DrawLine($ledTextBrush, $uusbCx1, ($usbY1+2), ($p1x+$cw1+20), ($p1y+$ch1-16))
$g.DrawString("Micro-USB (flashing)", $fontSm, $ledTextBrush, ($p1x+$cw1+20), ($p1y+$ch1-28))

$g.DrawString("Hole dia 3.2mm, inset 4.0mm from every edge of the board", $fontSm, $mutedBrush, $p1x, ($p1y + $ch1 + 22))
$g.DrawString("Screen 43.2 x 69.0mm (grey dashed), centered on the board", $fontSm, $mutedBrush, $p1x, ($p1y + $ch1 + 40))
$g.DrawString([string]::Format("Battery sticks out {0:N1}mm past the case edges.", $overhang), $fontSm, $footBrush, $p1x, ($p1y + $ch1 + 58))
$g.DrawString([string]::Format("Glow gap: {0:N1}mm channel around the bezel frame (orange dotted) - lets the RGB LED light leak out.", $glowGap), $fontSm, $glowTextBrush, $p1x, ($p1y + $ch1 + 76))
$g.DrawString([string]::Format("LED viewing hole (purple, back side): {0:N0}mm dia, {1:N0}mm from left / {2:N0}mm from bottom of the board - APPROXIMATE, verify against your board.", $ledDia, $ledX, $ledY), $fontSm, $ledTextBrush, $p1x, ($p1y + $ch1 + 94))
$g.DrawString([string]::Format("Micro-USB flashing port (purple, bottom edge): {0:N0} x {1:N1}mm, {2:N0}mm from the board's left edge - APPROXIMATE, verify.", $uusbW, $uusbH, $uusbX), $fontSm, $ledTextBrush, $p1x, ($p1y + $ch1 + 112))

# ================= PANEL 2: SIDE VIEW =================
$scale2 = 8.0
$p2x = 950; $p2y = 240
$g.DrawString("SIDE VIEW - half-cylinder battery mount, flush against the case body", $fontB, $textBrush, $p2x, $p2y - 65)

$caseHpx2 = $caseH * $scale2
$bodyDepthPx = $bodyDepth * $scale2
$cylDiaPx = $cylDia * $scale2
$radiusPx = $cylDiaPx / 2

$originX = $p2x + 260   # front face X (right edge of the profile)
$topY = $p2y            # top of the case
$botY = $p2y + $caseHpx2  # bottom of the case
$backX = $originX - $bodyDepthPx   # the case's own flat back wall, constant depth throughout

# flat body outline (with a small gap in the bottom cap for the USB-C notch)
$g.DrawLine($black, $originX, $topY, $originX, $botY)   # front face
$usbZmid2 = $backX + $bodyDepthPx/2
$usbHalfW2 = ($usbW/2) * $scale2
$g.DrawLine($black, $originX, $botY, ($usbZmid2+$usbHalfW2), $botY)   # bottom cap, right of the notch
$g.DrawLine($black, ($usbZmid2-$usbHalfW2), $botY, $backX, $botY)      # bottom cap, left of the notch
$g.DrawLine($black, $backX, $botY, $backX, $topY)        # flat back wall (constant depth)
$g.DrawLine($black, $backX, $topY, $originX, $topY)      # top cap

# USB-C notch, poking up into the body from the bottom cap
$usbBorePx = $usbBore * $scale2
$g.FillRectangle($btnBrush, ($usbZmid2-$usbHalfW2), ($botY-$usbBorePx), (2*$usbHalfW2), $usbBorePx)
$g.DrawRectangle($black, ($usbZmid2-$usbHalfW2), ($botY-$usbBorePx), (2*$usbHalfW2), $usbBorePx)
$g.DrawString("USB-C", $fontSm, $btnTextBrush, ($usbZmid2-16), ($botY+8))

# battery half-cylinder: flat diameter face exactly on backX (flush mating
# surface, not a single tangent point), round half bulges further back
$cylCy = $topY + ($caseH - $cylY) * $scale2
$g.FillPie($cylFill, ($backX - $radiusPx), ($cylCy - $radiusPx), $cylDiaPx, $cylDiaPx, 90, 180)
$g.DrawPie($black, ($backX - $radiusPx), ($cylCy - $radiusPx), $cylDiaPx, $cylDiaPx, 90, 180)

# power button + antenna: real circular holes through the RIGHT wall - this
# Y-Z side view is exactly the wall's own face, so they render true-shape here
$portZmid2 = $backX + $bodyDepthPx/2
$btnCy2 = $botY - $btnSideY * $scale2
$btnRpx2 = ($btnDia/2) * $scale2
$g.FillEllipse($btnBrush, ($portZmid2-$btnRpx2), ($btnCy2-$btnRpx2), (2*$btnRpx2), (2*$btnRpx2))
$g.DrawEllipse($black, ($portZmid2-$btnRpx2), ($btnCy2-$btnRpx2), (2*$btnRpx2), (2*$btnRpx2))
$g.DrawString([string]::Format("power button ({0:N0}mm dia)", $btnDia), $fontSm, $btnTextBrush, ($originX + 18), ($btnCy2 - 7))

$antCy2 = $botY - $antSideY * $scale2
$antRpx2 = ($antDia/2) * $scale2
$g.FillEllipse($footBrush, ($portZmid2-$antRpx2), ($antCy2-$antRpx2), (2*$antRpx2), (2*$antRpx2))
$g.DrawEllipse($black, ($portZmid2-$antRpx2), ($antCy2-$antRpx2), (2*$antRpx2), (2*$antRpx2))
$g.DrawString([string]::Format("antenna port ({0:N1}mm dia)", $antDia), $fontSm, $footBrush, ($originX + 18), ($antCy2 - 7))

# board strip along the front-top
$g.FillRectangle([System.Drawing.Brushes]::Khaki, ($originX - $bodyDepthPx + 3), ($topY + 40), ($bodyDepthPx - 8), 16)
$g.DrawRectangle($black, ($originX - $bodyDepthPx + 3), ($topY + 40), ($bodyDepthPx - 8), 16)
$g.DrawString("board", $fontSm, $mutedBrush, ($originX - $bodyDepthPx), ($topY + 20))

DimV $g $dim $dimBrush $fontSm $topY $botY ($originX + 30) "100.0 mm case"
DimH $g $dim $dimBrush $fontSm $backX $originX ($botY + 20) ([string]::Format("{0:N0} mm flat body", $bodyDepth))
DimH $g $dim $dimBrush $fontSm ($backX - $radiusPx) $backX ($cylCy - $radiusPx - 14) ([string]::Format("{0:N0} mm dia", $cylDia))
$g.DrawString([string]::Format("Flat cut face of the battery half-cylinder sits flush on the {0:N0}mm back wall -", $bodyDepth), $fontSm, $footBrush, $p2x, ($botY + 44))
$g.DrawString("a real mating surface, not a tangent point. Round half (12mm) pokes out as the kickstand.", $fontSm, $footBrush, $p2x, ($botY + 62))
$g.DrawString("Cylinder axis runs left-right (into/out of the page here) - see top view for its length.", $fontSm, $mutedBrush, $p2x, ($botY + 80))
$g.DrawString("A micro-USB flashing port also sits on the bottom edge, offset in X from the USB-C port - see top view.", $fontSm, $ledTextBrush, $p2x, ($botY + 98))

$g.Dispose()
$bmp.Save("C:\Users\davis\Desktop\Claude\StageLink\.claude\worktrees\case-mockup\images\cyd_battery_dims.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved"
