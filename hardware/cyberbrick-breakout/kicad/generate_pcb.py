#!/usr/bin/env python3
"""
generate_pcb.py — CyberBrick Breakout Board PCB layout generator
KiCad 9 compatible .kicad_pcb

Board: 60 x 36 mm
  - ABrobot ESP32-C3 OLED: socketed on B.Cu, USB-C pointing RIGHT, display faces front
  - J1-J4:   SH1.0 3-pin joystick connectors, top edge (F.Cu)
  - J9-J24:  SH1.0 2-pin button connectors, bottom edge (F.Cu)
  - J6:      MCP23017 2x9 button header, right edge (F.Cu)
  - J7:      I2C 1x4 header, right edge (F.Cu)
  - J8:      Power 1x2 header, right edge (F.Cu)
  - C1-C4:   0402 decoupling caps (F.Cu)
  - H1-H4:   M3 mounting holes
"""

import uuid as _uuid

BOARD_W = 60.0
BOARD_H = 36.0

net_names = [
    "GND", "VCC",
    "ADC_LX", "ADC_LY", "ADC_RX", "ADC_RY",
    "I2C_SDA", "I2C_SCL",
    "BTN_DL_UP", "BTN_DL_LEFT", "BTN_DL_RIGHT", "BTN_DL_DOWN",
    "BTN_DR_UP", "BTN_DR_LEFT", "BTN_DR_RIGHT", "BTN_DR_DOWN",
    "BTN_SHL1", "BTN_SHL2", "BTN_SHR1", "BTN_SHR2",
    "BTN_START", "BTN_SELECT", "BTN_HOTKEY", "BTN_COIN",
]
net_idx = {n: i+1 for i, n in enumerate(net_names)}

def net(name):
    return net_idx.get(name, 0)

def uid():
    return str(_uuid.uuid4())

lines = []
def emit(s):
    lines.append(s)

# ── text helpers ──────────────────────────────────────────────────────────────
def fp_ref(ref, rx, ry, layer="F.SilkS"):
    emit(f'    (fp_text reference "{ref}" (at {rx} {ry}) (layer "{layer}")')
    emit(f'      (effects (font (size 1 1) (thickness 0.15))))')

def fp_val(value, vx, vy, layer="F.Fab"):
    emit(f'    (fp_text value "{value}" (at {vx} {vy}) (layer "{layer}") hide')
    emit(f'      (effects (font (size 1 1) (thickness 0.15))))')

def fp_ref_back(ref, rx, ry):
    fp_ref(ref, rx, ry, layer="B.SilkS")

def fp_val_back(value, vx, vy):
    fp_val(value, vx, vy, layer="B.Fab")

# ── pad helpers ───────────────────────────────────────────────────────────────
def smd_pad(pad_num, net_name, x, y, w=1.0, h=1.5, layer="F.Cu"):
    n = net(net_name)
    nstr = f'(net {n} "{net_name}")' if net_name else ''
    paste = layer.replace("Cu", "Paste")
    mask  = layer.replace("Cu", "Mask")
    emit(f'    (pad "{pad_num}" smd rect (at {x} {y}) (size {w} {h})')
    emit(f'      (layers "{layer}" "{paste}" "{mask}") {nstr})')

def thru_pad(pad_num, net_name, x, y, drill=0.8, size=1.6):
    n = net(net_name)
    nstr = f'(net {n} "{net_name}")' if net_name else ''
    emit(f'    (pad "{pad_num}" thru_hole circle (at {x} {y}) (size {size} {size})')
    emit(f'      (drill {drill}) (layers "*.Cu" "*.Mask") {nstr})')

def np_pad(x, y, drill=3.2):
    emit(f'    (pad "" np_thru_hole circle (at {x} {y}) (size {drill} {drill})')
    emit(f'      (drill {drill}) (layers "*.Cu" "*.Mask"))')

