/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "esp_private/esp_system_attr.h"
#include "esp_err.h"

#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_app_format.h"

#include "esp_private/cache_err_int.h"
#include "esp_clk_internal.h"

#include "esp_rom_serial_output.h"
#include "esp_rom_sys.h"
#include "esp_rom_caps.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if CONFIG_IDF_TARGET_ESP32
#include "soc/dport_reg.h"
#include "esp32/rom/cache.h"
#include "esp32/rom/secure_boot.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/cache.h"
#include "esp32s2/rom/secure_boot.h"
#include "esp32s2/memprot.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/cache.h"
#include "esp32s3/rom/secure_boot.h"
#include "esp_memprot.h"
#include "soc/assist_debug_reg.h"
#include "soc/system_reg.h"
#include "esp32s3/rom/opi_flash.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/cache.h"
#include "esp32c3/rom/secure_boot.h"
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/cache.h"
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32C61
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32C5
#include "esp32c5/rom/cache.h"
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/cache.h"
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/cache.h"
#include "esp32c2/rom/secure_boot.h"
#elif CONFIG_IDF_TARGET_ESP32P4
#include "soc/hp_sys_clkrst_reg.h"
#include "hal/l2mem_ll.h"
#elif CONFIG_IDF_TARGET_ESP32H21
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32H4
#include "soc/lp_aon_reg.h"
#include "esp_memprot.h"
#elif CONFIG_IDF_TARGET_ESP32S31
#endif

#include "rom/ets_sys.h"
#include "esp_private/cache_utils.h"
#include "esp_private/rtc_clk.h"
#include "esp_rtc_time.h"
#include "rom/rtc.h"

#if SOC_INT_CLIC_SUPPORTED
#include "hal/interrupt_clic_ll.h"
#endif // SOC_INT_CLIC_SUPPORTED

#include "esp_private/esp_mmu_map_private.h"
#include "esp_private/image_process.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#include "esp_private/esp_psram_extram.h"
#if SOC_SPIRAM_XIP_SUPPORTED
#include "esp_private/mmu_psram_flash.h"
#endif
#endif

#include "esp_private/spi_flash_os.h"
#include "esp_private/mspi_timing_tuning.h"
#include "bootloader_flash_config.h"
#include "bootloader_flash.h"
#include "esp_private/crosscore_int.h"

#include "esp_private/sleep_gpio.h"
#if SOC_WDT_SUPPORTED || SOC_RTC_WDT_SUPPORTED
#include "hal/wdt_hal.h"
#endif
#include "soc/rtc.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/mmu_hal.h"
#include "hal/efuse_ll.h"
#include "hal/uart_ll.h"
#include "soc/uart_pins.h"
#include "hal/cpu_utility_ll.h"
#include "soc/periph_defs.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "esp_private/esp_clk_tree_common.h"

#if CONFIG_ESP32_TRAX || CONFIG_ESP32S2_TRAX || CONFIG_ESP32S3_TRAX
#include "esp_private/trax.h"
#endif

#include "bootloader_mem.h"

#if CONFIG_APP_BUILD_TYPE_RAM
#include "esp_rom_spiflash.h"
#include "bootloader_init.h"
#include "esp_private/bootloader_flash_internal.h"
#include "spi_flash_mmap.h"
#endif // CONFIG_APP_BUILD_TYPE_RAM

//This dependency will be removed in the future
#include "soc/ext_mem_defs.h"

#include "esp_private/startup_internal.h"
#include "esp_private/system_internal.h"
extern int _bss_start;
extern int _bss_end;


extern int _rtc_bss_start;
extern int _rtc_bss_end;



extern char _instruction_reserved_start;
extern char _instruction_reserved_end;
extern char _rodata_reserved_start;
extern char _rodata_reserved_end;

extern int _vector_table;
extern void kernel_start(void) __attribute__((noreturn));

ESP_LOG_ATTR_TAG(TAG, "cpu_start");



static void core_intr_matrix_clear(void)
{
    __attribute__((unused)) uint32_t core_id = esp_cpu_get_core_id();

    /* NOTE: With ESP-TEE enabled, each iteration in this loop results in a service call.
    * To accelerate the boot-up process, the interrupt configuration is pre-cleared in the TEE,
    * allowing this step to be safely skipped here.
    */
#if !CONFIG_SECURE_ENABLE_TEE
    for (int i = 0; i < ETS_MAX_INTR_SOURCE; i++) {
#if SOC_INT_CLIC_SUPPORTED
        interrupt_clic_ll_route(core_id, i, ETS_INVALID_INUM);
#else
        esp_rom_route_intr_matrix(core_id, i, ETS_INVALID_INUM);
#endif  // SOC_INT_CLIC_SUPPORTED
    }
#endif  // !CONFIG_SECURE_ENABLE_TEE

#if SOC_INT_CLIC_SUPPORTED
    for (int i = 0; i < 32; i++) {
        /* Set all the CPU interrupt lines to vectored by default, as it is on other RISC-V targets */
        esprv_int_set_vectored(i, true);
    }
#endif // SOC_INT_CLIC_SUPPORTED
}

