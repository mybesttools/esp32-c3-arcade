// BLE Everywhere PSP-style handheld case (parametric)
// Open in OpenSCAD and adjust the dimensions in the PARAMETERS section.
// Exports: front shell + back shell side-by-side for 3D printing.

$fn = 64;

// --------------------
// PARAMETERS
// --------------------
// Overall handheld body
case_w = 220;
case_h = 120;
case_d = 24;
corner_r = 14;

// Shell and fit
wall = 2.2;
split_z = 13;           // Front shell depth
lip_h = 2.0;            // Interlocking lip height
lip_clearance = 0.25;   // Fit tolerance between shells

// Front opening windows / controls
// Main display presets:
// - "pitft43"  : Adafruit PiTFT 4.3 class opening (landscape)
// - "custom"   : use screen_w/screen_h directly
main_display_preset = "pitft43";

// BTT PITFT43 V2.0 (from provided dimension drawing)
// PCB outer: 105.50 x 67.00 mm
// Mount-hole center pitch: 97.50 x 59.00 mm
pitft_pcb_w = 105.50;
pitft_pcb_h = 67.00;

// Front bezel target around PITFT43 opening.
// Requested: left/right/top = 5.5 mm, bottom = 8.5 mm
pitft_bezel_left = 5.5;
pitft_bezel_right = 5.5;
pitft_bezel_top = 5.5;
pitft_bezel_bottom = 8.5;

// PITFT board center on the front face.
pitft_center_x = 0;
pitft_center_y = 16;

pitft_window_w = pitft_pcb_w - pitft_bezel_left - pitft_bezel_right;
pitft_window_h = pitft_pcb_h - pitft_bezel_top - pitft_bezel_bottom;
pitft_window_center_y_offset = (pitft_bezel_bottom - pitft_bezel_top) / 2;

pitft_hole_pitch_x = 97.50;
pitft_hole_pitch_y = 59.00;
pitft_hole_d = 3.2;         // M3 clearance on display PCB
pitft_standoff_d = 7.0;     // case-side post diameter
pitft_standoff_hole_d = 3.2; // M3 clearance
pitft_mount_enabled = true;

screen_w = (main_display_preset == "pitft43") ? pitft_window_w : 42;
screen_h = (main_display_preset == "pitft43") ? pitft_window_h : 25;
screen_x = (main_display_preset == "pitft43") ? pitft_center_x : 0;
screen_y = (main_display_preset == "pitft43") ? (pitft_center_y + pitft_window_center_y_offset) : 16;

// Optional tiny onboard status display window (0.42-inch class).
tiny_status_window_enabled = true;
tiny_status_w = 18;
tiny_status_h = 9;
tiny_status_x = 0;
tiny_status_y = 49;

joystick_d = 24;
joystick_x_base = 82;
btn_cluster_x_base = 82;

// Fixed layout for current preview orientation: joystick LEFT, ABXY RIGHT.
joystick_x = abs(joystick_x_base);
joystick_y = -8;

btn_d = 10;
btn_pitch = 12.5;
btn_cluster_x = -abs(btn_cluster_x_base);
btn_cluster_y = -2;

echo(str("JOYSTICK_X=", joystick_x, " BTN_CLUSTER_X=", btn_cluster_x));

start_d = 8;
start_x = -10;
start_y = -27;
select_x = 10;
select_y = -27;

hotkey_d = 8;
hotkey_x = -25;
hotkey_y = -27;
coin_d = 8;
coin_x = 25;
coin_y = -27;

// USB-C cutout (top edge)
usb_w = 10;
usb_h = 4.2;
usb_z = 6;

// Main board keepout and mounts (adjust to real board)
board_w = 26;
board_h = 48;
board_th = 8;
board_x = 0;
board_y = 0;

mount_post_d = 6;
mount_hole_d = 2.4;    // M2 self-tapping pilot
mount_inset_x = 9;
mount_inset_y = 20;

// Assembly screw bosses (4 corner screws)
screw_post_d = 8;
screw_hole_d = 2.7;
screw_inset = 10;

// Printing layout
explode = 10;
print_gap = 12;
assembled_view = false;

// --------------------
// GEOMETRY HELPERS
// --------------------
module rounded_rect_2d(w, h, r) {
  hull() {
    translate([ w/2-r,  h/2-r]) circle(r=r);
    translate([-w/2+r,  h/2-r]) circle(r=r);
    translate([-w/2+r, -h/2+r]) circle(r=r);
    translate([ w/2-r, -h/2+r]) circle(r=r);
  }
}

module rounded_box(w, h, d, r) {
  linear_extrude(height=d)
    rounded_rect_2d(w, h, r);
}

module dpad_cutouts() {
  // Optional D-pad style cross if you want it later.
}

module button_cluster_4(x, y, d, p) {
  translate([x, y, 0]) {
    translate([-p/2, 0, 0]) circle(d=d);
    translate([ p/2, 0, 0]) circle(d=d);
    translate([0,  p/2, 0]) circle(d=d);
    translate([0, -p/2, 0]) circle(d=d);
  }
}

module front_cutout_2d() {
  union() {
    // Screen window
    translate([screen_x, screen_y])
      rounded_rect_2d(screen_w, screen_h, 3);

