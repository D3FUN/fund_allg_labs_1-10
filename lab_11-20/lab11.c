#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum { OK, INVALID_INPUT, MEMORY_ERROR, RANGE_ERROR } Status;

Status string_length(const char *text, size_t *length)
{
    if (text == NULL || length == NULL) return INVALID_INPUT;
    size_t n = 0;
    while (text[n] != '\0') n++;
    *length = n;
    return OK;
}

Status reverse_string(const char *text, char **result)
{
    size_t n;
    if (result == NULL || string_length(text, &n) != OK) return INVALID_INPUT;
    *result = NULL;
    if (n == SIZE_MAX) return RANGE_ERROR;
    char *copy = malloc(n + 1);
    if (copy == NULL) return MEMORY_ERROR;
    for (size_t i = 0; i < n; i++) copy[i] = text[n - 1 - i];
    copy[n] = '\0';
    *result = copy;
    return OK;
}

Status uppercase_odd(const char *text, char **result)
{
    size_t n;
    if (result == NULL || string_length(text, &n) != OK) return INVALID_INPUT;
    *result = NULL;
    if (n == SIZE_MAX) return RANGE_ERROR;
    char *copy = malloc(n + 1);
    if (copy == NULL) return MEMORY_ERROR;
    for (size_t i = 0; i < n; i++)
        copy[i] = i % 2 == 1 ? (char)toupper((unsigned char)text[i]) : text[i];
    copy[n] = '\0';
    *result = copy;
    return OK;
}

Status group_characters(const char *text, char **result)
{
    size_t n;
    if (result == NULL || string_length(text, &n) != OK) return INVALID_INPUT;
    *result = NULL;
    if (n == SIZE_MAX) return RANGE_ERROR;
    char *copy = malloc(n + 1);
    if (copy == NULL) return MEMORY_ERROR;
    size_t position = 0;
    for (int group = 0; group < 3; group++)
    {
        for (size_t i = 0; i < n; i++)
        {
            unsigned char c = (unsigned char)text[i];
            int category = isdigit(c) ? 0 : (isalpha(c) ? 1 : 2);
            if (category == group) copy[position++] = text[i];
        }
    }
    copy[position] = '\0';
    *result = copy;
    return OK;
}

Status concatenate_random(const char *const *strings, size_t count,
                          unsigned int seed, char **result)
{
    if (strings == NULL || count == 0 || result == NULL) return INVALID_INPUT;
    *result = NULL;
    if (count > SIZE_MAX / sizeof(size_t)) return RANGE_ERROR;
    size_t total = 0;
    for (size_t i = 0; i < count; i++)
    {
        size_t n;
        if (string_length(strings[i], &n) != OK) return INVALID_INPUT;
        if (n > SIZE_MAX - 1 - total) return RANGE_ERROR;
        total += n;
    }
    size_t *order = malloc(count * sizeof(*order));
    char *copy = malloc(total + 1);
    if (order == NULL || copy == NULL)
    {
        free(order);
        free(copy);
        return MEMORY_ERROR;
    }
    for (size_t i = 0; i < count; i++) order[i] = i;
    srand(seed);
    for (size_t i = count; i > 1; i--)
    {
        size_t j = (size_t)rand() % i;
        size_t temporary = order[i - 1];
        order[i - 1] = order[j];
        order[j] = temporary;
    }
    size_t position = 0;
    for (size_t i = 0; i < count; i++)
        for (size_t j = 0; strings[order[i]][j] != '\0'; j++)
            copy[position++] = strings[order[i]][j];
    copy[position] = '\0';
    free(order);
    *result = copy;
    return OK;
}

Status parse_seed(const char *text, unsigned int *seed)
{
    if (text == NULL || seed == NULL || *text == '\0') return INVALID_INPUT;
    unsigned int value = 0;
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] < '0' || text[i] > '9') return INVALID_INPUT;
        unsigned int digit = (unsigned int)(text[i] - '0');
        if (value > (UINT_MAX - digit) / 10) return RANGE_ERROR;
        value = value * 10 + digit;
    }
    *seed = value;
    return OK;
}

#ifndef LAB_NO_MAIN
int main(int argc, char **argv)
{
    if (argc < 3 || argv[1][0] != '-' || argv[1][1] == '\0' || argv[1][2] != '\0')
    {
        fprintf(stderr, "Использование: ./lab11 -l|-r|-u|-n строка; ./lab11 -c строка seed [строки...]\n");
        return 1;
    }
    char action = argv[1][1];
    if ((action == 'c' && argc < 4) || (action != 'c' && argc != 3))
    {
        fprintf(stderr, "Неверное количество аргументов.\n");
        return 1;
    }
    Status status = INVALID_INPUT;
    char *result = NULL;
    if (action == 'l')
    {
        size_t length;
        status = string_length(argv[2], &length);
        if (status == OK && printf("%zu\n", length) < 0) return 1;
    }
    else if (action == 'r') status = reverse_string(argv[2], &result);
    else if (action == 'u') status = uppercase_odd(argv[2], &result);
    else if (action == 'n') status = group_characters(argv[2], &result);
    else if (action == 'c')
    {
        unsigned int seed;
        status = parse_seed(argv[3], &seed);
        if (status == OK)
        {
            size_t count = (size_t)argc - 3;
            const char **strings = malloc(count * sizeof(*strings));
            if (strings == NULL) status = MEMORY_ERROR;
            else
            {
                strings[0] = argv[2];
                for (size_t i = 1; i < count; i++) strings[i] = argv[i + 3];
                status = concatenate_random(strings, count, seed, &result);
                free(strings);
            }
        }
    }
    if (status != OK)
    {
        fprintf(stderr, "Ошибка: %s.\n", status == MEMORY_ERROR ? "не хватает памяти" :
                status == RANGE_ERROR ? "слишком большое значение" : "неверные данные или флаг");
        free(result);
        return 1;
    }
    int failed = result != NULL && puts(result) == EOF;
    free(result);
    return failed || fflush(stdout) == EOF;
}
#endif
