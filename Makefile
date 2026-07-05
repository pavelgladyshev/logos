BUILD_ESP32C3 = build/esp32c3

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
	idf.py -B $(BUILD_ESP32C3) build
	
esp-flash: esp
	idf.py -p $(ESP_PORT) -B $(BUILD_ESP32C3) flash monitor

esp-clean: 
	idf.py -B $(BUILD_ESP32C3) clean

esp-clean-all:
	idf.py -B $(BUILD_ESP32C3) fullclean

#wasn't sure if all should include flashing as well
all: esp logisim-all

clean-all: esp-clean-all logisim-all
