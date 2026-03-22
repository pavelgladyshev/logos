#!/usr/bin/env python3
"""
Draw a PNG diagram of the logOS filesystem disk layout.
"""

from PIL import Image, ImageDraw, ImageFont
import os

OUT_PATH = os.path.join(os.path.dirname(__file__), "logos-fs-layout.png")

# ============================================================
# Configuration
# ============================================================
W = 860
MARGIN_L = 30
MARGIN_R = 30
MARGIN_T = 40
MARGIN_B = 30

# Colours
BG           = (255, 255, 255)
BORDER       = (60, 60, 60)
TEXT         = (30, 30, 30)
GREY         = (110, 110, 110)
RULE         = (200, 200, 200)
SUPERBLOCK   = (190, 215, 255)
BITMAP       = (255, 210, 185)
INODE        = (190, 240, 190)
DATA         = (255, 250, 195)
DETAIL_ALT   = (245, 245, 245)

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

F_TITLE  = sans_b(19)
F_HEAD   = sans_b(14)
F_LABEL  = sans_b(13)
F_SMALL  = sans(12)
F_MONO   = mono(12)
F_MONO_S = mono(11)

# ============================================================
# Helpers
# ============================================================
def text_w(draw, txt, font):
    bb = draw.textbbox((0, 0), txt, font=font)
    return bb[2] - bb[0]