FORCE_INLINE_ATTR IRAM_ATTR void init_cpu(void)
{
#ifdef __riscv
    if (esp_cpu_dbgr_is_attached()) {
        /* Let debugger some time to detect that target started, halt it, enable ebreaks and resume.
           500ms should be enough. */
        for (uint32_t ms_num = 0; ms_num < 2; ms_num++) {
            esp_rom_delay_us(100000);
        }
    }
    // Configure the global pointer register
    // (This should be the first thing IDF app does, as any other piece of code could be
    // relaxed by the linker to access something relative to __global_pointer$)
    __asm__ __volatile__(
        ".option push\n"
        ".option norelax\n"
        "la gp, __global_pointer$\n"
        ".option pop"
    );
#endif

    /* NOTE: When ESP-TEE is enabled, this sets up the callback function
     * which redirects all the interrupt management for the REE (user app)
     * to the TEE by raising the appropriate service calls.
     */
#if CONFIG_SECURE_ENABLE_TEE
    extern uint32_t esp_tee_service_call(int argc, ...);
    esprv_int_setup_mgmt_cb((void *)esp_tee_service_call);
#endif

#if SOC_BRANCH_PREDICTOR_SUPPORTED
    esp_cpu_branch_prediction_enable();
#endif
    // Move exception vectors to IRAM
    esp_cpu_intr_set_ivt_addr(&_vector_table);
#if SOC_INT_CLIC_SUPPORTED
    /* When hardware vectored interrupts are enabled in CLIC,
     * the CPU jumps to this base address + 4 * interrupt_id.
     */
    /* NOTE: When ESP-TEE is enabled, this sets up the U-mode
     * interrupt vector table (UTVT) */
    esp_cpu_intr_set_xtvt_addr(&_mtvt_table);
#endif
#if SOC_CPU_SUPPORT_WFE
    esp_cpu_disable_wfe_mode();
#endif
}


//Keep this static, the compiler will check if rst_reas is initialized.
FORCE_INLINE_ATTR IRAM_ATTR void get_reset_reason(soc_reset_reason_t *rst_reas)
{
    rst_reas[0] = esp_rom_get_reset_reason(0);
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    rst_reas[1] = esp_rom_get_reset_reason(1);
#endif
}




FORCE_INLINE_ATTR IRAM_ATTR void init_bss(const soc_reset_reason_t *rst_reas)
{
#if CONFIG_ESP32P4_SELECTS_REV_LESS_V3
    memset(&_bss_start_low, 0, (uintptr_t)&_bss_end_low - (uintptr_t)&_bss_start_low);
    memset(&_bss_start_high, 0, (uintptr_t)&_bss_end_high - (uintptr_t)&_bss_start_high);
#else
    memset(&_bss_start, 0, (uintptr_t)&_bss_end - (uintptr_t)&_bss_start);
#endif // CONFIG_ESP32P4_SELECTS_REV_LESS_V3

#if CONFIG_BT_LE_RELEASE_IRAM_SUPPORTED
    // Clear Bluetooth bss
    memset(&_bss_bt_start, 0, (uintptr_t)&_bss_bt_end - (uintptr_t)&_bss_bt_start);
#endif // CONFIG_BT_LE_RELEASE_IRAM_SUPPORTED

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_ESP32_IRAM_AS_8BIT_ACCESSIBLE_MEMORY)
    // Clear IRAM BSS
    memset(&_iram_bss_start, 0, (uintptr_t)&_iram_bss_end - (uintptr_t)&_iram_bss_start);
#endif

#if SOC_RTC_FAST_MEM_SUPPORTED || SOC_RTC_SLOW_MEM_SUPPORTED
    /* Unless waking from deep sleep (implying RTC memory is intact), clear RTC bss */
    if (rst_reas[0] != RESET_REASON_CORE_DEEP_SLEEP) {
        memset(&_rtc_bss_start, 0, (uintptr_t)&_rtc_bss_end - (uintptr_t)&_rtc_bss_start);
    }
#endif
}


