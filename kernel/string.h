/*
 * String and memory functions for bare metal RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef STRING_H
#define STRING_H

#include "types.h"

int   strlen(const char *s);
int   strcmp(const char *s1, const char *s2);
int   strncmp(const char *s1, const char *s2, uint32_t n);
char *strchr(const char *s, int c);
void  strcpy(char *dst, const char *src);
void  memset(void *dst, int c, uint32_t n);
void  memcpy(void *dst, const void *src, uint32_t n);

#endif /* STRING_H */
