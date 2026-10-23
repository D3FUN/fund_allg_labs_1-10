#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

typedef enum
{
    STATUS_OK,
    STATUS_INVALID_INPUT,
    STATUS_MEMORY_ERROR,
    STATUS_RANGE_ERROR
} Status;

Status digit_value(char symbol, int *value)
{
    if (value == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    if (symbol >= '0' && symbol <= '9')
    {
        *value = symbol - '0';
    }
    else if (symbol >= 'A' && symbol <= 'Z')
    {
        *value = symbol - 'A' + 10;
    }
    else if (symbol >= 'a' && symbol <= 'z')
    {
        *value = symbol - 'a' + 10;
    }
    else
    {
        return STATUS_INVALID_INPUT;
    }
    return STATUS_OK;
}

Status check_number(const char *text, int base)
{
    if (text == NULL || text[0] == '\0' || base < 2 || base > 36)
    {
        return STATUS_INVALID_INPUT;
    }
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        int digit = 0;
        if (digit_value(text[i], &digit) != STATUS_OK || digit >= base)
        {
            return STATUS_INVALID_INPUT;
        }
    }
    return STATUS_OK;
}

Status add_columns(int base, const char *first, const char *second, char **result)
{
    if (result == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    *result = NULL;
    if (check_number(first, base) != STATUS_OK ||
        check_number(second, base) != STATUS_OK)
    {
        return STATUS_INVALID_INPUT;
    }
    size_t first_length = strlen(first);
    size_t second_length = strlen(second);
    size_t length = first_length > second_length ? first_length : second_length;
    if (length > SIZE_MAX - 2)
    {
        return STATUS_RANGE_ERROR;
    }
    char *sum = malloc(length + 2);
    if (sum == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t position = length + 1;
    sum[position] = '\0';
    int carry = 0;
    while (first_length > 0 || second_length > 0 || carry != 0)
    {
        int left = 0;
        int right = 0;
        if (first_length > 0)
        {
            first_length--;
            (void)digit_value(first[first_length], &left);
        }
        if (second_length > 0)
        {
            second_length--;
            (void)digit_value(second[second_length], &right);
        }
        int current = left + right + carry;
        position--;
        sum[position] = digits[current % base];
        carry = current / base;
    }
    while (sum[position] == '0' && sum[position + 1] != '\0')
    {
        position++;
    }
    memmove(sum, sum + position, length + 2 - position);
    *result = sum;
    return STATUS_OK;
}

/* After count, pass exactly count strings. The caller frees *result. */
Status sum_numbers(int base, size_t count, char **result, ...)
{
    if (result == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    *result = NULL;
    if (base < 2 || base > 36)
    {
        return STATUS_INVALID_INPUT;
    }
    char *sum = malloc(2);
    if (sum == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    sum[0] = '0';
    sum[1] = '\0';
    va_list arguments;
    va_start(arguments, result);
    Status status = STATUS_OK;
    for (size_t i = 0; i < count && status == STATUS_OK; i++)
    {
        const char *number = va_arg(arguments, const char *);
        char *next_sum = NULL;
        status = add_columns(base, sum, number, &next_sum);
        free(sum);
        sum = next_sum;
    }
    va_end(arguments);
    if (status != STATUS_OK)
    {
        free(sum);
        return status;
    }
    *result = sum;
    return STATUS_OK;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    char *sum = NULL;
    Status status = sum_numbers(16, 3, &sum,
                               "FFFFFFFFFFFFFFFFFFFFFFFF", "1", "A");
    if (status != STATUS_OK)
    {
        fprintf(stderr, "Sum error: %d\n", status);
        return 1;
    }
    printf("Sum in base 16: %s\n", sum);
    free(sum);
    return 0;
}
#endif