FORCE_INLINE_ATTR IRAM_ATTR void ext_mem_init(void)
{
#if CONFIG_IDF_TARGET_ESP32H4
    // Initially assign Layer6 memory to CPU RAM. Multicore startup will
    // reassign it to ICache1 if the app is not configured for single-core mode.
    REG_CLR_BIT(LP_AON_SRAM_USAGE_CONF_REG, LP_AON_ICACHE1_USAGE);
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE && !SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
    // It helps to fix missed cache settings for other cores. It happens when bootloader is unicore.
    do_multicore_settings();
#endif

    cache_hal_config_t config = {
#if CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        .core_nums = 1,
#else
        .core_nums = SOC_CPU_CORES_NUM,
#endif
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
        .l2_cache_size = CONFIG_CACHE_L2_CACHE_SIZE,
        .l2_cache_line_size = CONFIG_CACHE_L2_CACHE_LINE_SIZE,
#endif
    };
    //cache hal ctx needs to be initialised
    cache_hal_init(&config);
    //mmu hal ctx needs to be initialised
    mmu_hal_config_t mmu_config = {
#if CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        .core_nums = 1,
#else
        .core_nums = SOC_CPU_CORES_NUM,
#endif
    };
    mmu_hal_ctx_init(&mmu_config);

#if CONFIG_IDF_TARGET_ESP32S2
    /* Configure the mode of instruction cache : cache size, cache associated ways, cache line size. */
    extern void esp_config_instruction_cache_mode(void);
    esp_config_instruction_cache_mode();

    /* If we need use SPIRAM, we should use data cache, or if we want to access rodata, we also should use data cache.
       Configure the mode of data : cache size, cache associated ways, cache line size.
       Enable data cache, so if we don't use SPIRAM, it just works. */
    extern void esp_config_data_cache_mode(void);
    esp_config_data_cache_mode();
    Cache_Enable_DCache(0);
#endif

#if CONFIG_IDF_TARGET_ESP32S3
    /* Configure the mode of instruction cache : cache size, cache line size. */
    extern void rom_config_instruction_cache_mode(uint32_t cfg_cache_size, uint8_t cfg_cache_ways, uint8_t cfg_cache_line_size);
    rom_config_instruction_cache_mode(CONFIG_ESP32S3_INSTRUCTION_CACHE_SIZE, CONFIG_ESP32S3_ICACHE_ASSOCIATED_WAYS, CONFIG_ESP32S3_INSTRUCTION_CACHE_LINE_SIZE);

    /* If we need use SPIRAM, we should use data cache.
       Configure the mode of data : cache size, cache line size.*/
    Cache_Suspend_DCache();
    extern void rom_config_data_cache_mode(uint32_t cfg_cache_size, uint8_t cfg_cache_ways, uint8_t cfg_cache_line_size);
    rom_config_data_cache_mode(CONFIG_ESP32S3_DATA_CACHE_SIZE, CONFIG_ESP32S3_DCACHE_ASSOCIATED_WAYS, CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE);
    Cache_Resume_DCache(0);
#endif // CONFIG_IDF_TARGET_ESP32S3

    // For RAM loadable ELF case, we don't need to reserve IROM/DROM as instructions and data
    // are all in internal RAM. If the RAM loadable ELF has any requirement to memory map the
    // external flash then it should use flash or partition mmap APIs.
#if ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE
    uint32_t cache_mmu_irom_size = 0;
#if !CONFIG_APP_BUILD_TYPE_ELF_RAM
    uint32_t _instruction_size = (uint32_t)&_instruction_reserved_end - (uint32_t)&_instruction_reserved_start;
    cache_mmu_irom_size = ((_instruction_size + SPI_FLASH_MMU_PAGE_SIZE - 1) / SPI_FLASH_MMU_PAGE_SIZE) * sizeof(uint32_t);
#endif // !CONFIG_APP_BUILD_TYPE_ELF_RAM
    /* Configure the Cache MMU size for instruction and rodata in flash. */
    Cache_Set_IDROM_MMU_Size(cache_mmu_irom_size, CACHE_DROM_MMU_MAX_END - cache_mmu_irom_size);
#endif // ESP_ROM_NEEDS_SET_CACHE_MMU_SIZE
}



#if CONFIG_LIBC_PICOLIBC
FORCE_INLINE_ATTR IRAM_ATTR void init_pre_logos_tls_area(int cpu_num)
{
    /**
     * Initialize the TLS area before RTOS starts, in case any code tries to access
     * TLS variables.
     *
     * TODO IDF-14914: Currently, we only initialize errno, which is the first TLS
     * variable as guaranteed by the linker script.
     */
    static int s_errno_array[SOC_CPU_CORES_NUM];
    esp_cpu_set_threadptr(&s_errno_array[cpu_num]);
}
#endif


