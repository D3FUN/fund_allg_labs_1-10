#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    STATUS_OK,
    STATUS_WRONG_ARGUMENTS,
    STATUS_OPEN_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_WRITE_ERROR
} Status;

static Status read_word(FILE *file, char **word, int *end_of_file)
{
    int symbol;
    size_t length = 0;
    size_t capacity = 16;
    char *text;

    if (file == NULL || word == NULL || end_of_file == NULL)
    {
        return STATUS_OPEN_ERROR;
    }

    *word = NULL;
    *end_of_file = 0;
    do
    {
        symbol = fgetc(file);
    }
    while (symbol != EOF && isspace((unsigned char)symbol));

    if (symbol == EOF)
    {
        *end_of_file = 1;
        return ferror(file) ? STATUS_OPEN_ERROR : STATUS_OK;
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

static Status write_word(FILE *file, const char *word, int *is_first_word)
{
    if (file == NULL || word == NULL || is_first_word == NULL)
    {
        return STATUS_WRITE_ERROR;
    }
    if (!*is_first_word)
    {
        if (fputc(' ', file) == EOF)
        {
            return STATUS_WRITE_ERROR;
        }
    }
    if (fputs(word, file) == EOF)
    {
        return STATUS_WRITE_ERROR;
    }
    *is_first_word = 0;
    return STATUS_OK;
}

static Status mix_files(FILE *first_file, FILE *second_file, FILE *result_file)
{
    int first_end = 0;
    int second_end = 0;
    int is_first_word = 1;
    Status status = STATUS_OK;

    while (!first_end || !second_end)
    {
        char *word = NULL;

        if (!first_end)
        {
            status = read_word(first_file, &word, &first_end);
            if (status == STATUS_OK && !first_end)
            {
                status = write_word(result_file, word, &is_first_word);
            }
            free(word);
            if (status != STATUS_OK)
            {
                return status;
            }
        }

        if (!second_end)
        {
            status = read_word(second_file, &word, &second_end);
            if (status == STATUS_OK && !second_end)
            {
                status = write_word(result_file, word, &is_first_word);
            }
            free(word);
            if (status != STATUS_OK)
            {
                return status;
            }
        }
    }
    return STATUS_OK;
}

static void write_number_in_base(FILE *file, unsigned int number, int base)
{
    const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char result[16];
    int length = 0;

    if (number == 0)
    {
        fputc('0', file);
    }
    else
    {
        while (number > 0)
        {
            result[length] = digits[number % (unsigned int)base];
            length++;
            number /= (unsigned int)base;
        }
        while (length > 0)
        {
            length--;
            fputc(result[length], file);
        }
    }
}

static Status write_changed_word(FILE *file, const char *word, long word_number,
                                 int *is_first_word)
{
    size_t i;
    int base = 0;

    if (file == NULL || word == NULL || is_first_word == NULL)
    {
        return STATUS_WRITE_ERROR;
    }
    if (!*is_first_word && fputc(' ', file) == EOF)
    {
        return STATUS_WRITE_ERROR;
    }

    if (word_number % 10 == 0)
    {
        base = 4;
    }
    else if (word_number % 5 == 0)
    {
        base = 8;
    }

    for (i = 0; word[i] != '\0'; i++)
    {
        unsigned char symbol = (unsigned char)word[i];
        if ((word_number % 2 == 0 && word_number % 10 != 0) &&
            symbol >= 'A' && symbol <= 'Z')
        {
            symbol = (unsigned char)tolower(symbol);
        }
        if (base != 0)
        {
            write_number_in_base(file, (unsigned int)symbol, base);
        }
        else if (fputc(symbol, file) == EOF)
        {
            return STATUS_WRITE_ERROR;
        }
    }
    if (ferror(file))
    {
        return STATUS_WRITE_ERROR;
    }
    *is_first_word = 0;
    return STATUS_OK;
}

static Status change_file(FILE *input_file, FILE *result_file)
{
    int end_of_file = 0;
    int is_first_word = 1;
    long word_number = 0;
    Status status;

    while (!end_of_file)
    {
        char *word = NULL;
        status = read_word(input_file, &word, &end_of_file);
        if (status != STATUS_OK)
        {
            free(word);
            return status;
        }
        if (!end_of_file)
        {
            word_number++;
            status = write_changed_word(result_file, word, word_number, &is_first_word);
            free(word);
            if (status != STATUS_OK)
            {
                return status;
            }
        }
    }
    return STATUS_OK;
}

static void print_error(Status status)
{
    if (status == STATUS_OPEN_ERROR)
    {
        printf("Ошибка: не удалось открыть один из файлов.\n");
    }
    else if (status == STATUS_MEMORY_ERROR)
    {
        printf("Ошибка: не удалось выделить память для лексемы.\n");
    }
    else if (status == STATUS_WRITE_ERROR)
    {
        printf("Ошибка: не удалось записать данные в выходной файл.\n");
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
    FILE *first_file;
    FILE *second_file;
    FILE *result_file;
    Status status;

    if (argc < 2)
    {
        printf("Использование: %s -r файл1 файл2 результат\n", argv[0]);
        printf("или: %s -a входной_файл результат\n", argv[0]);
        return STATUS_WRONG_ARGUMENTS;
    }
    if ((strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "/r") == 0) && argc == 5)
    {
        first_file = fopen(argv[2], "r");
        second_file = fopen(argv[3], "r");
        result_file = NULL;
        if (first_file != NULL && second_file != NULL &&
            !same_file(argv[2], argv[4]) && !same_file(argv[3], argv[4]))
        {
            result_file = fopen(argv[4], "w");
        }
        if (first_file == NULL || second_file == NULL || result_file == NULL)
        {
            if (first_file != NULL)
            {
                fclose(first_file);
            }
            if (second_file != NULL)
            {
                fclose(second_file);
            }
            if (result_file != NULL)
            {
                fclose(result_file);
            }
            print_error(STATUS_OPEN_ERROR);
            return STATUS_OPEN_ERROR;
        }
        status = mix_files(first_file, second_file, result_file);
        fclose(first_file);
        fclose(second_file);
        if (fclose(result_file) == EOF && status == STATUS_OK)
        {
            status = STATUS_WRITE_ERROR;
        }
    }
    else if ((strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "/a") == 0) && argc == 4)
    {
        first_file = fopen(argv[2], "r");
        result_file = NULL;
        if (first_file != NULL && !same_file(argv[2], argv[3]))
        {
            result_file = fopen(argv[3], "w");
        }
        if (first_file == NULL || result_file == NULL)
        {
            if (first_file != NULL)
            {
                fclose(first_file);
            }
            if (result_file != NULL)
            {
                fclose(result_file);
            }
            print_error(STATUS_OPEN_ERROR);
            return STATUS_OPEN_ERROR;
        }
        status = change_file(first_file, result_file);
        fclose(first_file);
        if (fclose(result_file) == EOF && status == STATUS_OK)
        {
            status = STATUS_WRITE_ERROR;
        }
    }
    else
    {
        printf("Ошибка: неверный флаг или количество аргументов.\n");
        printf("Для -r нужно 3 пути к файлам, для -a нужно 2 пути к файлам.\n");
        return STATUS_WRONG_ARGUMENTS;
    }

    if (status != STATUS_OK)
    {
        print_error(status);
        return status;
    }
    printf("Готово: результат успешно записан в выходной файл.\n");
    return STATUS_OK;
}
