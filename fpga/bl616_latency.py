# bl616_latency.py MS [INDEX]: an FTDI SET_LATENCY_TIMER request to the Tang Nano 20K's USB debugger, a BL616 that
# looks like an FT2232 (0403:6010): INDEX 1 is interface A (JTAG), 2 interface B (the serial port). Through usbfs,
# without libusb. The BL616 takes interface A's timer for its serial port too, and OpenOCD sets it to 255 ms, which
# holds back what the board sends that does not fill a USB packet (a console's echo) by up to 255 ms; with 1 ms the
# console is as quick as without OpenOCD. tangnano20k.cfg runs it.
import ctypes, fcntl, glob, os, sys
ms, index = int(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 1
for d in glob.glob("/sys/bus/usb/devices/*"):
    try:
        if open(d + "/idVendor").read().strip() == "0403" and open(d + "/idProduct").read().strip() == "6010":
            node = "/dev/bus/usb/%03d/%03d" % (int(open(d + "/busnum").read()), int(open(d + "/devnum").read()))
            break
    except OSError:
        pass
else:
    sys.exit("no 0403:6010 device")
class Ctrl(ctypes.Structure):
    _fields_ = [("bRequestType", ctypes.c_uint8), ("bRequest", ctypes.c_uint8), ("wValue", ctypes.c_uint16),
                ("wIndex", ctypes.c_uint16), ("wLength", ctypes.c_uint16), ("timeout", ctypes.c_uint32),
                ("data", ctypes.c_void_p)]
USBDEVFS_CONTROL = (3 << 30) | (ctypes.sizeof(Ctrl) << 16) | (ord('U') << 8) | 0
fd = os.open(node, os.O_RDWR)
fcntl.ioctl(fd, USBDEVFS_CONTROL, Ctrl(0x40, 0x09, ms, index, 0, 1000, None))
os.close(fd)
print(f"latency timer of interface {'AB'[index - 1]} set to {ms} ms on {node}")
