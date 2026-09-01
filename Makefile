BUILD_ESP32C3 = build

LOGISIM_DIR = main/components

ESP_PORT = /dev/ttyUSB0

logisim:
	$(MAKE) -C $(LOGISIM_DIR) 

fs-image:
	$(MAKE) -C $(LOGISIM_DIR) fs-image

logisim-all:
	$(MAKE) -C $(LOGISIM_DIR) all

logisim-clean:
	$(MAKE) -C $(LOGISIM_DIR) clean 

logisim-clean-all:
	$(MAKE) -C $(LOGISIM_DIR) clean-all

esp:
	./scripts/idf.sh -B $(BUILD_ESP32C3) build

emulator-check:
	./scripts/emulator-check.sh

qemu: emulator-check
	./scripts/qemu.sh

qemu-gdb: emulator-check
	./scripts/qemu.sh --gdb
	
esp-flash: esp
	./scripts/idf.sh -p $(ESP_PORT) -B $(BUILD_ESP32C3) flash monitor

esp-clean: 
	./scripts/idf.sh -B $(BUILD_ESP32C3) clean

esp-clean-all:
	./scripts/idf.sh -B $(BUILD_ESP32C3) fullclean

#wasn't sure if all should include flashing as well
all: esp logisim-all

clean-all: esp-clean-all logisim-all
