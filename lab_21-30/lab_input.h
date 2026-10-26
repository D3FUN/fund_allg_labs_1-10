#ifndef LAB_INPUT_H
#define LAB_INPUT_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, IO_ERROR, RANGE_ERROR, END_INPUT } Status;

/* A complete line, without a fixed-size buffer. The caller owns *out. */
static inline Status read_line(FILE *file, char **out)
{
    if (!file || !out) return INPUT_ERROR;
    *out = NULL;
    size_t length = 0, capacity = 64;
    char *line = malloc(capacity);
    if (!line) return MEMORY_ERROR;
    int c;
    while ((c = fgetc(file)) != EOF && c != '\n') {
        if (c == 0) { free(line); return INPUT_ERROR; }
        if (length == capacity - 1) {
            if (capacity > SIZE_MAX / 2) { free(line); return RANGE_ERROR; }
            size_t next = capacity * 2;
            char *temp = realloc(line, next);
            if (!temp) { free(line); return MEMORY_ERROR; }
            line = temp;
            capacity = next;
        }
        line[length++] = (char)c;
    }
    if (ferror(file)) { free(line); return IO_ERROR; }
    if (c == EOF && length == 0) { free(line); return END_INPUT; }
    line[length] = '\0';
    *out = line;
    return OK;
}

static inline Status split_words(char *line, char **words, size_t capacity, size_t *count)
{
    if (!line || !words || !count || !capacity) return INPUT_ERROR;
    *count = 0;
    char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*count == capacity) return INPUT_ERROR;
        words[(*count)++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    return OK;
}

static inline Status parse_unsigned(const char *text, uint64_t limit, uint64_t *out)
{
    if (!text || !*text || !out) return INPUT_ERROR;
    uint64_t value = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return INPUT_ERROR;
        unsigned digit = (unsigned)(*p - '0');
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10))
            return RANGE_ERROR;
        value = value * 10 + digit;
    }
    *out = value;
    return OK;
}

static inline Status parse_signed(const char *text, int64_t *out)
{
    if (!text || !out) return INPUT_ERROR;
    int negative = *text == '-';
    if (*text == '-' || *text == '+') text++;
    uint64_t value;
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    Status status = parse_unsigned(text, limit, &value);
    if (status != OK) return status;
    if (negative && value == (uint64_t)INT64_MAX + 1) *out = INT64_MIN;
    else *out = negative ? -(int64_t)value : (int64_t)value;
    return OK;
}
#endif
