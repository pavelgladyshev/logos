# logOS

logOS is a small RISC-V operating system targeting the ESP32-C3. This checkout
includes ESP-IDF beside the project and supports both physical hardware and the
Espressif ESP32-C3 QEMU machine.

## Run in the ESP32-C3 emulator

From this directory:

```sh
make emulator-check
make qemu
```

The launcher activates the adjacent `../esp-idf` checkout automatically. It
builds logOS, creates `build/qemu_flash.bin` and `build/qemu_efuse.bin`, then
starts QEMU with the serial console attached to the terminal. Exit QEMU with
<kbd>Ctrl</kbd>+<kbd>A</kbd>, then <kbd>X</kbd>.

For source-level debugging, run:

```sh
make qemu-gdb
```

Then use `./scripts/idf.sh -B build gdb` in another terminal.

## Install a missing emulator dependency

If `make emulator-check` reports that the ESP32-C3 QEMU machine is unavailable,
install the version selected by this ESP-IDF checkout:

```sh
python3 ../esp-idf/tools/idf_tools.py install qemu-riscv32
```

Do not rely on a standard Homebrew QEMU build: generic RISC-V system emulation
does not include Espressif's `esp32c3` machine and peripheral models.

## Other useful commands

```sh
make esp          # Build firmware for ESP32-C3
make esp-flash    # Build, flash /dev/ttyUSB0, and attach a monitor
make esp-clean    # Remove normal build outputs
```

Override `ESP_PORT` when using a different serial device, for example:

```sh
make esp-flash ESP_PORT=/dev/cu.usbmodem1101
```
