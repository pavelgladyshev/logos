/*
 * String and memory functions for bare metal RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "string.h"

int strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (uint8_t)*s1 - (uint8_t)*s2;
}

int strncmp(const char *s1, const char *s2, uint32_t n) {
    while (n > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (uint8_t)*s1 - (uint8_t)*s2;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)0;
}

void strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

void memset(void *dst, int c, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    /* Whole words while the destination is word-aligned (see memcpy) */
    if (((uint32_t)d & 3) == 0) {
        uint32_t w = (uint8_t)c * 0x01010101u;    /* the byte in all four bytes of a word */
        for (; n >= 4; n -= 4, d += 4)
            *(uint32_t *)d = w;
    }
    while (n--) *d++ = (uint8_t)c;    /* the rest byte by byte */
}

void memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    /* Whole words while both are word-aligned: a lw/sw at an address that is
     * not a multiple of 4 traps (address misaligned). */
    if ((((uint32_t)d | (uint32_t)s) & 3) == 0) {
        for (; n >= 4; n -= 4, d += 4, s += 4)
            *(uint32_t *)d = *(const uint32_t *)s;
    }
    while (n--) *d++ = *s++;    /* the rest, and unaligned copies, byte by byte */
}
