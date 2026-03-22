#!/usr/bin/env python3
"""
Draw a PNG diagram of the logOS 2.0 physical memory layout.
32KB process slots, 8 slots total.
"""

from PIL import Image, ImageDraw, ImageFont
import os

OUT_PATH = os.path.join(os.path.dirname(__file__), "logos-memory-layout.png")

# ============================================================
# Configuration
# ============================================================
W = 780
MARGIN_L = 30
MARGIN_R = 30
MARGIN_T = 30
MARGIN_B = 40

# Colours
BG           = (255, 255, 255)
BORDER       = (60, 60, 60)
TEXT         = (30, 30, 30)
GREY         = (110, 110, 110)
LIGHT_GREY   = (180, 180, 180)

ROM_COL      = (255, 220, 140)    # warm yellow
KERNEL_COL   = (140, 200, 255)    # light blue
SLOT_COL     = (180, 230, 180)    # light green
SLOT_ALT     = (160, 215, 160)    # slightly darker green for alternating
MMIO_COL     = (220, 190, 255)    # light purple
FREE_COL     = (235, 235, 235)    # light grey for unused
BOOT_BSS_COL = (255, 200, 180)   # light salmon

# Slot detail colours
SEC_TEXT     = (255, 240, 210)    # .text
SEC_RODATA   = (210, 235, 255)   # .rodata
SEC_DATA     = (210, 255, 210)   # .data
SEC_BSS      = (230, 230, 250)   # .bss
SEC_FREE     = (245, 245, 245)   # free space
SEC_STACK    = (255, 220, 220)   # stack
SEC_GUARD    = (255, 180, 180)   # guard

# Font helpers
def _try_font(paths, size):
    for p in paths:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()

def sans(size):
    return _try_font([
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ], size)

def sans_b(size):
    return _try_font([
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ], size)

