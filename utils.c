#include "uart.h"

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

long strtol(const char *nptr, char **endptr, int base) {
	long res = 0;
	int sign = 1;
	if (*nptr == '-') {
		sign = -1;
		nptr++;
	}
	while (*nptr) {
		char c = *nptr;
		int digit;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'z') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'Z') {
			digit = c - 'A' + 10;
		} else {
			break;
		}
		if (digit >= base) {
			break;
		}
		res = res * base + digit;
		nptr++;
	}
	if (endptr) {
		*endptr = (char *)nptr;
	}
	return res * sign;
}
