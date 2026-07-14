// StageLink enclosure for the ESP32-2432S028R (2.8" CYD), portrait mount.
// Parametric OpenSCAD: renders a case BODY (battery bay at the back,
// board seated near the front on pillars, antenna port, USB cutouts) and
// a front BEZEL that floats on four corner tabs, leaving a light-leak
// gap around the display so the internal alert LED glows out
// jack-o-lantern style.
//
// IMPORTANT: verify board_w / board_h / hole spacing / port positions
// with calipers on YOUR boards before printing - CYD batches vary
// by 1-2 mm. Expect one test print for fit.
//
// Layout (front = +Z, open face):
//   floor -> battery bay (18650 holder or 4xAA holder) -> board pillars
//   -> CYD board (display facing out) -> bezel with glow gap.
//
// Print: body open-face-up, bezel flat, 0.2 mm layers. The RP-SMA hole
// prints fine without supports at this size.

// ---------------- parameters ----------------
battery_mode = "18650";   // "18650" | "4xAA" | "none"

board_w = 50.0;       // CYD short edge (portrait width) - VERIFY
board_h = 86.5;       // CYD long edge - VERIFY
board_t = 1.6;
hole_inset = 2.5;     // mounting hole center inset from board edges - VERIFY
disp_w = 43.2;        // 2.8" active area, portrait
disp_h = 57.6;
disp_off_top = 8.0;   // active area offset from board top edge - VERIFY

wall = 2.4;
floor_t = 2.4;
board_clear_front = 10.0;  // depth from board face to case mouth (display + bezel room)
glow_gap = 1.8;            // light-leak gap around the bezel frame
tab_w = 8.0;               // corner tabs holding the bezel
bezel_t = 2.0;

usb_w = 13.0; usb_h = 7.0;    // charge-module USB-C cutout (right wall)
uusb_w = 12.0; uusb_h = 6.5;  // CYD micro-USB service cutout (bottom wall)
ant_hole_d = 6.5;             // RP-SMA bulkhead (top wall)

bat_18650 = [78, 24, 24];     // holder envelope L x W x D - VERIFY yours
bat_4xaa  = [63, 59, 17];

// ---------------- derived ----------------
bay = (battery_mode == "18650") ? bat_18650 :
      (battery_mode == "4xAA")  ? bat_4xaa  : [0, 0, 0];

inner_w = max(board_w + 6, bay[1] + 6);
inner_h = max(board_h + 6, bay[0] + 6);
bay_d   = (battery_mode == "none") ? 0 : bay[2] + 3;
pillar_h = bay_d + 4;                        // board seat height above floor
inner_d = pillar_h + board_t + board_clear_front;

body_w = inner_w + 2*wall;
body_h = inner_h + 2*wall;
body_d = inner_d + floor_t;                  // open front

// board position in cavity (centered in w, pushed to the top in h)
bx = wall + (inner_w - board_w)/2;
by = wall + inner_h - board_h - 3;
seat_z = floor_t + pillar_h;                 // top of pillars = board underside

$fn = 48;

echo("BODY OUTER:", body_w, "x", body_h, "x", body_d);

// ---------------- body ----------------
module body() {
  difference() {
    cube([body_w, body_h, body_d]);
    translate([wall, wall, floor_t]) cube([inner_w, inner_h, inner_d + 1]);

    // USB-C charge port (right wall, at battery-bay level)
    translate([body_w - wall - 1, body_h/2 - usb_w/2, floor_t + 4])
      cube([wall + 2, usb_w, usb_h]);

    // CYD micro-USB service port (bottom wall, at board level)
    translate([body_w/2 - uusb_w/2, -1, seat_z - 1])
      cube([uusb_w, wall + 2, uusb_h + 2]);

    // RP-SMA antenna hole (top wall, near the front so the pigtail
    // routes over the board, away from the battery bay)
    translate([body_w/2, body_h - wall - 1, body_d - 10])
      rotate([-90, 0, 0]) cylinder(d = ant_hole_d, h = wall + 2);

    // side vents (left wall)
    for (i = [0:4])
      translate([-1, body_h*0.3 + i*8, floor_t + 3])
        cube([wall + 2, 4, 1.6]);
  }

  // board pillars: solid from the floor to the board seat, M3 pilot on top
  for (p = [[hole_inset, hole_inset],
            [board_w - hole_inset, hole_inset],
            [hole_inset, board_h - hole_inset],
            [board_w - hole_inset, board_h - hole_inset]])
    translate([bx + p[0], by + p[1], floor_t])
      difference() {
        cylinder(d = 7, h = pillar_h);
        translate([0, 0, pillar_h - 6]) cylinder(d = 2.6, h = 6.1);  // M3 self-tap pilot
      }

  // battery bay corral on the floor (holder drops in, foam or a zip tie
  // through the slots holds it)
  if (battery_mode != "none")
    translate([wall + (inner_w - bay[1])/2 - 2, wall + (inner_h - bay[0])/2 - 2, floor_t])
      difference() {
        cube([bay[1] + 4, bay[0] + 4, 5]);
        translate([2, 2, -1]) cube([bay[1], bay[0], 7]);
        // zip-tie slots
        translate([-1, (bay[0] + 4)/2 - 2, 1]) cube([bay[1] + 6, 4, 2.5]);
      }
}

// ---------------- bezel ----------------
// Sits inside the case mouth on four corner tabs; everywhere else a
// glow_gap channel separates bezel from walls, letting the internal LED
// light leak out around all four display edges.
module bezel() {
  frame_w = inner_w - 2*glow_gap;
  frame_h = inner_h - 2*glow_gap;

  // display opening, aligned to the board position (translated into
  // bezel-local coordinates)
  dx = (bx - wall - glow_gap) + (board_w - disp_w)/2;
  dy = (by - wall - glow_gap) + board_h - disp_off_top - disp_h;

  difference() {
    cube([frame_w, frame_h, bezel_t]);
    translate([dx, dy, -1]) cube([disp_w, disp_h, bezel_t + 2]);
  }
  // corner tabs: rise behind the bezel face and friction-fit (or glue)
  // against the inner walls at the case mouth
  for (p = [[0, 0], [1, 0], [0, 1], [1, 1]])
    translate([p[0]*(frame_w - tab_w), p[1]*(frame_h - tab_w), bezel_t])
      cube([tab_w, tab_w, 4]);
}

// ---------------- layout ----------------
body();
translate([body_w + 12, 0, 0]) bezel();