def mono(size):
    return _try_font([
        "/System/Library/Fonts/Menlo.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    ], size)

F_TITLE    = sans_b(22)
F_SUBTITLE = sans(14)
F_LABEL_B  = sans_b(13)
F_LABEL    = sans(12)
F_MONO     = mono(12)
F_MONO_S   = mono(11)
F_SMALL    = sans(11)
F_TINY     = sans(10)

# ============================================================
# Helpers
# ============================================================
def text_w(draw, txt, font):
    bb = draw.textbbox((0, 0), txt, font=font)
    return bb[2] - bb[0]

def text_h(draw, txt, font):
    bb = draw.textbbox((0, 0), txt, font=font)
    return bb[3] - bb[1]

def center_text(draw, x, y, w, h, txt, font, colour=TEXT):
    tw = text_w(draw, txt, font)
    th = text_h(draw, txt, font)
    draw.text((x + (w - tw) // 2, y + (h - th) // 2 - 1), txt, fill=colour, font=font)

# ============================================================
# Memory regions
# ============================================================
# Each region: (address, label, sublabel, height, colour)
ADDR_COL_W = 95
BLOCK_X = ADDR_COL_W + 10
BLOCK_W = 380

# Heights proportional to actual sizes (but compressed for readability)
ROM_H = 50
KERNEL_H = 50
SLOT_H = 32      # per-slot height
FREE_H = 20      # unused gap
BOOT_BSS_H = 30
STACK_PTR_H = 16
MMIO_H = 40
CONSOLE_H = 30

regions = [
    (0x00000000, "ROM",                  "Bootloader (read-only)", ROM_H,      ROM_COL),
    (0x00100000, "Kernel",               "~64 KB code/data/BSS",   KERNEL_H,   KERNEL_COL),
]

# 8 process slots at 32KB each
for i in range(8):
    addr = 0x00110000 + i * 0x8000
    if i == 0:
        sub = "32 KB — Shell (/bin/sh)"
    else:
        sub = "32 KB"
    col = SLOT_COL if i % 2 == 0 else SLOT_ALT
    regions.append((addr, f"Process Slot {i}", sub, SLOT_H, col))

# After slot 7 ends at 0x00150000
regions.append((0x00150000, "(unused)", "",                     FREE_H,      FREE_COL))
regions.append((0x001F0000, "Bootloader BSS", "60 KB (boot only)", BOOT_BSS_H, BOOT_BSS_COL))
regions.append((0x001FFFFC, "Stack Pointer",  "Initial SP",      STACK_PTR_H, FREE_COL))
regions.append((0x00200000, "Block Device MMIO", "Disk I/O",     MMIO_H,      MMIO_COL))
regions.append((0xFFFF0004, "Console MMIO",   "Serial I/O",      CONSOLE_H,   MMIO_COL))

# Compute total height
total_region_h = sum(r[3] for r in regions)
GAP = 2  # gap between regions
total_gaps = (len(regions) - 1) * GAP

# Slot detail panel
DETAIL_X = BLOCK_X + BLOCK_W + 30
DETAIL_W = 180
DETAIL_H = 8 * SLOT_H + 7 * GAP  # same height as all slots

H = MARGIN_T + 50 + total_region_h + total_gaps + 40 + MARGIN_B

# ============================================================
# Draw
# ============================================================
img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# Title
y = MARGIN_T
title = "logOS"
tw = text_w(d, title, F_TITLE)
d.text(((W - tw) // 2, y), title, fill=TEXT, font=F_TITLE)
y += 28

subtitle = "Physical Memory Layout"
sw = text_w(d, subtitle, F_SUBTITLE)
d.text(((W - sw) // 2, y), subtitle, fill=(46, 134, 171), font=F_SUBTITLE)
y += 24

# Track slot positions for detail panel lines
slot_positions = []

for i, (addr, label, sublabel, h, col) in enumerate(regions):
    # Address label
    addr_str = f"0x{addr:08X}"
    d.text((MARGIN_L, y + (h - 12) // 2), addr_str, fill=GREY, font=F_MONO_S)

    # Block
    d.rectangle([BLOCK_X, y, BLOCK_X + BLOCK_W, y + h], fill=col, outline=BORDER, width=1)

    # Label
    if sublabel:
        lw = text_w(d, label, F_LABEL_B)
        sw2 = text_w(d, sublabel, F_SMALL)
        if h >= 40:
            d.text((BLOCK_X + (BLOCK_W - lw) // 2, y + h // 2 - 14), label, fill=TEXT, font=F_LABEL_B)
            d.text((BLOCK_X + (BLOCK_W - sw2) // 2, y + h // 2 + 2), sublabel, fill=GREY, font=F_SMALL)
        else:
            d.text((BLOCK_X + (BLOCK_W - lw) // 2, y + (h - 12) // 2 - 1), label, fill=TEXT, font=F_LABEL_B)
    else:
        center_text(d, BLOCK_X, y, BLOCK_W, h, label, F_LABEL_B)

    # Track slot positions
    if label.startswith("Process Slot"):
        slot_positions.append((y, h))

    y += h + GAP

# ============================================================
# Slot detail panel (right side)
# ============================================================
if slot_positions:
    # Position detail panel aligned with slots
    det_y_start = slot_positions[0][0]
    det_y_end = slot_positions[-1][0] + slot_positions[-1][1]
    det_total = det_y_end - det_y_start

    # Draw connecting line from slots to detail
    mid_slot_y = det_y_start + det_total // 2
    d.line([BLOCK_X + BLOCK_W, mid_slot_y, DETAIL_X, mid_slot_y],
           fill=LIGHT_GREY, width=1)

    # Detail panel title
    dtitle = "32 KB Slot Layout"
    dtw = text_w(d, dtitle, F_LABEL_B)
    d.text((DETAIL_X + (DETAIL_W - dtw) // 2, det_y_start - 18),
           dtitle, fill=(46, 134, 171), font=F_LABEL_B)

    # Slot sections (proportional within the panel)
    detail_sections = [
        (".text",    "Program code",        SEC_TEXT,   0.20),
        (".rodata",  "Read-only data",      SEC_RODATA, 0.10),
        (".data",    "Initialized data",    SEC_DATA,   0.08),
        (".bss",     "Zeroed globals",      SEC_BSS,    0.08),
        ("(free)",   "",                    SEC_FREE,   0.24),
        ("Stack",    "Grows downward",      SEC_STACK,  0.22),
        ("Guard",    "0x100 bytes",         SEC_GUARD,  0.08),
    ]

    dy = det_y_start
    for sname, sdesc, scol, frac in detail_sections:
        sh = int(det_total * frac)
        d.rectangle([DETAIL_X, dy, DETAIL_X + DETAIL_W, dy + sh],
                    fill=scol, outline=BORDER, width=1)

        nw = text_w(d, sname, F_LABEL_B)
        if sdesc and sh >= 28:
            d.text((DETAIL_X + (DETAIL_W - nw) // 2, dy + sh // 2 - 12),
                   sname, fill=TEXT, font=F_LABEL_B)
            dw = text_w(d, sdesc, F_TINY)
            d.text((DETAIL_X + (DETAIL_W - dw) // 2, dy + sh // 2 + 2),
                   sdesc, fill=GREY, font=F_TINY)
        else:
            center_text(d, DETAIL_X, dy, DETAIL_W, sh, sname, F_LABEL_B)

        dy += sh

    # Labels for base and top
    d.text((DETAIL_X + DETAIL_W + 5, det_y_start - 2), "base", fill=GREY, font=F_MONO_S)
    d.text((DETAIL_X + DETAIL_W + 5, dy - 14), "base +", fill=GREY, font=F_MONO_S)
    d.text((DETAIL_X + DETAIL_W + 5, dy - 2), "0x7F00", fill=GREY, font=F_MONO_S)

# ============================================================
# Legend
# ============================================================
legend_y = y + 10
legend_items = [
    ("ROM", ROM_COL),
    ("Kernel", KERNEL_COL),
    ("Process Slots", SLOT_COL),
    ("Memory-Mapped I/O", MMIO_COL),
]

lx = MARGIN_L + 40
for lbl, col in legend_items:
    d.rectangle([lx, legend_y, lx + 14, legend_y + 14], fill=col, outline=BORDER, width=1)
    d.text((lx + 20, legend_y), lbl, fill=TEXT, font=F_SMALL)
    lx += text_w(d, lbl, F_SMALL) + 50

# Footer
footer = "logOS 2.0 Blackrock  —  RISC-V RV32IM / Logisim Evolution"
fw = text_w(d, footer, F_SMALL)
d.text(((W - fw) // 2, H - MARGIN_B + 10), footer, fill=LIGHT_GREY, font=F_SMALL)

# ============================================================
# Save
# ============================================================
img.save(OUT_PATH, dpi=(150, 150))
print(f"Saved {OUT_PATH} ({img.size[0]}x{img.size[1]})")
