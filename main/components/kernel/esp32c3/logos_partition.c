#include "logos_partition.h"


#include "console.h"
// #include "esp_err.h"
#include "esp_flash.h"
#include "esp_private/esp_flash_internal.h"

/* Validate relative accesses before adding the physical flash base address. */
static int check_range(uint32_t offset, uint32_t len)
{
    if (offset > LOGOS_PARTITION_SIZE) {
        return -1;
    }

    if (len > LOGOS_PARTITION_SIZE - offset) {
        return -1;
    }

    return 0;
}

int logos_partition_init(void)
{
    int err;

    /*
     * Normal ESP-IDF reaches this through the registered init_flash function.
     * The custom kernel path skips that init walker, so initialize the default
     * flash chip on demand before read/write/erase operations.
     */
    if (esp_flash_default_chip != NULL &&
        esp_flash_chip_driver_initialized(esp_flash_default_chip)) {
        return 0;
    }

    err = esp_flash_init_default_chip();
    if (err != ESP_OK) {
        logos_printf("logos_partition: esp_flash_init_default_chip failed: 0x%08lx\n",
                              (unsigned long)err);
        return -1;
    }

    return 0;
}

int logos_partition_read(uint32_t offset, void *buf, uint32_t len)
{
    if (buf == NULL || check_range(offset, len) != 0) {
        return -1;
    }

    /* NULL selects esp_flash_default_chip after logos_partition_init(). */
    return esp_flash_read(NULL, buf, LOGOS_PARTITION_OFFSET + offset, len) == ESP_OK
        ? 0
        : -1;
}

int logos_partition_write(uint32_t offset, const void *buf, uint32_t len)
{
    if (buf == NULL || check_range(offset, len) != 0) {
        return -1;
    }

    /* Writes require the caller to erase the containing sector first. */
    return esp_flash_write(NULL, buf, LOGOS_PARTITION_OFFSET + offset, len) == ESP_OK
        ? 0
        : -1;
}

int logos_partition_erase(uint32_t offset, uint32_t len)
{
    if (check_range(offset, len) != 0) {
        return -1;
    }

    /* ESP flash erase operations must be sector aligned. */
    if ((offset % LOGOS_PARTITION_SECTOR_SIZE) != 0 ||
        (len % LOGOS_PARTITION_SECTOR_SIZE) != 0) {
        return -1;
    }

    return esp_flash_erase_region(NULL, LOGOS_PARTITION_OFFSET + offset, len) == ESP_OK
        ? 0
        : -1;
}

uint32_t logos_partition_size(void)
{
    return LOGOS_PARTITION_SIZE;
}
