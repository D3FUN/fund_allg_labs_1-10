#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { OK, INVALID_INPUT, MEMORY_ERROR, RANGE_ERROR, FILE_ERROR } Status;
typedef struct { size_t line, column; } Match;
typedef struct
{
    const char *path;
    Match *matches;
    size_t count;
    Status status;
} FileMatches;

Status free_search(FileMatches *files, size_t count)
{
    if (files == NULL && count != 0) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++) free(files[i].matches);
    free(files);
    return OK;
}

Status read_bytes(const char *path, unsigned char **result, size_t *length)
{
    if (path == NULL || *path == '\0' || result == NULL || length == NULL) return INVALID_INPUT;
    *result = NULL;
    *length = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return FILE_ERROR;
    size_t capacity = 256, used = 0;
    unsigned char *bytes = malloc(capacity);
    Status status = bytes == NULL ? MEMORY_ERROR : OK;
    int c;
    while (status == OK && (c = fgetc(file)) != EOF)
    {
        if (used == capacity)
        {
            if (capacity > SIZE_MAX / 2) status = RANGE_ERROR;
            else
            {
                capacity *= 2;
                unsigned char *temporary = realloc(bytes, capacity);
                if (temporary == NULL) status = MEMORY_ERROR;
                else bytes = temporary;
            }
        }
        if (status == OK) bytes[used++] = (unsigned char)c;
    }
    if (ferror(file)) status = FILE_ERROR;
    if (fclose(file) != 0 && status == OK) status = FILE_ERROR;
    if (status != OK) { free(bytes); return status; }
    *result = bytes;
    *length = used;
    return OK;
}

Status find_matches(const unsigned char *bytes, size_t length, const char *pattern,
                    Match **result, size_t *count)
{
    if ((bytes == NULL && length != 0) || pattern == NULL || *pattern == '\0' ||
        result == NULL || count == NULL) return INVALID_INPUT;
    *result = NULL;
    *count = 0;
    size_t pattern_length = strlen(pattern), used = 0, capacity = 0;
    size_t line = 1, column = 1;
    Match *matches = NULL;
    for (size_t i = 0; i < length; i++)
    {
        size_t j = 0;
        if (pattern_length <= length - i)
            while (j < pattern_length && bytes[i + j] == (unsigned char)pattern[j]) j++;
        if (j == pattern_length)
        {
            if (used == capacity)
            {
                size_t next = capacity == 0 ? 8 : capacity * 2;
                if (capacity > SIZE_MAX / 2 || next > SIZE_MAX / sizeof(*matches))
                { free(matches); return RANGE_ERROR; }
                Match *temporary = realloc(matches, next * sizeof(*matches));
                if (temporary == NULL) { free(matches); return MEMORY_ERROR; }
                matches = temporary;
                capacity = next;
            }
            matches[used++] = (Match){line, column};
        }
        if (bytes[i] == '\n') { line++; column = 1; }
        else column++;
    }
    *result = matches;
    *count = used;
    return OK;
}

Status search_in_files(const char *pattern, FileMatches **result, size_t count, ...)
{
    if (pattern == NULL || *pattern == '\0' || result == NULL || count == 0) return INVALID_INPUT;
    *result = NULL;
    if (count > SIZE_MAX / sizeof(FileMatches)) return RANGE_ERROR;
    FileMatches *files = calloc(count, sizeof(*files));
    if (files == NULL) return MEMORY_ERROR;
    va_list arguments;
    va_start(arguments, count);
    Status status = OK;
    for (size_t i = 0; i < count && status == OK; i++)
    {
        const char *path = va_arg(arguments, const char *);
        files[i].path = path;
        unsigned char *bytes = NULL;
        size_t length = 0;
        files[i].status = read_bytes(path, &bytes, &length);
        if (files[i].status == OK)
            files[i].status = find_matches(bytes, length, pattern, &files[i].matches, &files[i].count);
        free(bytes);
        if (files[i].status == MEMORY_ERROR || files[i].status == RANGE_ERROR)
            status = files[i].status;
    }
    va_end(arguments);
    if (status != OK) { free_search(files, count); return status; }
    *result = files;
    return OK;
}

Status print_search(FILE *output, const FileMatches *files, size_t count)
{
    if (output == NULL || (files == NULL && count != 0)) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++)
    {
        if (files[i].path == NULL || (files[i].matches == NULL && files[i].count != 0)) return INVALID_INPUT;
        if (fprintf(output, "%s:\n", files[i].path) < 0) return FILE_ERROR;
        if (files[i].status != OK)
        {
            if (fprintf(output, "  Ошибка чтения файла (код %d).\n", files[i].status) < 0) return FILE_ERROR;
        }
        else if (files[i].count == 0)
        {
            if (fprintf(output, "  Вхождений нет.\n") < 0) return FILE_ERROR;
        }
        else
            for (size_t j = 0; j < files[i].count; j++)
                if (fprintf(output, "  строка %zu, символ %zu\n", files[i].matches[j].line,
                            files[i].matches[j].column) < 0) return FILE_ERROR;
    }
    return OK;
}

#ifndef LAB_NO_MAIN
int main(int argc, char **argv)
{
    if (argc < 3 || argc > 5)
    {
        fprintf(stderr, "Использование: ./lab13 подстрока файл1 [файл2 [файл3]]\n");
        return 1;
    }
    FileMatches *files = NULL;
    size_t count = (size_t)argc - 2;
    Status status;
    /* C не позволяет превратить argv в переменные аргументы: три демонстрационных вызова. */
    if (count == 1) status = search_in_files(argv[1], &files, count, (const char *)argv[2]);
    else if (count == 2) status = search_in_files(argv[1], &files, count, (const char *)argv[2], (const char *)argv[3]);
    else status = search_in_files(argv[1], &files, count, (const char *)argv[2], (const char *)argv[3], (const char *)argv[4]);
    int failed = status != OK;
    if (status == OK)
    {
        status = print_search(stdout, files, count);
        failed = status != OK;
        for (size_t i = 0; i < count; i++) if (files[i].status != OK) failed = 1;
    }
    else fprintf(stderr, "Не удалось выполнить поиск (код %d).\n", status);
    free_search(files, files == NULL ? 0 : count);
    return failed || fflush(stdout) == EOF;
}
#endif