# ── board header ──────────────────────────────────────────────────────────────
def header():
    emit('(kicad_pcb (version 20231120) (generator "cyberbrick-gen")')
    emit('  (general (thickness 1.6) (legacy_teardrops no))')
    emit('  (paper "A4")')
    emit('  (title_block')
    emit('    (title "CyberBrick Controller Breakout")')
    emit('    (rev "1.3")')
    emit('    (company "DIY")')
    emit('    (comment 1 "60x36mm | ESP32-C3 on back, USB-C right, display faces front")')
    emit('  )')
    emit('  (layers')
    for spec in [
        '(0 "F.Cu" signal)',
        '(31 "B.Cu" signal)',
        '(32 "B.Adhes" user "B.Adhesive")',
        '(33 "F.Adhes" user "F.Adhesive")',
        '(34 "B.Paste" user)',
        '(35 "F.Paste" user)',
        '(36 "B.SilkS" user "B.Silkscreen")',
        '(37 "F.SilkS" user "F.Silkscreen")',
        '(38 "B.Mask" user)',
        '(39 "F.Mask" user)',
        '(40 "Dwgs.User" user "User.Drawings")',
        '(41 "Cmts.User" user "User.Comments")',
        '(42 "Eco1.User" user "User.Eco1")',
        '(43 "Eco2.User" user "User.Eco2")',
        '(44 "Edge.Cuts" user)',
        '(45 "Margin" user)',
        '(46 "B.CrtYd" user "B.Courtyard")',
        '(47 "F.CrtYd" user "F.Courtyard")',
        '(48 "B.Fab" user "B.Fabrication")',
        '(49 "F.Fab" user "F.Fabrication")',
    ]:
        emit(f'    {spec}')
    emit('  )')
    emit('  (net 0 "")')
    for name, idx in net_idx.items():
        emit(f'  (net {idx} "{name}")')

# ── board outline ─────────────────────────────────────────────────────────────
def edge_cuts():
    emit(f'  (gr_rect (start 0 0) (end {BOARD_W} {BOARD_H})')
    emit(f'    (stroke (width 0.05) (type solid))')
    emit(f'    (fill none)')
    emit(f'    (layer "Edge.Cuts")')
    emit(f'    (uuid "{uid()}"))')

# ── mounting holes ────────────────────────────────────────────────────────────
def mounting_holes():
    for x, y, ref in [
        (2.5, 2.5, "H1"), (BOARD_W-2.5, 2.5, "H2"),
        (2.5, BOARD_H-2.5, "H3"), (BOARD_W-2.5, BOARD_H-2.5, "H4"),
    ]:
        emit(f'  (footprint "MountingHole:MountingHole_3.2mm_M3"')
        emit(f'    (layer "F.Cu") (at {x} {y}) (uuid "{uid()}")')
        fp_ref(ref, 0, -4)
        fp_val("M3", 0, 4)
        np_pad(0, 0, drill=3.2)
        emit('  )')

