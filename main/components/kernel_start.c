#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "bootloader_flash.h"
#include "bootloader_flash_config.h"
#include "bootloader_mem.h"
#include "esp_app_format.h"
#include "esp_clk_internal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_private/cache_err_int.h"
#include "esp_private/cache_utils.h"
#include "esp_private/esp_clk.h"
#include "esp_private/esp_clk_tree_common.h"
#include "esp_private/esp_mmu_map_private.h"
#include "esp_private/esp_system_attr.h"
#include "esp_private/mspi_timing_tuning.h"
#include "esp_private/rtc_clk.h"
#include "esp_private/sleep_gpio.h"
#include "esp_private/spi_flash_os.h"
#include "esp_private/startup_internal.h"
#include "esp_private/system_internal.h"
#include "esp_rom_serial_output.h"
#include "esp_rom_sys.h"
#include "esp_rtc_time.h"
#include "hal/cache_hal.h"
#include "hal/cpu_utility_ll.h"
#include "hal/efuse_ll.h"
#include "hal/mmu_hal.h"
#include "hal/uart_ll.h"
#include "hal/wdt_hal.h"
#include "rom/ets_sys.h"
#include "rom/rtc.h"
#include "soc/ext_mem_defs.h"
#include "soc/periph_defs.h"
#include "soc/rtc.h"
#include "soc/soc_caps.h"
#include "soc/uart_pins.h"

#if CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/cache.h"
#include "esp32c3/rom/secure_boot.h"
#include "esp_memprot.h"
#endif

#if CONFIG_SPIRAM
#include "esp_private/esp_psram_extram.h"
#include "esp_psram.h"
#endif

#if CONFIG_APP_BUILD_TYPE_RAM
#include "bootloader_init.h"
#include "esp_private/bootloader_flash_internal.h"
#include "esp_rom_spiflash.h"
#include "spi_flash_mmap.h"
#endif

extern int _bss_start;
extern int _bss_end;
extern int _rtc_bss_start;
extern int _rtc_bss_end;
extern char _rodata_reserved_start;
extern int _vector_table;

ESP_LOG_ATTR_TAG(TAG, "kernel_start");

static void core_intr_matrix_clear(void);
FORCE_INLINE_ATTR IRAM_ATTR void init_cpu(void);
FORCE_INLINE_ATTR IRAM_ATTR void get_reset_reason(soc_reset_reason_t *rst_reas);
FORCE_INLINE_ATTR IRAM_ATTR void init_bss(const soc_reset_reason_t *rst_reas);

#if CONFIG_LIBC_PICOLIBC
FORCE_INLINE_ATTR IRAM_ATTR void init_pre_rtos_tls_area(int cpu_num);
#endif

#if CONFIG_APP_BUILD_TYPE_RAM
FORCE_INLINE_ATTR IRAM_ATTR void ram_app_init(void);
#endif

#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
FORCE_INLINE_ATTR IRAM_ATTR void ext_mem_init(void)
{
    /*
     * Initialize cache HAL context.
     *
     * This is not a Unix-style MMU. On ESP32-C3, this supports cached access
     * to external SPI flash mappings.
     */
    cache_hal_config_t config = {
        .core_nums = 1,
    };
    cache_hal_init(&config);

    /*
     * Initialize MMU HAL context.
     *
     * Again: this is ESP flash/cache address translation, not process virtual
     * memory.
     */
    mmu_hal_config_t mmu_config = {
        .core_nums = 1,
    };
    mmu_hal_ctx_init(&mmu_config);
}
#endif 

// On chips with different virtual address space for flash and PSRAM,
// code in flash is not available before XIP is initialized.
// Hence, these functions have to be in IRAM.
#if SOC_MMU_PER_EXT_MEM_TARGET
#define MSPI_INIT_ATTR FORCE_INLINE_ATTR IRAM_ATTR
#else
#define MSPI_INIT_ATTR NOINLINE_ATTR static
#endif

