// Parametric reconstruction of images/xtouch Battery Case Back.stl,
// rotated to portrait, moved to the TOP of the case, and resized to fit a
// real single-cell 18650 holder instead of the reference part's own
// dimensions (this is what you asked for: battery section at the top of
// the back side, its bulge forming the kickstand wedge).
//
// Same caveat as xtouch_reference_front.scad: rebuilt from scratch by
// measuring the original STL mesh, not an automatic conversion. Design
// language kept from the reference (tapered wedge tray, 4 corner pegs, 2
// slots in the thick end wall); envelope is now sized to the actual
// holder, not the reference's own 101.3 x 47.9 x 37.6mm block.
//
// SIZE NOTE: an 18650 holder is 76mm long, which does not fit within the
// board's own 97.3mm-tall footprint alongside the display (confirmed and
// accepted - see conversation). So this back piece is now TALLER than
// the front bezel: the battery pocket + taper sit above the front
// piece's own height, and a thin flat cover continues down the rest of
// the way to match the front's height for the board/display zone below.
// Total case height grows from ~97.3mm to ~225mm.
//
// ---------------- battery holder (measured, single 18650 + wires) -------
holder_L = 76.0;    // holder length
holder_W = 20.0;     // holder width
holder_T = 21.0;     // holder thickness (holds the 18.6mm cell + contacts)
clear    = 4.0;       // clearance added to each pocket dimension

// ---------------- envelope ----------------
port_w = 60.8;        // matches xtouch_reference_front.scad's port_w
port_h = 97.3;         // matches xtouch_reference_front.scad's port_h (board/display zone)

pocket_y = holder_L + clear;   // 80 - flat-pocket run (holds the holder's length)
pocket_w = holder_W + clear;   // 24 - flat-pocket width (holder centered in port_w)
pocket_z = holder_T + clear;   // 25 - flat-pocket depth (holder thickness + clearance)

wall       = 2.5;        // tray wall thickness
board_wall = 3.0;         // thin flat back-cover thickness over the board/display zone
thick_z    = pocket_z + wall;   // 27.5 - tray's outer depth at the flat pocket

prop_angle = 30;             // desk-rest angle off horizontal, same target
                              // used in stagelink_case.scad's own kickstand
taper_transition = (thick_z - board_wall) / tan(prop_angle);  // run needed to taper
                                                                // back to board_wall
back_y_total = pocket_y + taper_transition;      // battery-zone Y (pocket + taper),
                                                  // measured down from the case's top edge
total_h = back_y_total + port_h;                 // full case height, top edge to bottom

echo("BACK total case height =", total_h, "mm (board/display zone", port_h,
     "+ battery zone", back_y_total, ")");

peg_dia   = 5.5;
peg_h     = 5.0;
peg_inset = 8.0;

slot_w      = 5.5;
slot_h      = 14.0;
slot_inset  = 11.0;
slot_z_mid  = thick_z * 0.55;

$fn = 48;

echo("BACK (real 18650 holder): pocket_y =", pocket_y, " thick_z =", thick_z,
     " taper_transition =", taper_transition, " -> battery zone Y =", back_y_total);

// Local frame: local_y = 0 at the thick (top) end, increasing DOWN into
// the case. Flat-bottomed for the first pocket_y (holds the holder), then
// a straight taper down to board_wall (matching the thin cover below)
// over taper_transition, then a thin flat cover for the rest of the case
// height. Mirrored into world coordinates at the bottom of this file so
// local_y=0 lands at the case's actual top edge (world y = total_h) and
// increasing local_y moves down the case (world y decreasing).
module wedge_profile(w, depth_flat, y_flat, y_taper_end, depth_end) {
  pts = [[0, 0], [0, -depth_flat], [y_flat, -depth_flat],
         [y_taper_end, -depth_end], [y_taper_end, 0]];
  rotate([90, 0, 90])
    linear_extrude(height = w)
      polygon(points = pts);
}

// thin flat cover for the board/display zone, continuing from where the
// taper ends down to the bottom of the case
module cover_slab(w, y0, y1, depth) {
  pts = [[y0, 0], [y0, -depth], [y1, -depth], [y1, 0]];
  rotate([90, 0, 90])
    linear_extrude(height = w)
      polygon(points = pts);
}

module back_tray_local() {
  union() {
    difference() {
      wedge_profile(port_w, thick_z, pocket_y, back_y_total, board_wall);

      // hollow the tray, opening upward (+Z, toward the front bezel) -
      // only over the pocket + taper region, not the thin cover below
      translate([wall, wall, 0])
        wedge_profile(port_w - 2*wall, thick_z - wall, pocket_y + 2, back_y_total, 1);

      // 2 vertical slots through the thick (top) end wall
      for (sx = [slot_inset, port_w - slot_inset])
        translate([sx, -1, -slot_z_mid - slot_h/2])
          rotate([-90, 0, 0])
            linear_extrude(height = wall + 2)
              offset(r = slot_w/2 - 0.01)
                square([0.02, slot_h - slot_w], center = true);
    }

    // 4 corner pegs, proud of the rim, within the flat-pocket region
    for (p = [[peg_inset, peg_inset], [port_w - peg_inset, peg_inset],
              [peg_inset, pocket_y - peg_inset], [port_w - peg_inset, pocket_y - peg_inset]])
      translate([p[0], p[1], 0])
        cylinder(d = peg_dia, h = peg_h);

    // thin flat back cover over the board/display zone
    cover_slab(port_w, back_y_total, total_h, board_wall);
  }
}

// place at the top of a portrait case: local y=0 (thick/pocket end) ->
// world y=total_h (the case's new, taller top edge); local y increases
// downward toward the bottom of the case
translate([0, total_h, 0])
  mirror([0, 1, 0])
    back_tray_local();
