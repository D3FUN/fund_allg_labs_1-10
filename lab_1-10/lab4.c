#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef enum
{
    STATUS_OK,
    STATUS_WRONG_ARGUMENTS,
    STATUS_WRONG_FLAG,
    STATUS_MEMORY_ERROR,
    STATUS_FILE_ERROR,
    STATUS_WRITE_ERROR
} Status;

static int is_latin_letter(int symbol)
{
    if ((symbol >= 'A' && symbol <= 'Z') || (symbol >= 'a' && symbol <= 'z'))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int is_digit(int symbol)
{
    if (symbol >= '0' && symbol <= '9')
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static Status make_output_name(const char *input_name, char **output_name)
{
    const char *last_slash;
    size_t folder_length;
    size_t name_length;
    char *result;

    if (input_name == NULL || output_name == NULL)
    {
        return STATUS_WRONG_ARGUMENTS;
    }
    last_slash = strrchr(input_name, '/');
    if (last_slash == NULL)
    {
        folder_length = 0;
        name_length = strlen(input_name);
    }
    else
    {
        folder_length = (size_t)(last_slash - input_name + 1);
        name_length = strlen(last_slash + 1);
    }
    result = malloc(folder_length + 4 + name_length + 1);
    if (result == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    if (folder_length > 0)
    {
        memcpy(result, input_name, folder_length);
    }
    memcpy(result + folder_length, "out_", 4);
    memcpy(result + folder_length + 4, input_name + folder_length, name_length + 1);
    *output_name = result;
    return STATUS_OK;
}

static Status remove_digits(FILE *input, FILE *output)
{
    int symbol;
    while ((symbol = fgetc(input)) != EOF)
    {
        if (!is_digit(symbol))
        {
            if (fputc(symbol, output) == EOF)
            {
                return STATUS_WRITE_ERROR;
            }
        }
    }
    if (ferror(input))
    {
        return STATUS_FILE_ERROR;
    }
    return STATUS_OK;
}

static Status count_symbols_in_lines(FILE *input, FILE *output, char mode)
{
    int symbol;
    unsigned long long count = 0;
    int has_symbols = 0;
    while ((symbol = fgetc(input)) != EOF)
    {
        if (symbol == '\n')
        {
            if (fprintf(output, "%llu\n", count) < 0)
            {
                return STATUS_WRITE_ERROR;
            }
            count = 0;
            has_symbols = 0;
        }
        else
        {
            has_symbols = 1;
            if (mode == 'i' && is_latin_letter(symbol))
            {
                count++;
            }
            else if (mode == 's' && !is_latin_letter(symbol) && !is_digit(symbol) && symbol != ' ')
            {
                count++;
            }
            else
            {
            }
        }
    }
    if (ferror(input))
    {
        return STATUS_FILE_ERROR;
    }
    if (has_symbols)
    {
        if (fprintf(output, "%llu\n", count) < 0)
        {
            return STATUS_WRITE_ERROR;
        }
    }
    return STATUS_OK;
}

static Status replace_symbols_by_hex(FILE *input, FILE *output)
{
    int symbol;
    while ((symbol = fgetc(input)) != EOF)
    {
        if (is_digit(symbol))
        {
            if (fputc(symbol, output) == EOF)
            {
                return STATUS_WRITE_ERROR;
            }
        }
        else
        {
            if (fprintf(output, "%X", (unsigned char)symbol) < 0)
            {
                return STATUS_WRITE_ERROR;
            }
        }
    }
    if (ferror(input))
    {
        return STATUS_FILE_ERROR;
    }
    return STATUS_OK;
}

static Status process_file(FILE *input, FILE *output, char action)
{
    if (action == 'd')
    {
        return remove_digits(input, output);
    }
    else if (action == 'i' || action == 's')
    {
        return count_symbols_in_lines(input, output, action);
    }
    else if (action == 'a')
    {
        return replace_symbols_by_hex(input, output);
    }
    else
    {
        return STATUS_WRONG_FLAG;
    }
}

int main(int argc, char *argv[])
{
    const char *input_name;
    const char *output_name;
    char *generated_name = NULL;
    char action;
    int has_output_name = 0;
    FILE *input;
    FILE *output;
    Status status;
    struct stat input_info;
    struct stat output_info;
    if (argc < 3)
    {
        printf("Ошибка: недостаточно аргументов.\n");
        return 1;
    }
    if (argv[1][0] != '-' && argv[1][0] != '/')
    {
        printf("Ошибка: флаг должен начинаться с символа '-' или '/'.\n");
        return 1;
    }
    if (argv[1][1] == 'n')
    {
        has_output_name = 1;
        if (strlen(argv[1]) != 3 || argc != 4)
        {
            printf("Ошибка: для флага с n нужны входной и выходной файлы.\n");
            return 1;
        }
        action = argv[1][2];
    }
    else
    {
        if (strlen(argv[1]) != 2 || argc != 3)
        {
            printf("Ошибка: неверное количество аргументов или формат флага.\n");
            return 1;
        }
        action = argv[1][1];
    }
    if (action != 'd' && action != 'i' && action != 's' && action != 'a')
    {
        printf("Ошибка: неизвестный флаг.\n");
        return 1;
    }
    input_name = argv[2];
    if (has_output_name)
    {
        output_name = argv[3];
    }
    else
    {
        status = make_output_name(input_name, &generated_name);
        if (status != STATUS_OK)
        {
            printf("Ошибка: не удалось выделить память для имени выходного файла.\n");
            return 1;
        }
        output_name = generated_name;
    }
    /* Different paths and links can refer to the same existing file. */
    if (strcmp(input_name, output_name) == 0 ||
        (stat(input_name, &input_info) == 0 && stat(output_name, &output_info) == 0 &&
         input_info.st_dev == output_info.st_dev && input_info.st_ino == output_info.st_ino))
    {
        printf("Ошибка: входной и выходной файлы не должны совпадать.\n");
        free(generated_name);
        return 1;
    }
    input = fopen(input_name, "rb");
    if (input == NULL)
    {
        printf("Ошибка: не удалось открыть входной файл '%s'.\n", input_name);
        free(generated_name);
        return 1;
    }
    output = fopen(output_name, "wb");
    if (output == NULL)
    {
        printf("Ошибка: не удалось открыть выходной файл '%s'.\n", output_name);
        fclose(input);
        free(generated_name);
        return 1;
    }
    status = process_file(input, output, action);
    if (fclose(input) != 0 && status == STATUS_OK)
    {
        status = STATUS_FILE_ERROR;
    }
    if (fclose(output) != 0 && status == STATUS_OK)
    {
        status = STATUS_WRITE_ERROR;
    }
    if (status != STATUS_OK)
    {
        printf("Ошибка: не удалось полностью обработать файл.\n");
        free(generated_name);
        return 1;
    }
    printf("Готово. Результат записан в файл '%s'.\n", output_name);
    free(generated_name);
    return 0;
}