# ── SH1.0 3-pin RA (SM03B-SRSS-TB) ──────────────────────────────────────────
def sh10_3pin(ref, label, cx, cy, nets3):
    emit(f'  (footprint "Connector_JST:JST_SH_SM03B-SRSS-TB_1x03-1MP_P1.00mm_Horizontal"')
    emit(f'    (layer "F.Cu") (at {cx} {cy}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3.5)
    fp_val(label, 0, 3.5)
    for i, (offset, net_name) in enumerate(zip([-1.0, 0.0, 1.0], nets3)):
        smd_pad(str(i+1), net_name, offset, 0, w=0.95, h=1.55)
    emit(f'    (pad "MP" smd rect (at 2.25 0) (size 1.2 2.0)')
    emit(f'      (layers "F.Cu" "F.Paste" "F.Mask"))')
    emit('  )')

# ── SH1.0 2-pin RA (SM02B-SRSS-TB) ──────────────────────────────────────────
def sh10_2pin(ref, label, cx, cy, net1, net2="GND"):
    emit(f'  (footprint "Connector_JST:JST_SH_SM02B-SRSS-TB_1x02-1MP_P1.00mm_Horizontal"')
    emit(f'    (layer "F.Cu") (at {cx} {cy}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3.0)
    fp_val(label, 0, 3.0)
    smd_pad("1", net1, -0.5, 0, w=0.95, h=1.55)
    smd_pad("2", net2,  0.5, 0, w=0.95, h=1.55)
    emit(f'    (pad "MP" smd rect (at 1.75 0) (size 1.2 2.0)')
    emit(f'      (layers "F.Cu" "F.Paste" "F.Mask"))')
    emit('  )')

# ── 0402 cap ──────────────────────────────────────────────────────────────────
def cap_0402(ref, cx, cy):
    emit(f'  (footprint "Capacitor_SMD:C_0402_1005Metric"')
    emit(f'    (layer "F.Cu") (at {cx} {cy}) (uuid "{uid()}")')
    fp_ref(ref, 0, -1.5)
    fp_val("100nF", 0, 1.5)
    smd_pad("1", "VCC", -0.5, 0, w=0.5, h=0.5)
    smd_pad("2", "GND",  0.5, 0, w=0.5, h=0.5)
    emit('  )')

# ── 2.54mm TH header 1xN ──────────────────────────────────────────────────────
def header_1xN(ref, label, x0, y0, n_pins, net_list, pitch=2.54):
    emit(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x{n_pins:02d}_P2.54mm_Vertical"')
    emit(f'    (layer "F.Cu") (at {x0} {y0}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3)
    fp_val(label, 0, n_pins * pitch + 1)
    for i, net_name in enumerate(net_list):
        thru_pad(str(i+1), net_name, 0, i * pitch)
    emit('  )')

# ── 2.54mm TH header 2xN ──────────────────────────────────────────────────────
def header_2xN(ref, label, x0, y0, n_pairs, net_list, pitch=2.54):
    emit(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_2x{n_pairs:02d}_P2.54mm_Vertical"')
    emit(f'    (layer "F.Cu") (at {x0} {y0}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3)
    fp_val(label, pitch / 2, n_pairs * pitch + 1)
    for i, net_name in enumerate(net_list):
        col = i % 2
        row = i // 2
        thru_pad(str(i+1), net_name, col * pitch, row * pitch)
    emit('  )')

# ── ESP32-C3 socket on B.Cu ───────────────────────────────────────────────────
# Board flipped: module on back, USB-C to the RIGHT, display faces front.
# Pin 1 of each row = rightmost pin (USB-C end).
# Two rows horizontal, 15.24mm apart vertically, centred on board.
# 8 pins × 2.54mm = 19.32mm wide → pin1_x = board_right - margin
# We place pin 1 at x=55, pins go LEFT (negative X direction) → use angle=270
# Row top at y = board_cy - 7.62, row bot at y = board_cy + 7.62

BOARD_CX   = BOARD_W / 2        # 30.0
BOARD_CY   = BOARD_H / 2        # 18.0
ROW_SEP    = 15.24               # 600 mil — VERIFY against physical module!
PIN1_X     = 55.0                # rightmost pin (USB-C end)
# angle=270: footprint rotated so pin 1 is on the right, pins go left

def socket_1x8_back(ref, label, x0, y0, net_list):
    """8-pin socket on B.Cu. Pins laid out horizontally, pin1 at x0,y0, going right→left."""
    emit(f'  (footprint "Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical"')
    emit(f'    (layer "B.Cu") (at {x0} {y0} 270) (uuid "{uid()}")')
    fp_ref_back(ref, 0, -3)
    fp_val_back(label, 0, 3)
    for i, net_name in enumerate(net_list):
        px = i * 2.54
        if net_name:
            thru_pad(str(i+1), net_name, px, 0)
        else:
            emit(f'    (pad "{i+1}" thru_hole circle (at {px} 0) (size 1.6 1.6)')
            emit(f'      (drill 0.8) (layers "*.Cu" "*.Mask"))')
    emit('  )')

def esp32_socket():
    # USB-C is to the right → pin 1 is rightmost.
    # Top row (viewed from back): IO8 IO9 IO8 IO7 IO6=SCL IO5=SDA IO4 IO3=ADC_RY
    top_nets = ["", "", "", "", "I2C_SCL", "I2C_SDA", "", "ADC_RY"]
    socket_1x8_back("U1A", "ESP32-C3 top",
                    PIN1_X, BOARD_CY - ROW_SEP / 2,
                    top_nets)
    # Bottom row: V5 GND VCC RX TX IO2=ADC_RX IO1=ADC_LY IO0=ADC_LX
    bot_nets = ["", "GND", "VCC", "", "", "ADC_RX", "ADC_LY", "ADC_LX"]
    socket_1x8_back("U1B", "ESP32-C3 bot",
                    PIN1_X, BOARD_CY + ROW_SEP / 2,
                    bot_nets)

# ── Joystick connectors — top edge, 4 evenly spaced ──────────────────────────
def joystick_connectors():
    configs = [
        ("J1", "LX", "ADC_LX"),
        ("J2", "LY", "ADC_LY"),
        ("J3", "RX", "ADC_RX"),
        ("J4", "RY", "ADC_RY"),
    ]
    # 4 connectors across 60mm board, avoid mounting holes at x=2.5
    xs = [8.0, 18.0, 28.0, 38.0]
    for (ref, label, sig_net), x in zip(configs, xs):
        sh10_3pin(ref, label, x, 3.0, ["VCC", sig_net, "GND"])
        cap_0402(f"C{configs.index((ref,label,sig_net))+1}", x, 8.5)

# ── Button connectors — bottom edge, 2 rows of 8 ─────────────────────────────
def button_connectors():
    btns = [
        ("J9",  "DL_UP",    "BTN_DL_UP"),
        ("J10", "DL_LFT",   "BTN_DL_LEFT"),
        ("J11", "DL_RGT",   "BTN_DL_RIGHT"),
        ("J12", "DL_DWN",   "BTN_DL_DOWN"),
        ("J13", "DR_UP",    "BTN_DR_UP"),
        ("J14", "DR_LFT",   "BTN_DR_LEFT"),
        ("J15", "DR_RGT",   "BTN_DR_RIGHT"),
        ("J16", "DR_DWN",   "BTN_DR_DOWN"),
        ("J17", "SHL1",     "BTN_SHL1"),
        ("J18", "SHL2",     "BTN_SHL2"),
        ("J19", "SHR1",     "BTN_SHR1"),
        ("J20", "SHR2",     "BTN_SHR2"),
        ("J21", "START",    "BTN_START"),
        ("J22", "SEL",      "BTN_SELECT"),
        ("J23", "HOTKEY",   "BTN_HOTKEY"),
        ("J24", "COIN",     "BTN_COIN"),
    ]
    pitch = 6.5
    for i, (ref, label, btn_net) in enumerate(btns):
        row = i // 8
        col = i % 8
        x = 4.5 + col * pitch
        y = BOARD_H - 4.5 + row * 4.0   # row 0 = y31.5, row 1 = y35.5 (just inside edge)
        sh10_2pin(ref, label, x, y, btn_net, "GND")

# ── Output headers — right edge ───────────────────────────────────────────────
def output_headers():
    # J6: MCP23017 2x9 — too tall for 36mm board, rotate 90°: place horizontally
    # Instead use two separate 1x9 headers side by side
    gpa_nets = ["BTN_DL_UP","BTN_DL_LEFT","BTN_DL_RIGHT","BTN_DL_DOWN",
                "BTN_DR_UP","BTN_DR_LEFT","BTN_DR_RIGHT","BTN_DR_DOWN","GND"]
    gpb_nets = ["BTN_SHL1","BTN_SHL2","BTN_SHR1","BTN_SHR2",
                "BTN_START","BTN_SELECT","BTN_HOTKEY","BTN_COIN","GND"]
    header_1xN("J6A", "GPA", 53.5, 12.0, 9, gpa_nets)
    header_1xN("J6B", "GPB", 57.5, 12.0, 9, gpb_nets)

    # J7: I2C 1x4
    header_1xN("J7", "I2C", 53.5, 38.0, 4, ["I2C_SDA","I2C_SCL","VCC","GND"])

    # J8: Power 1x2
    header_1xN("J8", "PWR", 57.5, 38.0, 2, ["VCC","GND"])

# ── GND fill + silkscreen ─────────────────────────────────────────────────────
def fills_and_silk():
    emit(f'  (zone (net {net("GND")}) (net_name "GND") (layer "B.Cu")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (hatch full 0.508)')
    emit(f'    (connect_pads (clearance 0.2))')
    emit(f'    (min_thickness 0.25)')
    emit(f'    (filled_areas_thickness no)')
    emit(f'    (fill yes')
    emit(f'      (thermal_gap 0.3)')
    emit(f'      (thermal_bridge_width 0.3)')
    emit(f'    )')
    emit(f'    (polygon')
    emit(f'      (pts')
    emit(f'        (xy 0.5 0.5)')
    emit(f'        (xy {BOARD_W-0.5} 0.5)')
    emit(f'        (xy {BOARD_W-0.5} {BOARD_H-0.5})')
    emit(f'        (xy 0.5 {BOARD_H-0.5})')
    emit(f'      )')
    emit(f'    )')
    emit('  )')

    emit(f'  (gr_text "CyberBrick Breakout v1.3" (at {BOARD_W/2} {BOARD_H/2}) (layer "F.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 1.0 1.0) (thickness 0.15))))')

    emit(f'  (gr_text "GPIO2=BOOT: no pulldown J3" (at {BOARD_W/2} {BOARD_H/2 + 3}) (layer "F.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 0.6 0.6) (thickness 0.1))))')

    emit(f'  (gr_text "Verify U1 row spacing!" (at {BOARD_W/2} {BOARD_H/2 + 5.5}) (layer "B.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 0.6 0.6) (thickness 0.1)) (justify mirror)))')

def footer():
    emit(')')

# ── Run ───────────────────────────────────────────────────────────────────────
header()
edge_cuts()
mounting_holes()
esp32_socket()
joystick_connectors()
button_connectors()
output_headers()
fills_and_silk()
footer()

out = "\n".join(lines)
outpath = "/tmp/esp32-c3-arcade/hardware/cyberbrick-breakout/kicad/cyberbrick-breakout.kicad_pcb"
with open(outpath, "w") as f:
    f.write(out)

print(f"Written {len(out)} bytes")
print(f"Board: {BOARD_W}x{BOARD_H}mm | ESP32-C3 on B.Cu, USB-C right, display front")
print(f"Footprints: U1(2) J1-J4(4) caps(4) J9-J24(16) J6A+J6B+J7+J8(4) holes(4) = 34")