    // Tiny onboard display/status window.
    if (tiny_status_window_enabled)
      translate([tiny_status_x, tiny_status_y])
        rounded_rect_2d(tiny_status_w, tiny_status_h, 1.5);

    // Joystick hole
    translate([joystick_x, joystick_y]) circle(d=joystick_d);

    // ABXY cluster
    button_cluster_4(btn_cluster_x, btn_cluster_y, btn_d, btn_pitch);

    // Start / Select / Hotkey / Coin
    translate([start_x, start_y]) circle(d=start_d);
    translate([select_x, select_y]) circle(d=start_d);
    translate([hotkey_x, hotkey_y]) circle(d=hotkey_d);
    translate([coin_x, coin_y]) circle(d=coin_d);
  }
}

module board_keepout() {
  translate([board_x - board_w/2, board_y - board_h/2, wall])
    cube([board_w, board_h, board_th + 3]);
}

module board_mount_posts(z0, z1) {
  // Four posts around board area.
  for (sx = [-1, 1])
    for (sy = [-1, 1]) {
      x = board_x + sx * mount_inset_x;
      y = board_y + sy * mount_inset_y;

      translate([x, y, z0])
        difference() {
          cylinder(h=z1-z0, d=mount_post_d);
          translate([0, 0, -0.1]) cylinder(h=(z1-z0)+0.2, d=mount_hole_d);
        }
    }
}

module pitft_keepout() {
  // Dedicated cavity volume for the 4.3" display PCB stack.
  // +2mm margins keep room for components and cable bend.
  translate([pitft_center_x - (pitft_pcb_w + 2)/2, pitft_center_y - (pitft_pcb_h + 2)/2, wall])
    cube([pitft_pcb_w + 2, pitft_pcb_h + 2, board_th + 6]);
}

module pitft_mount_posts(z0, z1) {
  if (pitft_mount_enabled) {
    for (sx = [-1, 1])
      for (sy = [-1, 1]) {
        x = pitft_center_x + sx * (pitft_hole_pitch_x / 2);
        y = pitft_center_y + sy * (pitft_hole_pitch_y / 2);

        translate([x, y, z0])
          difference() {
            cylinder(h=z1-z0, d=pitft_standoff_d);
            translate([0, 0, -0.1]) cylinder(h=(z1-z0)+0.2, d=pitft_standoff_hole_d);
          }
      }
  }
}

module shell_screw_posts(z0, z1) {
  for (sx = [-1, 1])
    for (sy = [-1, 1]) {
      x = sx * (case_w/2 - screw_inset);
      y = sy * (case_h/2 - screw_inset);

      translate([x, y, z0])
        difference() {
          cylinder(h=z1-z0, d=screw_post_d);
          translate([0, 0, -0.1]) cylinder(h=(z1-z0)+0.2, d=screw_hole_d);
        }
    }
}

module front_shell() {
  difference() {
    union() {
      // Outer front shell block
      rounded_box(case_w, case_h, split_z, corner_r);

      // Inner lip that mates into back shell
      translate([0, 0, split_z - lip_h])
        linear_extrude(height=lip_h)
          rounded_rect_2d(case_w - 2*(wall + lip_clearance),
                          case_h - 2*(wall + lip_clearance),
                          max(1, corner_r - wall - lip_clearance));

      // Internal posts
      board_mount_posts(wall, split_z - wall);
      pitft_mount_posts(wall, split_z - wall);
      shell_screw_posts(wall, split_z - wall);
    }

    // Hollow cavity
    translate([0, 0, wall])
      rounded_box(case_w - 2*wall, case_h - 2*wall, split_z, max(1, corner_r - wall));

    // Front face cutouts
    translate([0, 0, -0.1])
      linear_extrude(height=wall + 0.3)
        front_cutout_2d();

    // Board keepout
    board_keepout();
    pitft_keepout();
  }
}

module back_shell() {
  back_d = case_d - split_z;

  difference() {
    union() {
      rounded_box(case_w, case_h, back_d, corner_r);

      // Receiving channel for front lip
      // (implemented by subtractive shape below)

      // Internal posts
      board_mount_posts(wall, back_d - wall);
      pitft_mount_posts(wall, back_d - wall);
      shell_screw_posts(wall, back_d - wall);
    }

    // Hollow cavity
    translate([0, 0, wall])
      rounded_box(case_w - 2*wall, case_h - 2*wall, back_d, max(1, corner_r - wall));

    // Lip receiver pocket
    translate([0, 0, -0.1])
      linear_extrude(height=lip_h + 0.3)
        rounded_rect_2d(case_w - 2*(wall - lip_clearance),
                        case_h - 2*(wall - lip_clearance),
                        max(1, corner_r - wall + lip_clearance));

    // USB-C top-edge cutout (centered)
    translate([-usb_w/2, case_h/2 - wall - 0.2, usb_z])
      cube([usb_w, wall + 1.0, usb_h]);
  }
}

module layout_print() {
  // Front shell left, back shell right
  translate([-(case_w/2 + print_gap/2), 0, 0])
    front_shell();

  translate([(case_w/2 + print_gap/2), 0, 0])
    back_shell();
}

module layout_assembled() {
  front_shell();
  translate([0, 0, split_z + explode])
    back_shell();
}

if (assembled_view)
  layout_assembled();
else
  layout_print();
