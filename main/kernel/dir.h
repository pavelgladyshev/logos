/*
 * Directory operations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef DIR_H
#define DIR_H

#include "fs_types.h"

int dir_lookup(uint32_t dir_ino, const char *name, uint32_t *found_ino);
int dir_add(uint32_t dir_ino, const char *name, uint32_t ino);
int dir_remove(uint32_t dir_ino, const char *name);
int dir_list(uint32_t dir_ino, struct dirent *entries, uint32_t max_entries, uint32_t *count);
int fs_mkdir(uint32_t parent_ino, const char *name);
int fs_rmdir(uint32_t parent_ino, const char *name);

#endif /* DIR_H */
