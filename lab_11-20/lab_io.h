#ifndef LAB_IO_H
#define LAB_IO_H
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { OK, INVALID_INPUT, MEMORY_ERROR, RANGE_ERROR, FILE_ERROR, END_OF_FILE } Status;

static inline Status read_line(FILE *file, char **result)
{
    if (file == NULL || result == NULL) return INVALID_INPUT;
    *result = NULL;
    size_t length = 0, capacity = 64;
    char *line = malloc(capacity);
    if (line == NULL) return MEMORY_ERROR;
    int c;
    while ((c = fgetc(file)) != EOF && c != '\n')
    {
        if (c == 0) { free(line); return INVALID_INPUT; }
        if (length == capacity - 1)
        {
            if (capacity > SIZE_MAX / 2) { free(line); return RANGE_ERROR; }
            capacity *= 2;
            char *new_line = realloc(line, capacity);
            if (new_line == NULL) { free(line); return MEMORY_ERROR; }
            line = new_line;
        }
        line[length++] = (char)c;
    }
    if (ferror(file)) { free(line); return FILE_ERROR; }
    if (c == EOF && length == 0) { free(line); return END_OF_FILE; }
    line[length] = '\0';
    *result = line;
    return OK;
}

/* Разрезает собственный буфер: указатели fields действуют до free(line). */
static inline Status split_fields(char *line, char **fields, size_t expected)
{
    if (line == NULL || fields == NULL || expected == 0) return INVALID_INPUT;
    size_t count = 0;
    char *p = line;
    while (*p != '\0')
    {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;
        if (count == expected) return INVALID_INPUT;
        fields[count++] = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        if (*p != '\0') *p++ = '\0';
    }
    return count == expected ? OK : INVALID_INPUT;
}

static inline Status parse_size(const char *text, size_t *result)
{
    if (text == NULL || result == NULL || *text == '\0') return INVALID_INPUT;
    size_t value = 0;
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] < '0' || text[i] > '9') return INVALID_INPUT;
        size_t digit = (size_t)(text[i] - '0');
        if (value > (SIZE_MAX - digit) / 10) return RANGE_ERROR;
        value = value * 10 + digit;
    }
    *result = value;
    return OK;
}

static inline Status copy_text(const char *text, char **result)
{
    if (text == NULL || result == NULL) return INVALID_INPUT;
    *result = NULL;
    size_t length = strlen(text);
    if (length == SIZE_MAX) return RANGE_ERROR;
    char *copy = malloc(length + 1);
    if (copy == NULL) return MEMORY_ERROR;
    memcpy(copy, text, length + 1);
    *result = copy;
    return OK;
}
#endif