MSPI_INIT_ATTR void sys_rtc_init(const soc_reset_reason_t *rst_reas)
{
#if SOC_RTC_WDT_SUPPORTED
#ifndef CONFIG_BOOTLOADER_WDT_ENABLE
    // from panic handler we can be reset by RWDT or TG0WDT
    if (rst_reas[0] == RESET_REASON_CORE_RTC_WDT ||
        rst_reas[0] == RESET_REASON_CORE_MWDT0) {
        wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_disable(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    }
#endif
#endif

    // Configure the power related stuff. After this the MSPI timing tuning can be done.
    esp_rtc_init();
}

#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
static NOINLINE_ATTR IRAM_ATTR void flash_init_state(void)
{
    /**
     * This function initialise the Flash chip to the user-defined settings.
     *
     * In bootloader, we only init Flash (and MSPI) to a preliminary state, for being flexible to
     * different chips.
     * In this stage, we re-configure the Flash (and MSPI) to required configuration
     */
    spi_flash_init_chip_state();

    // This function needs to be called when PLL is enabled. It must run after
    // spi_flash_init_chip_state in case some flash state is modified.
    mspi_timing_flash_tuning();
}

MSPI_INIT_ATTR void mspi_init(void)
{
    /*
     * Configure pins and hardware used for external SPI flash access.
     */
    esp_mspi_pin_init();

    /*
     * Read/update the flash chip ID known to the bootloader support layer.
     */
    bootloader_flash_update_id();

    flash_init_state();

    /*
     * Required by image_process() and flash_mmap APIs.
     */
    esp_mmu_map_init();

#if CONFIG_SPIRAM_BOOT_HW_INIT
    if (esp_psram_chip_init() != ESP_OK) {
#if CONFIG_SPIRAM_IGNORE_NOTFOUND
        ESP_DRAM_LOGW(TAG, "Failed to init external RAM; continuing without it.");
#else
        ESP_DRAM_LOGE(TAG, "Failed to init external RAM!");
        abort();
#endif
    }
#endif

#if CONFIG_SPIRAM_BOOT_INIT
    if (esp_psram_init() != ESP_OK) {
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
        ESP_DRAM_LOGE(TAG, "Failed to init external RAM, needed for external .bss segment");
        abort();
#endif
    }
#endif
}
#endif

/*
 * Initialize other parts of the system.
 *
 * ESP32-C3 is single-core, so multicore startup paths are removed.
 */
NOINLINE_ATTR static void system_early_init(const soc_reset_reason_t *rst_reas)
{
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    esp_mspi_pin_reserve();
#endif

    ESP_EARLY_LOGI(TAG, "Unicore app");

    /* NOTE: When ESP-TEE is enabled, it configures its own memory protection
     * scheme using the CPU-inherent features PMP and PMA and the APM peripheral.
     */
#if !CONFIG_SECURE_ENABLE_TEE
    bootloader_init_mem();
#endif

#if CONFIG_SPIRAM_MEMTEST
    if (esp_psram_is_initialized()) {
        bool ext_ram_ok = esp_psram_extram_test();
        if (!ext_ram_ok) {
            ESP_EARLY_LOGE(TAG, "External RAM failed memory test!");
            abort();
        }
    }
#endif

#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
    esp_psram_bss_init();
#endif

    esp_clk_tree_initialize();
    esp_clk_init();
    esp_perip_clk_init();

    // Now that the clocks have been set-up, set the startup time from RTC
    // and default RTC-backed system time provider.
    g_startup_time = esp_rtc_get_time_us();

    // Clear interrupt matrix for PRO CPU core
    core_intr_matrix_clear();

#ifndef CONFIG_IDF_ENV_FPGA
#ifdef CONFIG_ESP_CONSOLE_UART
    /*
     * Reconfigure ROM console UART baud rate using the initialized clock tree.
     */
    uint32_t clock_hz = esp_clk_apb_freq();

#if ESP_ROM_UART_CLK_IS_XTAL
    clock_hz = esp_clk_xtal_freq();
#endif

    esp_rom_output_tx_wait_idle(CONFIG_ESP_CONSOLE_ROM_SERIAL_PORT_NUM);

    _uart_ll_set_baudrate(
        UART_LL_GET_HW(CONFIG_ESP_CONSOLE_ROM_SERIAL_PORT_NUM),
        CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        clock_hz
    );

    int console_uart_tx_pin = U0TXD_GPIO_NUM;
    int console_uart_rx_pin = U0RXD_GPIO_NUM;

#if CONFIG_ESP_CONSOLE_UART_CUSTOM
    console_uart_tx_pin = (CONFIG_ESP_CONSOLE_UART_TX_GPIO >= 0)
        ? CONFIG_ESP_CONSOLE_UART_TX_GPIO
        : U0TXD_GPIO_NUM;
    console_uart_rx_pin = (CONFIG_ESP_CONSOLE_UART_RX_GPIO >= 0)
        ? CONFIG_ESP_CONSOLE_UART_RX_GPIO
        : U0RXD_GPIO_NUM;
#endif

    ESP_EARLY_LOGI(TAG, "GPIO %d and %d are used as console UART I/O pins",
                   console_uart_rx_pin, console_uart_tx_pin);
#endif
#endif

#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    esp_cache_err_int_init();
#endif

#if CONFIG_ESP_SYSTEM_MEMPROT && CONFIG_ESP_SYSTEM_MEMPROT_PMS && !CONFIG_ESP_SYSTEM_MEMPROT_PMS_TEST
    // Memprot cannot be locked during OS startup, as lock-on prevents PMS
    // changes until the next reboot. If this happens, it is likely a malicious
    // attempt to bypass the system safety setup, so print an error and reset.

    bool is_locked = false;
    if (esp_mprot_is_conf_locked_any(&is_locked) != ESP_OK || is_locked) {
        ESP_EARLY_LOGE(TAG,
                       "Memprot feature locked after the system reset! "
                       "Potential safety corruption, rebooting.");
        esp_restart_noos();
    }

    // default configuration of PMS Memprot
    esp_err_t memp_err = ESP_OK;
    esp_memp_config_t memp_cfg = ESP_MEMPROT_DEFAULT_CONFIG();

#if !CONFIG_ESP_SYSTEM_MEMPROT_PMS_LOCK
    memp_cfg.lock_feature = false;
#endif

    memp_err = esp_mprot_set_prot(&memp_cfg);

    if (memp_err != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "Failed to set Memprot feature (0x%08X: %s), rebooting.",
                       memp_err, esp_err_to_name(memp_err));
        esp_restart_noos();
    }
#endif

#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    // External devices (including SPI0/1, cache) should be initialized

#if !CONFIG_APP_BUILD_TYPE_RAM
    // Normal startup flow. We arrive here with the help of the 1st and 2nd
    // bootloaders. There are valid headers.

    // Read the application binary image header. This will also decrypt the
    // header if the image is encrypted.
    esp_image_header_t fhdr = {0};

    // We can access the image header through the cache by reading from the
    // memory-mapped virtual DROM start offset.
    uint32_t fhdr_src_addr =
        (uint32_t)(&_rodata_reserved_start)
        - sizeof(esp_image_header_t)
        - sizeof(esp_image_segment_header_t);

    hal_memcpy(&fhdr, (void *)fhdr_src_addr, sizeof(fhdr));

    if (fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        ESP_EARLY_LOGE(TAG, "Invalid app image header");
        abort();
    }

#if CONFIG_SPI_FLASH_SIZE_OVERRIDE
    int app_flash_size = esp_image_get_flash_size(fhdr.spi_size);
    if (app_flash_size < 1 * 1024 * 1024) {
        ESP_EARLY_LOGE(TAG, "Invalid flash size in app image header.");
        abort();
    }
    bootloader_flash_update_size(app_flash_size);
#endif

#else
    // CONFIG_APP_BUILD_TYPE_RAM && !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    bootloader_flash_unlock();
#endif
#endif
}

