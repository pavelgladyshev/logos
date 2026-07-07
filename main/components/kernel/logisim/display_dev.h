#ifndef DISPLAY_DEV_H
#define DISPLAY_DEV_H

#define DISPLAY_MAJOR 2
#define SCREEN_DATA (*(volatile uint32_t *)0xFFFF0010)

int display_dev_init(void);

#endif
