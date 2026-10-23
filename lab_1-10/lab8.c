#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>


#define BIG_BASE 1000000000U

typedef enum
{
    STATUS_OK,
    STATUS_WRONG_ARGUMENTS,
    STATUS_OPEN_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_INVALID_NUMBER,
    STATUS_WRITE_ERROR
} Status;

typedef struct
{
    unsigned int *digits;
    size_t size;
    size_t capacity;
} BigNumber;

static void free_big_number(BigNumber *number)
{
    if (number != NULL)
    {
        free(number->digits);
        number->digits = NULL;
        number->size = 0;
        number->capacity = 0;
    }
}

static Status make_big_number(BigNumber *number)
{
    if (number == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }

    number->digits = malloc(sizeof(unsigned int));
    if (number->digits == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    number->digits[0] = 0;
    number->size = 1;
    number->capacity = 1;
    return STATUS_OK;
}

static Status multiply_and_add(BigNumber *number, unsigned int multiplier, unsigned int add)
{
    size_t i;
    unsigned long long carry = add;

    if (number == NULL || number->digits == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }

    for (i = 0; i < number->size; i++)
    {
        unsigned long long value = (unsigned long long)number->digits[i] * multiplier + carry;
        number->digits[i] = (unsigned int)(value % BIG_BASE);
        carry = value / BIG_BASE;
    }

    while (carry > 0)
    {
        if (number->size == number->capacity)
        {
            size_t new_capacity = number->capacity * 2;
            unsigned int *new_digits;

            if (number->capacity > SIZE_MAX / 2 / sizeof(unsigned int))
            {
                return STATUS_MEMORY_ERROR;
            }
            new_digits = realloc(number->digits, new_capacity * sizeof(unsigned int));
            if (new_digits == NULL)
            {
                return STATUS_MEMORY_ERROR;
            }
            number->digits = new_digits;
            number->capacity = new_capacity;
        }
        number->digits[number->size] = (unsigned int)(carry % BIG_BASE);
        number->size++;
        carry /= BIG_BASE;
    }
    return STATUS_OK;
}

static int symbol_value(char symbol)
{
    if (symbol >= '0' && symbol <= '9')
    {
        return symbol - '0';
    }
    if (symbol >= 'A' && symbol <= 'Z')
    {
        return symbol - 'A' + 10;
    }
    if (symbol >= 'a' && symbol <= 'z')
    {
        return symbol - 'a' + 10;
    }
    return -1;
}


static Status read_word(FILE *file, char **word)
{
    int symbol;
    size_t length = 0;
    size_t capacity = 16;
    char *text;

    if (file == NULL || word == NULL)
    {
        return STATUS_OPEN_ERROR;
    }

    *word = NULL;
    do
    {
        symbol = fgetc(file);
    }
    while (symbol != EOF && isspace((unsigned char)symbol));

    if (symbol == EOF)
    {
        if (ferror(file))
        {
            return STATUS_OPEN_ERROR;
        }
        return STATUS_OK;
    }

    text = malloc(capacity);
    if (text == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }

    while (symbol != EOF && !isspace((unsigned char)symbol))
    {
        if (length + 1 >= capacity)
        {
            char *new_text;
            if (capacity > SIZE_MAX / 2)
            {
                free(text);
                return STATUS_MEMORY_ERROR;
            }
            capacity *= 2;
            new_text = realloc(text, capacity);
            if (new_text == NULL)
            {
                free(text);
                return STATUS_MEMORY_ERROR;
            }
            text = new_text;
        }
        text[length] = (char)symbol;
        length++;
        symbol = fgetc(file);
    }
    if (ferror(file))
    {
        free(text);
        return STATUS_OPEN_ERROR;
    }
    text[length] = '\0';
    *word = text;
    return STATUS_OK;
}

static Status write_decimal(FILE *file, const BigNumber *number)
{
    size_t i;

    if (file == NULL || number == NULL || number->digits == NULL)
    {
        return STATUS_WRITE_ERROR;
    }
    if (fprintf(file, "%u", number->digits[number->size - 1]) < 0)
    {
        return STATUS_WRITE_ERROR;
    }
    for (i = number->size - 1; i > 0; i--)
    {
        if (fprintf(file, "%09u", number->digits[i - 1]) < 0)
        {
            return STATUS_WRITE_ERROR;
        }
    }
    return STATUS_OK;
}

static Status process_number(FILE *output, const char *word)
{
    size_t i;
    size_t first_nonzero = 0;
    unsigned int base = 2;
    BigNumber decimal;
    Status status;

    if (output == NULL || word == NULL || word[0] == '\0')
    {
        return STATUS_INVALID_NUMBER;
    }

    while (word[first_nonzero] == '0')
    {
        first_nonzero++;
    }
    if (word[first_nonzero] == '\0')
    {
        first_nonzero--;
    }

    for (i = first_nonzero; word[i] != '\0'; i++)
    {
        int value = symbol_value(word[i]);
        if (value < 0 || value >= 36)
        {
            return STATUS_INVALID_NUMBER;
        }
        if ((unsigned int)(value + 1) > base)
        {
            base = (unsigned int)(value + 1);
        }
    }

    status = make_big_number(&decimal);
    if (status != STATUS_OK)
    {
        return status;
    }
    for (i = first_nonzero; word[i] != '\0'; i++)
    {
        status = multiply_and_add(&decimal, base, (unsigned int)symbol_value(word[i]));
        if (status != STATUS_OK)
        {
            free_big_number(&decimal);
            return status;
        }
    }

    for (i = first_nonzero; word[i] != '\0'; i++)
    {
        if (fputc((int)toupper((unsigned char)word[i]), output) == EOF)
        {
            free_big_number(&decimal);
            return STATUS_WRITE_ERROR;
        }
    }
    if (fprintf(output, " %u ", base) < 0)
    {
        free_big_number(&decimal);
        return STATUS_WRITE_ERROR;
    }
    status = write_decimal(output, &decimal);
    free_big_number(&decimal);
    if (status != STATUS_OK)
    {
        return status;
    }
    if (fputc('\n', output) == EOF)
    {
        return STATUS_WRITE_ERROR;
    }
    return STATUS_OK;
}

static void print_error(Status status, const char *word)
{
    if (status == STATUS_INVALID_NUMBER)
    {
        fprintf(stderr, "Ошибка: некорректное число \"%s\".\n", word);
    }
    else if (status == STATUS_MEMORY_ERROR)
    {
        fprintf(stderr, "Ошибка: не удалось выделить память.\n");
    }
    else if (status == STATUS_OPEN_ERROR)
    {
        fprintf(stderr, "Ошибка: не удалось прочитать входной файл.\n");
    }
    else if (status == STATUS_WRITE_ERROR)
    {
        fprintf(stderr, "Ошибка: не удалось записать выходной файл.\n");
    }
}

static int same_file(const char *first, const char *second)
{
    struct stat first_info;
    struct stat second_info;
    return strcmp(first, second) == 0 ||
           (stat(first, &first_info) == 0 && stat(second, &second_info) == 0 &&
            first_info.st_dev == second_info.st_dev && first_info.st_ino == second_info.st_ino);
}

int main(int argc, char *argv[])
{
    FILE *input;
    FILE *output;
    Status status = STATUS_OK;

    if (argc != 3)
    {
        fprintf(stderr, "Использование: %s <входной_файл> <выходной_файл>\n", argv[0]);
        return STATUS_WRONG_ARGUMENTS;
    }

    input = fopen(argv[1], "r");
    if (input == NULL)
    {
        fprintf(stderr, "Ошибка: не удалось открыть входной файл.\n");
        return STATUS_OPEN_ERROR;
    }
    output = same_file(argv[1], argv[2]) ? NULL : fopen(argv[2], "w");
    if (output == NULL)
    {
        fclose(input);
        fprintf(stderr, "Ошибка: не удалось открыть выходной файл.\n");
        return STATUS_OPEN_ERROR;
    }

    while (status == STATUS_OK)
    {
        char *word = NULL;
        status = read_word(input, &word);
        if (status == STATUS_OK && word != NULL)
        {
            status = process_number(output, word);
            if (status != STATUS_OK)
            {
                print_error(status, word);
            }
        }
        if (word == NULL)
        {
            break;
        }
        free(word);
    }

    if (fclose(input) != 0 && status == STATUS_OK)
    {
        status = STATUS_OPEN_ERROR;
    }
    if (fclose(output) != 0 && status == STATUS_OK)
    {
        status = STATUS_WRITE_ERROR;
    }
    if (status != STATUS_OK)
    {
        if (status != STATUS_INVALID_NUMBER)
        {
            print_error(status, "");
        }
        return status;
    }

    printf("Готово: результаты записаны в файл %s\n", argv[2]);
    return STATUS_OK;
}
