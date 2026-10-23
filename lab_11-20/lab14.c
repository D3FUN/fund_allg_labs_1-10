#include "lab_io.h"

typedef struct { char *name; size_t size, importance; } BackupFile;
typedef struct
{
    BackupFile *files;
    size_t count, total_size, total_importance;
} BackupResult;

Status valid_filename(const char *name)
{
    if (name == NULL || *name == '\0') return INVALID_INPUT;
    for (size_t i = 0; name[i] != '\0'; i++)
        if (isspace((unsigned char)name[i])) return INVALID_INPUT;
    return OK;
}

Status free_files(BackupFile *files, size_t count)
{
    if (files == NULL && count != 0) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++) free(files[i].name);
    free(files);
    return OK;
}

Status read_backup(FILE *input, BackupFile **result, size_t *count, size_t *capacity)
{
    if (input == NULL || result == NULL || count == NULL || capacity == NULL) return INVALID_INPUT;
    *result = NULL;
    *count = 0;
    char *line = NULL, *header[2];
    Status status = read_line(input, &line);
    size_t n = 0, limit = 0;
    if (status == OK) status = split_fields(line, header, 2);
    if (status == OK) status = parse_size(header[0], &n);
    if (status == OK) status = parse_size(header[1], &limit);
    free(line);
    if (status != OK) return status == END_OF_FILE ? INVALID_INPUT : status;
    if (n == 0) return INVALID_INPUT;
    if (n > SIZE_MAX / sizeof(BackupFile)) return RANGE_ERROR;
    BackupFile *files = calloc(n, sizeof(*files));
    if (files == NULL) return MEMORY_ERROR;
    for (size_t i = 0; i < n && status == OK; i++)
    {
        char *fields[3];
        status = read_line(input, &line);
        if (status == END_OF_FILE) status = INVALID_INPUT;
        if (status == OK) status = split_fields(line, fields, 3);
        if (status == OK) status = valid_filename(fields[0]);
        if (status == OK) status = parse_size(fields[1], &files[i].size);
        if (status == OK && files[i].size == 0) status = INVALID_INPUT;
        if (status == OK) status = parse_size(fields[2], &files[i].importance);
        if (status == OK) status = copy_text(fields[0], &files[i].name);
        for (size_t j = 0; j < i && status == OK; j++)
            if (strcmp(files[i].name, files[j].name) == 0) status = INVALID_INPUT;
        free(line);
    }
    if (status == OK)
    {
        int c;
        while ((c = fgetc(input)) != EOF)
            if (!isspace((unsigned char)c)) { status = INVALID_INPUT; break; }
        if (ferror(input)) status = FILE_ERROR;
    }
    if (status != OK) { free_files(files, n); return status; }
    *result = files;
    *count = n;
    *capacity = limit;
    return OK;
}

Status choose_backup(const BackupFile *files, size_t count, size_t capacity, BackupResult *result)
{
    if (files == NULL || count == 0 || result == NULL) return INVALID_INPUT;
    *result = (BackupResult){NULL, 0, 0, 0};
    size_t useful_capacity = 0;
    int fits = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (valid_filename(files[i].name) != OK || files[i].size == 0) return INVALID_INPUT;
        for (size_t j = 0; j < i; j++)
            if (strcmp(files[i].name, files[j].name) == 0) return INVALID_INPUT;
        if (files[i].size <= capacity) fits = 1;
        if (files[i].size > capacity - useful_capacity) useful_capacity = capacity;
        else useful_capacity += files[i].size;
    }
    if (!fits || capacity == 0) return OK;
    if (useful_capacity == SIZE_MAX) return RANGE_ERROR;
    size_t width = useful_capacity + 1;
    if (width > SIZE_MAX / sizeof(size_t) || count > SIZE_MAX / width ||
        count > SIZE_MAX / sizeof(BackupFile)) return RANGE_ERROR;
    size_t *best = calloc(width, sizeof(*best));
    unsigned char *take = calloc(count, width);
    BackupFile *selected = malloc(count * sizeof(*selected));
    if (best == NULL || take == NULL || selected == NULL)
    {
        free(best); free(take); free(selected);
        return MEMORY_ERROR;
    }
    Status status = OK;
    for (size_t i = 0; i < count && status == OK; i++)
    {
        if (files[i].size > useful_capacity) continue;
        for (size_t c = useful_capacity; c >= files[i].size; c--)
        {
            size_t previous = best[c - files[i].size];
            if (files[i].importance > SIZE_MAX - previous) { status = RANGE_ERROR; break; }
            size_t candidate = previous + files[i].importance;
            if (candidate > best[c])
            {
                best[c] = candidate;
                take[i * width + c] = 1;
            }
        }
    }
    size_t used = 0, total_size = 0, remaining = useful_capacity;
    if (status == OK)
    {
        for (size_t i = count; i > 0; i--)
            if (take[(i - 1) * width + remaining])
            {
                selected[used++] = files[i - 1];
                remaining -= files[i - 1].size;
                total_size += files[i - 1].size;
            }
        for (size_t i = 0; i < used / 2; i++)
        {
            BackupFile temporary = selected[i];
            selected[i] = selected[used - 1 - i];
            selected[used - 1 - i] = temporary;
        }
        result->files = selected;
        result->count = used;
        result->total_size = total_size;
        result->total_importance = best[useful_capacity];
    }
    else free(selected);
    free(best);
    free(take);
    return status;
}

Status print_backup(FILE *output, const BackupResult *result)
{
    if (output == NULL || result == NULL || (result->files == NULL && result->count != 0)) return INVALID_INPUT;
    if (fprintf(output, "TOTAL_SIZE %zu\nTOTAL_IMPORTANCE %zu\nFILES %zu\n", result->total_size,
                result->total_importance, result->count) < 0) return FILE_ERROR;
    for (size_t i = 0; i < result->count; i++)
    {
        if (valid_filename(result->files[i].name) != OK) return INVALID_INPUT;
        if (fprintf(output, "%s\n", result->files[i].name) < 0) return FILE_ERROR;
    }
    return OK;
}

#ifndef LAB_NO_MAIN
int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) { fprintf(stderr, "Использование: ./lab14 < данные.txt\n"); return 1; }
    BackupFile *files = NULL;
    size_t count = 0, capacity = 0;
    BackupResult result = {NULL, 0, 0, 0};
    Status status = read_backup(stdin, &files, &count, &capacity);
    if (status == OK) status = choose_backup(files, count, capacity, &result);
    if (status == OK) status = print_backup(stdout, &result);
    free(result.files);
    free_files(files, count);
    if (status != OK) fprintf(stderr, "Ошибка данных, памяти или вывода (код %d).\n", status);
    return status != OK || fflush(stdout) == EOF;
}
#endif