#define MSPI_INIT_ATTR NOINLINE_ATTR static
MSPI_INIT_ATTR void sys_rtc_init(const soc_reset_reason_t *rst_reas)
{
#if SOC_RTC_WDT_SUPPORTED
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
#define RWDT_RESET           RESET_REASON_CORE_RWDT
#else
#define RWDT_RESET           RESET_REASON_CORE_RTC_WDT
#endif

#if CONFIG_IDF_TARGET_ESP32P4
#define MWDT_RESET           RESET_REASON_CORE_MWDT
#else
#define MWDT_RESET           RESET_REASON_CORE_MWDT0
#endif

#ifndef CONFIG_BOOTLOADER_WDT_ENABLE
    // from panic handler we can be reset by RWDT or TG0WDT
    if (rst_reas[0] == RWDT_RESET || rst_reas[0] == MWDT_RESET
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
            || rst_reas[1] == RWDT_RESET || rst_reas[1] == MWDT_RESET
#endif
       ) {
        wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_disable(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    }
#endif
#endif /* SOC_RTC_WDT_SUPPORTED */

    // Configure the power related stuff. After this the MSPI timing tuning can be done.
    esp_rtc_init();
}

static void IRAM_ATTR disable_bootloader_wdt(void)
{
#if SOC_RTC_WDT_SUPPORTED
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();

    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_disable(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
#endif
}

NOINLINE_ATTR static void system_early_init(const soc_reset_reason_t *rst_reas)
{
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    esp_mspi_pin_reserve();
#endif

    ESP_EARLY_LOGI(TAG, "Unicore app");

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

    g_startup_time = esp_rtc_get_time_us();

    core_intr_matrix_clear();

#ifndef CONFIG_IDF_ENV_FPGA
#ifdef CONFIG_ESP_CONSOLE_UART
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
    bool is_locked = false;
    if (esp_mprot_is_conf_locked_any(&is_locked) != ESP_OK || is_locked) {
        ESP_EARLY_LOGE(TAG,
                       "Memprot feature locked after the system reset! "
                       "Potential safety corruption, rebooting.");
        esp_restart_noos();
    }

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
#if !CONFIG_APP_BUILD_TYPE_RAM
    esp_image_header_t fhdr = {0};
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
    bootloader_flash_unlock();
#endif
#endif

    (void)rst_reas;
}


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
    // This function needs to be called when PLL is enabled. Needs to be called after spi_flash_init_chip_state in case
    // some state of flash is modified.
    mspi_timing_flash_tuning();
}


MSPI_INIT_ATTR void mspi_init(void)
{
#if CONFIG_ESPTOOLPY_OCT_FLASH && !CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT
    bool efuse_opflash_en = efuse_ll_get_flash_type();
    if (!efuse_opflash_en) {
        ESP_DRAM_LOGE(TAG, "Octal Flash option selected, but EFUSE not configured!");
        abort();
    }
#endif

    esp_mspi_pin_init();
    // For Octal flash, it's hard to implement a read_id function in OPI mode for all vendors.
    // So we have to read it here in SPI mode, before entering the OPI mode.
    bootloader_flash_update_id();

    flash_init_state();

    esp_mmu_map_init(); //required by image_process() and flash_mmap APIs

#if !CONFIG_APP_BUILD_TYPE_ELF_RAM
#if CONFIG_SPIRAM_FLASH_LOAD_TO_PSRAM
    ESP_ERROR_CHECK(image_process());
#endif
#endif

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
 

void IRAM_ATTR call_start_cpu0(void)
{

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    soc_reset_reason_t rst_reas[SOC_CPU_CORES_NUM] = { [0 ... SOC_CPU_CORES_NUM - 1] = RESET_REASON_CHIP_POWER_ON };
#else
    soc_reset_reason_t rst_reas[1] = { RESET_REASON_CHIP_POWER_ON };
#endif

    init_cpu();
    get_reset_reason(rst_reas);
    // Clear BSS. Please do not attempt to do any complex stuff (like early logging) before this.
    init_bss(rst_reas);

#if CONFIG_LIBC_PICOLIBC
    init_pre_logos_tls_area(0);
#endif

    // Initialize the cache and mmu.
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    ext_mem_init();
#endif // !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP

    sys_rtc_init(rst_reas);
    disable_bootloader_wdt();

    // Frequency adjustment and other MSPI initialization stuff like PSRAM initialization.
#if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
    mspi_init();
#endif // !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP

    system_early_init(rst_reas);

    kernel_start();

}
