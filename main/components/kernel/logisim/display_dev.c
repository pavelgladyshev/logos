#include "device.h"
#include "display_dev.h"

static int display_write(uint8_t minor, const void *buf, uint32_t len) {
    const uint32_t *src = (const uint32_t *)buf;
    uint32_t i;

    for (i = 0; i < len; i++) {
        SCREEN_DATA = src[i];
    }

    return len;
}


static struct device_ops display_ops = {
    .open = 0,
    .close = 0,
    .read = 0,
    .write = display_write
};

int display_dev_init(void){
    return device_register(DISPLAY_MAJOR, &display_ops);
}
