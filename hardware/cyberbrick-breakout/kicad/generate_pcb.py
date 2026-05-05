#!/usr/bin/env python3
"""
generate_pcb.py — CyberBrick Breakout Board PCB layout generator
Writes a valid KiCad 7/8 .kicad_pcb file without requiring pcbnew.

Board: 85 x 70 mm
  - ABrobot ESP32-C3 OLED socket (U1): 2x 1x8 female headers, centred top-half
  - J1-J4:   SH1.0 3-pin joystick connectors, top edge
  - J9-J24:  SH1.0 2-pin button connectors, bottom edge
  - J6:      MCP23017 2x9 button header, right edge
  - J7:      I2C 1x4 header, right edge
  - J8:      Power 1x2 header, right edge
  - C1-C4:   0402 decoupling caps near joystick connectors
  - H1-H4:   M3 mounting holes
"""

import uuid as _uuid

BOARD_W = 85.0
BOARD_H = 70.0

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

# ── fp_text helper ────────────────────────────────────────────────────────────
def fp_ref(ref, rx, ry):
    emit(f'    (fp_text reference "{ref}" (at {rx} {ry}) (layer "F.SilkS")')
    emit(f'      (effects (font (size 1 1) (thickness 0.15))))')

def fp_val(value, vx, vy, hide=True):
    h = ' hide' if hide else ''
    emit(f'    (fp_text value "{value}" (at {vx} {vy}) (layer "F.Fab"){h}')
    emit(f'      (effects (font (size 1 1) (thickness 0.15))))')

# ── pad helpers ───────────────────────────────────────────────────────────────
def smd_pad(pad_num, net_name, x, y, w=1.0, h=1.5):
    n = net(net_name)
    nstr = f'(net {n} "{net_name}")' if net_name else ''
    emit(f'    (pad "{pad_num}" smd rect (at {x} {y}) (size {w} {h})')
    emit(f'      (layers "F.Cu" "F.Paste" "F.Mask") {nstr})')

def thru_pad(pad_num, net_name, x, y, drill=0.8, size=1.6):
    n = net(net_name)
    nstr = f'(net {n} "{net_name}")' if net_name else ''
    emit(f'    (pad "{pad_num}" thru_hole circle (at {x} {y}) (size {size} {size})')
    emit(f'      (drill {drill}) (layers "*.Cu" "*.Mask") {nstr})')

def np_pad(x, y, drill=3.2):
    emit(f'    (pad "" np_thru_hole circle (at {x} {y}) (size {drill} {drill})')
    emit(f'      (drill {drill}) (layers "*.Cu" "*.Mask"))')

# ── board header ─────────────────────────────────────────────────────────────
def header():
    emit('(kicad_pcb (version 20231120) (generator "cyberbrick-gen")')
    emit('  (general (thickness 1.6) (legacy_teardrops no))')
    emit('  (paper "A4")')
    emit('  (title_block')
    emit('    (title "CyberBrick Controller Breakout")')
    emit('    (rev "1.2")')
    emit('    (company "DIY")')
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
        (3.5, 3.5, "H1"), (BOARD_W-3.5, 3.5, "H2"),
        (3.5, BOARD_H-3.5, "H3"), (BOARD_W-3.5, BOARD_H-3.5, "H4"),
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
    # MP pad
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

# ── 0402 capacitor ────────────────────────────────────────────────────────────
def cap_0402(ref, cx, cy):
    emit(f'  (footprint "Capacitor_SMD:C_0402_1005Metric"')
    emit(f'    (layer "F.Cu") (at {cx} {cy}) (uuid "{uid()}")')
    fp_ref(ref, 0, -1.5)
    fp_val("100nF", 0, 1.5)
    smd_pad("1", "VCC",  -0.5, 0, w=0.5, h=0.5)
    smd_pad("2", "GND",   0.5, 0, w=0.5, h=0.5)
    emit('  )')

