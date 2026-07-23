// Parametric reconstruction of images/xtouch Battery Case Front.stl,
// rotated to portrait AND resized to the actual StageLink CYD board and
// display (see StageLink/case/stagelink_case.scad) instead of the
// reference part's own dimensions.
//
// Same caveat as xtouch_reference_back.scad: rebuilt from scratch by
// measuring the original STL mesh, not an automatic conversion - STLs
// carry no dimensions or feature history. The overall design language
// (screen cutout, corner bosses, edge slot, raised rib detail) is kept
// from the reference; the envelope/cutout are now sized to the real
// board/display instead of the reference's own 101.3 x 54.5mm frame.
//
// ---------------- real board/display dims (from stagelink_case.scad) ---
board_w = 50.0;
board_h = 86.5;
disp_w = 43.2;
disp_h = 57.6;
disp_off_top = 8.0;    // display active area offset from board top edge
wall = 2.4;             // frame margin around the board (matches stagelink_case.scad)

// ---------------- derived envelope (board + margin, same formula as
// stagelink_case.scad's inner_w/inner_h -> body_w/body_h) ----------------
port_w = board_w + 6 + 2*wall;   // 60.8
port_h = board_h + 6 + 2*wall;   // 97.3

frame_t = 12.0;          // overall thickness (kept from the reference)
corner_r = 8.0;           // outer corner radius

cutout_w = disp_w;
cutout_h = disp_h;
// board is centered in the frame (margins work out symmetric - see
// echo below); cutout is centered in X, offset in Y so its top edge sits
// disp_off_top below the board's (centered) top edge.
margin_y = (port_h - board_h)/2;
board_top_y = port_h - margin_y;
cutout_top_y = board_top_y - disp_off_top;
cutout_center_y = cutout_top_y - disp_h/2;
cutout_dx = 0;
cutout_dy = cutout_center_y - port_h/2;

boss_od = 6.0;
boss_id = 3.0;
boss_inset = 10.0;

edge_slot_w = 12.0;
edge_slot_h = 4.0;

rail_count = 3;
rail_w = 3.0;
rail_gap = 4.0;
rail_h = 4.0;

$fn = 48;

echo("FRONT (real board size): port_w =", port_w, " port_h =", port_h,
     " board margin x/y =", (port_w - board_w)/2, (port_h - board_h)/2);

module rrect(w, h, r) {
  hull() {
    for (p = [[r, r], [w - r, r], [r, h - r], [w - r, h - r]])
      translate(p) circle(r = r);
  }
}

module bezel_front() {
  cx = port_w/2 + cutout_dx;
  cy = port_h/2 + cutout_dy;

  difference() {
    linear_extrude(height = frame_t)
      rrect(port_w, port_h, corner_r);

    // screen cutout, straight through
    translate([cx - cutout_w/2, cy - cutout_h/2, -1])
      cube([cutout_w, cutout_h, frame_t + 2]);

    // top-edge slot (speaker/cable), centered on the width
    translate([port_w/2 - edge_slot_w/2, port_h - 4, frame_t - edge_slot_h - 1])
      cube([edge_slot_w, 6, edge_slot_h + 2]);

    // boss pilot holes (diagonal corners)
    for (p = [[boss_inset, port_h - boss_inset], [port_w - boss_inset, boss_inset]])
      translate([p[0], p[1], -1])
        cylinder(d = boss_id, h = frame_t + 2);
  }

  // raised bosses around the pilot holes, on the interior floor
  for (p = [[boss_inset, port_h - boss_inset], [port_w - boss_inset, boss_inset]])
    translate([p[0], p[1], 0])
      difference() {
        cylinder(d = boss_od, h = frame_t * 0.6);
        translate([0, 0, -1]) cylinder(d = boss_id, h = frame_t * 0.6 + 2);
      }

  // raised rib detail, bottom margin (between the frame's bottom wall and
  // the screen cutout) - horizontal ribs, spanning the width
  rail_y0 = corner_r * 0.6;
  for (i = [0:rail_count - 1])
    translate([6, rail_y0 + i*(rail_w + rail_gap), 0])
      cube([port_w - 12, rail_w, rail_h]);
}

bezel_front();
