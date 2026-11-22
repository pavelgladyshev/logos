#!/usr/bin/env python3
# Writes a copy of a Logisim circuit with a program in its ROM and, if asked, the RISC-V CPU's
# debugger and CSR panel switched on. The projects' Makefiles call it (make run, gui, debug).
# By Pavel Gladyshev, licensed under Creative Commons Attribution International license 4.0
#
#   mkcircuit.py IN.circ OUT.circ [--rom IMAGE.txt] [--debug PORT]
#                [--csr-panel none|machine|supervisor]
#
#   --rom        the ROM labelled ROM_1 (or, if no ROM has that label, the circuit's only ROM)
#                gets the image: a Logisim "v2.0 raw" file of 32-bit words in hexadecimal,
#                as the Makefiles write it
#   --debug      the RV32IM CPU starts a GDB server on that TCP port (Debug Enable)
#   --csr-panel  the RV32IM CPU shows that panel of CSRs below its registers
#
# The rest of the file is copied as it is. Standard library only.

import argparse
import re
import sys

ROM_LABEL = "ROM_1"
CPU_NAME = "RV32IM CPU"
ROM_WORDS = 1 << 14          # the ROM's address width is 14 bits

# An opening <comp ...> tag (not a self-closing one) of a component with the given name
def comp_open_tags(text, name):
    pattern = r'<comp\b[^>]*?\bname="%s"[^>]*?(?<!/)>' % re.escape(name)
    return list(re.finditer(pattern, text))

def comp_close(text, start):
    end = text.find("</comp>", start)
    return end

def die(message):
    sys.exit("mkcircuit.py: " + message)

def read_image(path):
    try:
        with open(path) as f:
            lines = f.read().split("\n")
    except OSError as e:
        die("cannot read the ROM image: %s" % e)
    if not lines or lines[0].strip() != "v2.0 raw":
        die("%s is not a Logisim image: its first line is not \"v2.0 raw\"" % path)
    words = []
    for number, line in enumerate(lines[1:], 2):
        for token in line.split("#")[0].split():
            m = re.fullmatch(r"(?:(\d+)\*)?([0-9a-fA-F]{1,8})", token)
            if not m:
                die("%s, line %d: %r is not a 32-bit hexadecimal word" % (path, number, token))
            words.extend([int(m.group(2), 16)] * int(m.group(1) or 1))
    return words

def rom_contents(words, newline):
    while words and words[-1] == 0:
        words = words[:-1]
    rows = ["addr/data: 14 32"]
    for i in range(0, len(words), 8):
        rows.append(" ".join("%x" % w for w in words[i:i + 8]))
    return newline.join(rows) + newline

def set_rom(text, words, newline):
    if len(words) > ROM_WORDS:
        die("the image has %d words (%d bytes) but the ROM holds %d words (%d bytes): "
            "the program is too big for it" % (len(words), 4 * len(words), ROM_WORDS, 4 * ROM_WORDS))
    tag = find_rom(text)
    close = comp_close(text, tag.end())
    if close < 0:
        die("a <comp> element is not closed")
    body = text[tag.end():close]
    contents = rom_contents(words, newline)
    m = re.search(r'(<a\s+name="contents">)(.*?)(</a>)', body, re.S)
    if m:
        body = body[:m.start()] + m.group(1) + contents + m.group(3) + body[m.end():]
    else:
        indent = line_indent(body)
        body = newline + indent + '<a name="contents">' + contents + "</a>" + body
    return text[:tag.end()] + body + text[close:]

# The ROM to load: the one labelled ROM_1 or, in a circuit that has no such label, its only ROM
def find_rom(text, where="the circuit"):
    roms = comp_open_tags(text, "ROM")
    for tag in roms:
        close = comp_close(text, tag.end())
        if close >= 0 and re.search(r'<a\s+name="label"\s+val="%s"\s*/>' % ROM_LABEL, text[tag.end():close]):
            return tag
    if len(roms) == 1:
        return roms[0]
    if not roms:
        die("there is no ROM in %s" % where)
    die("there are %d ROMs in %s and none is labelled %s" % (len(roms), where, ROM_LABEL))

def line_indent(body):
    m = re.search(r"\n([ \t]*)<a\b", body)
    return m.group(1) if m else "      "

def set_attributes(body, attributes, newline, indent):
    for name, value in attributes:
        pattern = r'<a\s+name="%s"\s+val="[^"]*"(\s*/>)' % name
        new = '<a name="%s" val="%s"' % (name, value)
        m = re.search(pattern, body)
        if m:
            body = body[:m.start()] + new + m.group(1) + body[m.end():]
        else:
            body = newline + indent + new + "/>" + body
    return body

def set_cpu(text, attributes, newline):
    tags = comp_open_tags(text, CPU_NAME)
    if tags:
        tag = tags[0]
        close = comp_close(text, tag.end())
        if close < 0:
            die("a <comp> element is not closed")
        body = text[tag.end():close]
        body = set_attributes(body, attributes, newline, line_indent(body))
        return text[:tag.end()] + body + text[close:]
    # a CPU without any attribute may be written as a self-closing element
    m = re.search(r'<comp\b[^>]*?\bname="%s"[^>]*?/>' % re.escape(CPU_NAME), text)
    if not m:
        die("there is no %s in the circuit" % CPU_NAME)
    line_start = text.rfind("\n", 0, m.start()) + 1
    outer = re.match(r"[ \t]*", text[line_start:]).group(0)
    inner = outer + "  "
    opening = m.group(0)[:-2].rstrip() + ">"
    body = set_attributes("", attributes, newline, inner)
    return text[:m.start()] + opening + body + newline + outer + "</comp>" + text[m.end():]

def main():
    ap = argparse.ArgumentParser(description="Copy a circuit with a program in its ROM and the CPU's debugger switched on.")
    ap.add_argument("circuit_in")
    ap.add_argument("circuit_out")
    ap.add_argument("--rom", metavar="IMAGE.txt", help="Logisim image for the ROM labelled %s" % ROM_LABEL)
    ap.add_argument("--debug", metavar="PORT", type=int, help="start the CPU's GDB server on this TCP port")
    ap.add_argument("--csr-panel", choices=["none", "machine", "supervisor"], help="the CPU's panel of CSRs")
    args = ap.parse_args()
    if args.debug is not None and not 0 <= args.debug <= 65535:
        die("%d is not a TCP port" % args.debug)

    try:
        with open(args.circuit_in, newline="") as f:
            text = f.read()
    except OSError as e:
        die("cannot read the circuit: %s" % e)
    newline = "\r\n" if "\r\n" in text else "\n"

    # Check before changing anything, so that the errors do not depend on the options
    find_rom(text, args.circuit_in)
    if not re.search(r'<comp\b[^>]*?\bname="%s"' % re.escape(CPU_NAME), text):
        die("there is no %s in %s" % (CPU_NAME, args.circuit_in))

    if args.rom:
        text = set_rom(text, read_image(args.rom), newline)
    attributes = []
    if args.debug is not None:
        attributes += [("gdbServerRunning", "true"), ("tcpPort", str(args.debug))]
    if args.csr_panel:
        attributes += [("csrPanel", args.csr_panel)]
    if attributes:
        text = set_cpu(text, attributes, newline)

    try:
        with open(args.circuit_out, "w", newline="") as f:
            f.write(text)
    except OSError as e:
        die("cannot write the circuit: %s" % e)

main()
