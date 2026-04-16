#include "utils.h"

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strlen(const char *str) {
	int len = 0;

	while (str[len] != '\0') {
		len++;
	}

	return len;
}

long strtol(const char *nptr, char **endptr, int base) {
	long result = 0;
	int sign = 1;

	if (base < 2 || base > 36) {
		if (endptr) {
			*endptr = (char *)nptr;
		}
		return 0;
	}

	if (*nptr == '-') {
		sign = -1;
		nptr++;
	}

	while (*nptr != '\0') {
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

		result = result * base + digit;
		nptr++;
	}

	if (endptr) {
		*endptr = (char *)nptr;
	}

	return result * sign;
}

int parse_u64(const char *text, uint64_t *value) {
	uint64_t result = 0;
	int base = 10;
	int parsed_digit = 0;

	if (!text || !value || *text == '\0') {
		return -1;
	}

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		text += 2;
	}

	if (*text == '\0') {
		return -1;
	}

	while (*text != '\0') {
		unsigned int digit;
		char c = *text++;

		if (c >= '0' && c <= '9') {
			digit = (unsigned int)(c - '0');
		} else if (c >= 'a' && c <= 'f') {
			digit = (unsigned int)(c - 'a' + 10);
		} else if (c >= 'A' && c <= 'F') {
			digit = (unsigned int)(c - 'A' + 10);
		} else {
			return -1;
		}

		if (digit >= (unsigned int)base) {
			return -1;
		}

		result = (result * (uint64_t)base) + (uint64_t)digit;
		parsed_digit = 1;
	}

	if (!parsed_digit) {
		return -1;
	}

	*value = result;
	return 0;
}

void memzero(void *buffer, size_t size) {
	unsigned char *cursor = (unsigned char *)buffer;

	while (size-- > 0) {
		*cursor++ = 0;
	}
}
