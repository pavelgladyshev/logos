#include "kernel_time.h"

#include "esp_rom_sys.h"

/* Split long delays so the microsecond conversion stays small and predictable. */
void kernel_delay_ms(uint32_t ms)
{
    while (ms > 0) {
        uint32_t chunk = ms > 1000 ? 1000 : ms;

        esp_rom_delay_us(chunk * 1000U);
        ms -= chunk;
    }
}
