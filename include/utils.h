#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

int strcmp(const char *s1, const char *s2);
int strlen(const char *str);
long strtol(const char *nptr, char **endptr, int base);
void memzero(void *buffer, size_t size);

#endif /* UTILS_H */