def draw_block_rect(draw, x, y, w, h, fill, label, sublabel=None):
    """Draw a block rectangle with centred label and optional sublabel."""
    draw.rectangle([x, y, x + w, y + h], fill=fill, outline=BORDER, width=2)
    lw = text_w(draw, label, F_LABEL)
    if sublabel:
        sw = text_w(draw, sublabel, F_SMALL)
        draw.text((x + (w - lw) // 2, y + 8), label, fill=TEXT, font=F_LABEL)
        draw.text((x + (w - sw) // 2, y + 26), sublabel, fill=GREY, font=F_SMALL)
    else:
        draw.text((x + (w - lw) // 2, y + (h - 14) // 2), label, fill=TEXT, font=F_LABEL)

# ============================================================
# Compute layout
# ============================================================
BAR_W = W - MARGIN_L - MARGIN_R
BAR_H = 50

# Give metadata blocks readable widths, data fills the rest
SB_W  = 100   # superblock
BM_W  = 80    # bitmap
IN_W  = 160   # inode table (8 blocks)
DA_W  = BAR_W - SB_W - BM_W - IN_W  # data blocks

# Detail sections
DETAIL_ROW_H = 20
DETAIL_HDR_H = 26
DETAIL_GAP   = 14

detail_sections = [
    {
        "title": "Superblock (Block 0 \u2014 512 bytes)",
        "colour": SUPERBLOCK,
        "fields": [
            ("magic",         "uint32",    '0x53465346 ("FSFS")'),
            ("total_blocks",  "uint32",    "512"),
            ("total_inodes",  "uint32",    "32"),
            ("free_blocks",   "uint32",    "count of free data blocks"),
            ("free_inodes",   "uint32",    "count of free inodes"),
            ("bitmap_start",  "uint32",    "1"),
            ("bitmap_blocks", "uint32",    "1"),
            ("inode_start",   "uint32",    "2"),
            ("inode_blocks",  "uint32",    "8"),
            ("data_start",    "uint32",    "10"),
            ("reserved",      "byte[472]", "padding to fill 512-byte block"),
        ],
    },
    {
        "title": "Inode (128 bytes \u2014 4 per block, 32 total)",
        "colour": INODE,
        "fields": [
            ("size",        "uint32",     "file size in bytes"),
            ("type",        "uint8",      "0=free  1=file  2=dir  3=chardev"),
            ("major",       "uint8",      "device major number"),
            ("minor",       "uint8",      "device minor number"),
            ("link_count",  "uint8",      "number of hard links"),
            ("blocks[60]",  "uint16[60]", "60 direct block pointers (max 30 KB)"),
        ],
    },
    {
        "title": "Directory Entry (32 bytes \u2014 16 per block)",
        "colour": DATA,
        "fields": [
            ("inode", "uint32",   "inode number (0xFFFFFFFF = free)"),
            ("name",  "char[28]", "filename, null-terminated"),
        ],
    },
]

det_h = 0
for sec in detail_sections:
    det_h += DETAIL_HDR_H + len(sec["fields"]) * DETAIL_ROW_H + DETAIL_GAP

H = MARGIN_T + 28 + 14 + BAR_H + 20 + 18 + 22 + det_h + MARGIN_B

# ============================================================
# Draw image
# ============================================================
img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

y = MARGIN_T

# --- Title ---
title = "logOS Filesystem Disk Layout  (512 \u00d7 512 = 256 KB)"
tw = text_w(d, title, F_TITLE)
d.text(((W - tw) // 2, y), title, fill=TEXT, font=F_TITLE)
y += 28 + 14

# --- Block bar ---
bar_x = MARGIN_L
bar_y = y

draw_block_rect(d, bar_x,                     bar_y, SB_W, BAR_H, SUPERBLOCK, "Superblock", "Block 0")
draw_block_rect(d, bar_x + SB_W,              bar_y, BM_W, BAR_H, BITMAP,     "Bitmap",     "Block 1")
draw_block_rect(d, bar_x + SB_W + BM_W,       bar_y, IN_W, BAR_H, INODE,      "Inode Table","Blocks 2\u20139")
draw_block_rect(d, bar_x + SB_W + BM_W + IN_W,bar_y, DA_W, BAR_H, DATA,       "Data Blocks","Blocks 10\u2013511")

# --- Block numbers below the bar ---
num_y = bar_y + BAR_H + 4
d.text((bar_x + 1, num_y), "0", fill=GREY, font=F_MONO_S)
d.text((bar_x + SB_W + 1, num_y), "1", fill=GREY, font=F_MONO_S)
d.text((bar_x + SB_W + BM_W + 1, num_y), "2", fill=GREY, font=F_MONO_S)
d.text((bar_x + SB_W + BM_W + IN_W + 1, num_y), "10", fill=GREY, font=F_MONO_S)
t255 = "511"
d.text((bar_x + BAR_W - text_w(d, t255, F_MONO_S) - 2, num_y), t255, fill=GREY, font=F_MONO_S)

# --- Sizes below block numbers ---
sz_y = num_y + 16
def sz_center(offset, width, txt):
    cx = bar_x + offset + width // 2
    tw = text_w(d, txt, F_MONO_S)
    d.text((cx - tw // 2, sz_y), txt, fill=GREY, font=F_MONO_S)

sz_center(0,                     SB_W, "512 B")
sz_center(SB_W,                  BM_W, "512 B")
sz_center(SB_W + BM_W,          IN_W, "4 KB")
sz_center(SB_W + BM_W + IN_W,   DA_W, "251 KB")

y = sz_y + 18 + 8

# --- Detail sections ---
det_x = MARGIN_L
det_w = BAR_W
COL1 = 125   # field name column
COL2 = 95    # type column

for sec in detail_sections:
    # Header
    d.rectangle([det_x, y, det_x + det_w, y + DETAIL_HDR_H],
                fill=sec["colour"], outline=BORDER, width=1)
    d.text((det_x + 10, y + 5), sec["title"], fill=TEXT, font=F_HEAD)
    y += DETAIL_HDR_H

    # Rows
    for i, (fname, ftype, fdesc) in enumerate(sec["fields"]):
        bg = DETAIL_ALT if i % 2 == 0 else BG
        d.rectangle([det_x, y, det_x + det_w, y + DETAIL_ROW_H], fill=bg)
        d.line([det_x, y + DETAIL_ROW_H - 1, det_x + det_w, y + DETAIL_ROW_H - 1], fill=RULE)

        d.text((det_x + 12, y + 3), fname, fill=TEXT, font=F_MONO)
        d.text((det_x + COL1 + 10, y + 3), ftype, fill=GREY, font=F_MONO_S)
        d.text((det_x + COL1 + COL2 + 20, y + 3), fdesc, fill=TEXT, font=F_SMALL)
        y += DETAIL_ROW_H

    # Bottom border
    d.line([det_x, y, det_x + det_w, y], fill=BORDER, width=1)
    y += DETAIL_GAP

# ============================================================
# Save
# ============================================================
img.save(OUT_PATH, dpi=(150, 150))
print(f"Saved {OUT_PATH} ({img.size[0]}x{img.size[1]})")
