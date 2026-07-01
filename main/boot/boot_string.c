/*
 * Bootloader string/memory utilities
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "boot.h"

void boot_memcpy(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
}

void boot_memset(void *dst, int c, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--)
        *d++ = (uint8_t)c;
}

int boot_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (uint8_t)*s1 - (uint8_t)*s2;
}