/*
 * We arrive here after the bootloader finished loading the program from flash.
 * The hardware is mostly uninitialized.
 *
 * ESP32-C3 has one CPU core, so there is no APP CPU / CPU1 path here.
 *
 * We do have a stack, so we can do the initialization in C.
 */
void IRAM_ATTR call_start_cpu0(void)
{
    soc_reset_reason_t rst_reas[1] = {
        RESET_REASON_CHIP_POWER_ON
    };

    init_cpu();

    get_reset_reason(rst_reas);

    // Clear BSS. Please do not attempt to do any complex stuff (like early logging) before this.
    init_bss(rst_reas);

#if CONFIG_LIBC_PICOLIBC
    init_pre_rtos_tls_area(0);
#endif

    // When the app is loaded into RAM for execution, some hardware
    // initialization steps normally done by the bootloader are done here.
#if CONFIG_APP_BUILD_TYPE_RAM
    ram_app_init();
#endif

    // Initialize the cache and mmu.
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    ext_mem_init();
#endif

    sys_rtc_init(rst_reas);

    // Frequency adjustment and other MSPI initialization stuff like PSRAM initialization.
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    mspi_init();
#endif

    /* ----------------------------------Separator-----------------------------
     * CPU0 can access external memory (cache) now. Please do not access
     * external memory before this.
     */

    /*
     * Initialize remaining early system services.
     */
    system_early_init(rst_reas);

    /*
     * Continue into ESP-IDF system startup.
     *
     * In normal ESP-IDF this eventually starts FreeRTOS and calls app_main().
     * In your own OS kernel, this is the call you would eventually replace
     * with your own kernel entry path.
     */
    SYS_STARTUP_FN();
}
