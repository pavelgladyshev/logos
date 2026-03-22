/*
 * User-space C library implementation
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * All I/O functions use system calls to communicate with the kernel.
 * String and memory functions are pure user-space implementations.
 */

#include "libc.h"

/* Variadic argument handling using GCC builtins */
typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

/*
 * spawn() convenience wrapper — calls spawnve() with NULL envp,
 * which inherits the parent's environment unchanged.
 */
int spawn(const char *path, char *const argv[])
{
    return spawnve(path, argv, (char *const *)0);
}

/*
 * exec() convenience wrapper — calls execve() with NULL envp,
 * which keeps the current process's environment unchanged.
 */
int exec(const char *path, char *const argv[])
{
    return execve(path, argv, (char *const *)0);
}

/*
 * Standard I/O functions
 */

int putchar(int c)
{
    char ch = (char)c;
    if (write(STDOUT_FILENO, &ch, 1) == 1) {
        return c;
    }
    return -1;
}

int puts(const char *s)
{
    size_t len = strlen(s);
    if (write(STDOUT_FILENO, s, len) != (ssize_t)len) {
        return -1;
    }
    char nl = '\n';
    write(STDOUT_FILENO, &nl, 1);
    return 0;
}

int getchar(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return -1;
}

char *gets(char *buf, int size)
{
    int i = 0;
    int c;

    while (i < size - 1) {
        c = getchar();
        if (c < 0) break;  /* EOF or error */

        if (c == '\r' || c == '\n') {
            putchar('\n');  /* Echo newline */
            break;
        }

        if (c == 127 || c == 8) {  /* Backspace or DEL */
            if (i > 0) {
                i--;
                write(STDOUT_FILENO, "\b \b", 3);  /* Erase character on screen */
            }
            continue;
        }

        putchar(c);  /* Echo character */
        buf[i++] = c;
    }
    buf[i] = '\0';
    return buf;
}

/* Helper: format unsigned int into buffer, return length */
static int fmt_uint(unsigned int n, char *buf, int bufsz)
{
    int i = 0;
    char tmp[12];
    if (n == 0) { tmp[i++] = '0'; }
    else { while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; } }
    if (i > bufsz) i = bufsz;
    int len = i;
    while (i > 0) *buf++ = tmp[--i];
    return len;
}

/* Helper: format signed int into buffer, return length */
static int fmt_int(int n, char *buf, int bufsz)
{
    if (n < 0) {
        *buf = '-';
        return 1 + fmt_uint((unsigned int)(-n), buf + 1, bufsz - 1);
    }
    return fmt_uint((unsigned int)n, buf, bufsz);
}

/* Helper: format hex into buffer, return length */
static int fmt_hex(unsigned int n, char *buf, int bufsz)
{
    int i, started = 0, len = 0;
    for (i = 28; i >= 0; i -= 4) {
        int digit = (n >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            if (len < bufsz)
                buf[len++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
            started = 1;
        }
    }
    return len;
}

/* Helper: emit a string with optional width and left-justify */
static int emit_padded(const char *s, int len, int width, int left_justify)
{
    int count = 0;
    int pad = (width > len) ? width - len : 0;
    if (!left_justify) { for (int i = 0; i < pad; i++) { putchar(' '); count++; } }
    for (int i = 0; i < len; i++) { putchar(s[i]); count++; }
    if (left_justify) { for (int i = 0; i < pad; i++) { putchar(' '); count++; } }
    return count;
}

/* printf supporting %s, %d, %u, %x, %c, %% with optional width and '-' flag */
int printf(const char *fmt, ...)
{
    va_list ap;
    int count = 0;
    char buf[24];

    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            /* Parse flags */
            int left_justify = 0;
            if (*fmt == '-') { left_justify = 1; fmt++; }
            /* Parse width */
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
            /* Parse conversion */
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(ap, const char*);
                    if (!s) s = "(null)";
                    int len = 0;
                    while (s[len]) len++;
                    count += emit_padded(s, len, width, left_justify);
                    break;
                }
                case 'd': {
                    int len = fmt_int(va_arg(ap, int), buf, sizeof(buf));
                    count += emit_padded(buf, len, width, left_justify);
                    break;
                }
                case 'u': {
                    int len = fmt_uint(va_arg(ap, unsigned int), buf, sizeof(buf));
                    count += emit_padded(buf, len, width, left_justify);
                    break;
                }
                case 'x': {
                    int len = fmt_hex(va_arg(ap, unsigned int), buf, sizeof(buf));
                    count += emit_padded(buf, len, width, left_justify);
                    break;
                }
                case 'c':
                    buf[0] = (char)va_arg(ap, int);
                    count += emit_padded(buf, 1, width, left_justify);
                    break;
                case '%':
                    putchar('%');
                    count++;
                    break;
                case '\0':
                    va_end(ap);
                    return count;
                default:
                    putchar('%');
                    putchar(*fmt);
                    count += 2;
                    break;
            }
        } else {
            putchar(*fmt);
            count++;
        }
        fmt++;
    }

    va_end(ap);
    return count;
}

/*
 * String functions
 */

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strcpy(char *dest, const char *src)
{
    char *ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *ret = dest;
    while (n > 0 && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    return ret;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)0;
}

/*
 * Memory functions
 */

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

int atoi(const char *s)
{
    int n = 0;

    if (*s == '\0') return -1;

    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }

    /* If we stopped on a non-NUL character, the string wasn't all digits */
    if (*s != '\0') return -1;

    return n;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

/*
 * Environment variables
 *
 * setenv(), unsetenv(), env_count(), getenv_r(), and getenv_entry()
 * are direct syscall stubs in syscall.S.
 * getenv() and env_to_envp() add static buffers on top.
 */

#define ENV_MAX_VARS  16
#define ENV_MAX_LEN   64

/* Static buffer for getenv() return value */
static char getenv_buf[ENV_MAX_LEN];

/* Static buffer pool for env_to_envp() return values */
static char envp_pool[ENV_MAX_VARS][ENV_MAX_LEN];

char *getenv(const char *name)
{
    int result = getenv_r(name, getenv_buf, ENV_MAX_LEN);
    if (result < 0) return (char *)0;
    return getenv_buf;
}

int env_to_envp(char *envp[])
{
    int count = env_count();
    int i;
    for (i = 0; i < count && i < ENV_MAX_VARS; i++) {
        getenv_entry(i, envp_pool[i], ENV_MAX_LEN);
        envp[i] = envp_pool[i];
    }
    envp[i] = (char *)0;
    return i;
}
