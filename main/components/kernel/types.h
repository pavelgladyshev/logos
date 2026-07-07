#ifndef TYPES_H
#define TYPES_H

#ifdef ESP32
#include <stdint.h>
#else
    typedef unsigned char      uint8_t;
    typedef unsigned short     uint16_t;
    typedef unsigned int       uint32_t;
    typedef signed int         int32_t;

    #ifndef NULL
    #define NULL ((void*)0)
    #endif
#endif
#endif /* TYPES_H */