# ── 2.54mm through-hole header (1xN) ─────────────────────────────────────────
def header_1xN(ref, label, x0, y0, n_pins, net_list, pitch=2.54):
    emit(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_1x{n_pins:02d}_P2.54mm_Vertical"')
    emit(f'    (layer "F.Cu") (at {x0} {y0}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3)
    fp_val(label, 0, n_pins * pitch + 1)
    for i, net_name in enumerate(net_list):
        py = i * pitch
        thru_pad(str(i+1), net_name, 0, py)
    emit('  )')

# ── 2.54mm through-hole header (2xN) ─────────────────────────────────────────
def header_2xN(ref, label, x0, y0, n_pairs, net_list, pitch=2.54):
    emit(f'  (footprint "Connector_PinHeader_2.54mm:PinHeader_2x{n_pairs:02d}_P2.54mm_Vertical"')
    emit(f'    (layer "F.Cu") (at {x0} {y0}) (uuid "{uid()}")')
    fp_ref(ref, 0, -3)
    fp_val(label, pitch / 2, n_pairs * pitch + 1)
    for i, net_name in enumerate(net_list):
        col = i % 2
        row = i // 2
        px = col * pitch
        py = row * pitch
        thru_pad(str(i+1), net_name, px, py)
    emit('  )')

# ── 1x8 pin socket (female) ───────────────────────────────────────────────────
def socket_1x8(ref, label, x0, y0, net_list):
    """8-pin socket, pins laid out horizontally left→right. x0,y0 = pin 1 position."""
    emit(f'  (footprint "Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical"')
    emit(f'    (layer "F.Cu") (at {x0} {y0} 90) (uuid "{uid()}")')
    fp_ref(ref, 9, -3)
    fp_val(label, 9, 3)
    for i, net_name in enumerate(net_list):
        px = i * 2.54
        if net_name:
            thru_pad(str(i+1), net_name, px, 0)
        else:
            emit(f'    (pad "{i+1}" thru_hole circle (at {px} 0) (size 1.6 1.6)')
            emit(f'      (drill 0.8) (layers "*.Cu" "*.Mask"))')
    emit('  )')

# ── ESP32-C3 OLED socket ──────────────────────────────────────────────────────
# Module pin 1 = left side (USB-C end)
# Top row at y = CY - ROW_SEP/2, bottom row at y = CY + ROW_SEP/2
# 8 pins * 2.54mm = 19.32mm wide
ESP32_PIN1_X = 12.0   # X of pin 1 on both rows
ESP32_CY     = 28.0   # Y centre of module
ROW_SEP      = 15.24  # 600mil row separation — VERIFY against physical module!

def esp32_socket():
    # Top row T1-T8: IO8 IO9 IO8 IO7 IO6=SCL IO5=SDA IO4 IO3=ADC_RY
    top_nets = ["", "", "", "", "I2C_SCL", "I2C_SDA", "", "ADC_RY"]
    socket_1x8("U1A", "ESP32-C3 top",
                ESP32_PIN1_X,
                ESP32_CY - ROW_SEP / 2,
                top_nets)

    # Bottom row B1-B8: V5 GND VCC RX TX IO2=ADC_RX IO1=ADC_LY IO0=ADC_LX
    bot_nets = ["", "GND", "VCC", "", "", "ADC_RX", "ADC_LY", "ADC_LX"]
    socket_1x8("U1B", "ESP32-C3 bot",
                ESP32_PIN1_X,
                ESP32_CY + ROW_SEP / 2,
                bot_nets)

# ── Joystick connectors ───────────────────────────────────────────────────────
def joystick_connectors():
    configs = [
        ("J1", "LX", "ADC_LX"),
        ("J2", "LY", "ADC_LY"),
        ("J3", "RX", "ADC_RX"),
        ("J4", "RY", "ADC_RY"),
    ]
    for i, (ref, label, sig_net) in enumerate(configs):
        x = 10.0 + i * 13.0
        sh10_3pin(ref, label, x, 4.0, ["VCC", sig_net, "GND"])
        cap_0402(f"C{i+1}", x, 9.5)

# ── Button connectors ─────────────────────────────────────────────────────────
def button_connectors():
    btns = [
        ("J9",  "DL_UP",    "BTN_DL_UP"),
        ("J10", "DL_LEFT",  "BTN_DL_LEFT"),
        ("J11", "DL_RIGHT", "BTN_DL_RIGHT"),
        ("J12", "DL_DOWN",  "BTN_DL_DOWN"),
        ("J13", "DR_UP",    "BTN_DR_UP"),
        ("J14", "DR_LEFT",  "BTN_DR_LEFT"),
        ("J15", "DR_RIGHT", "BTN_DR_RIGHT"),
        ("J16", "DR_DOWN",  "BTN_DR_DOWN"),
        ("J17", "SHL1",     "BTN_SHL1"),
        ("J18", "SHL2",     "BTN_SHL2"),
        ("J19", "SHR1",     "BTN_SHR1"),
        ("J20", "SHR2",     "BTN_SHR2"),
        ("J21", "START",    "BTN_START"),
        ("J22", "SELECT",   "BTN_SELECT"),
        ("J23", "HOTKEY",   "BTN_HOTKEY"),
        ("J24", "COIN",     "BTN_COIN"),
    ]
    pitch = 4.8
    for i, (ref, label, btn_net) in enumerate(btns):
        row = i // 8
        col = i % 8
        x = 5.0 + col * pitch
        y = BOARD_H - 8.0 + row * 4.5
        sh10_2pin(ref, label, x, y, btn_net, "GND")

# ── Output headers ────────────────────────────────────────────────────────────
def output_headers():
    mcp_nets = [
        "BTN_DL_UP",   "BTN_DR_UP",
        "BTN_DL_LEFT", "BTN_DR_LEFT",
        "BTN_DL_RIGHT","BTN_DR_RIGHT",
        "BTN_DL_DOWN", "BTN_DR_DOWN",
        "BTN_SHL1",    "BTN_SHR1",
        "BTN_SHL2",    "BTN_SHR2",
        "BTN_START",   "BTN_SELECT",
        "BTN_HOTKEY",  "BTN_COIN",
        "GND",         "GND",
    ]
    header_2xN("J6", "MCP_BTNS", 76.5, 12.0, 9, mcp_nets)
    header_1xN("J7", "I2C",      76.5, 42.0, 4, ["I2C_SDA","I2C_SCL","VCC","GND"])
    header_1xN("J8", "PWR_IN",   76.5, 56.0, 2, ["VCC","GND"])

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

    # Board label
    emit(f'  (gr_text "CyberBrick Breakout v1.2" (at {BOARD_W/2} {BOARD_H/2 + 5}) (layer "F.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 1.2 1.2) (thickness 0.15))))')

    # GPIO2 warning
    emit(f'  (gr_text "GPIO2=BOOT STRAP: no pulldown on J3!" (at 42 50) (layer "F.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 0.6 0.6) (thickness 0.1))))')

    # Module measurement warning
    emit(f'  (gr_text "Verify U1 row spacing before ordering!" (at 35 35) (layer "F.SilkS")')
    emit(f'    (uuid "{uid()}")')
    emit(f'    (effects (font (size 0.6 0.6) (thickness 0.1))))')

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

print(f"Written {len(out)} bytes to {outpath}")
print("Footprints: ESP32-C3(2) J1-J4(4) J9-J24(16) J6+J7+J8(3) C1-C4(4) H1-H4(4) = 33")
