/*
 * Console character device driver
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef CONSOLE_DEV_H
#define CONSOLE_DEV_H

/* Console device major number */
#define CONSOLE_MAJOR  1

/*
 * Initialize the console device driver
 * Call this at system startup to register the driver
 */
int console_dev_init(void);

#endif /* CONSOLE_DEV_H */
