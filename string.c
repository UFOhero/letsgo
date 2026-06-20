#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memset(void *dest, int c, size_t n) {
    char *d = dest;
    for (size_t i = 0; i < n; i++) d[i] = c;
    return dest;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) { s1++; s2++; }
    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (!s1[i]) return 0;
    }
    return 0;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

static char *strtok_ptr = NULL;
char *strtok(char *str, const char *delim) {
    if (str) strtok_ptr = str;
    if (!strtok_ptr) return NULL;
    while (*strtok_ptr && strchr(delim, *strtok_ptr)) strtok_ptr++;
    if (*strtok_ptr == 0) return NULL;
    char *token = strtok_ptr;
    while (*strtok_ptr && !strchr(delim, *strtok_ptr)) strtok_ptr++;
    if (*strtok_ptr) {
        *strtok_ptr = 0;
        strtok_ptr++;
    }
    return token;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == c) last = s;
        s++;
    }
    return (char*)last;
}