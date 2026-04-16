#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

int strcmp(const char *s1, const char *s2);
int strlen(const char *str);
long strtol(const char *nptr, char **endptr, int base);
int parse_u64(const char *text, uint64_t *value);
void memzero(void *buffer, size_t size);

#endif /* UTILS_H */
