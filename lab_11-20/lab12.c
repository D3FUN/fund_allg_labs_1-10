#include "lab_io.h"
#include <sys/stat.h>

typedef struct
{
    unsigned int id;
    char *name;
    char *surname;
    double salary;
} Employee;

Status valid_name(const char *name)
{
    if (name == NULL || *name == '\0') return INVALID_INPUT;
    for (size_t i = 0; name[i] != '\0'; i++)
        if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z')))
            return INVALID_INPUT;
    return OK;
}

Status parse_salary(const char *text, double *result)
{
    if (text == NULL || result == NULL || *text == '\0') return INVALID_INPUT;
    const char *p = text;
    if (*p == '+') p++;
    size_t digits = 0;
    while (*p >= '0' && *p <= '9') { p++; digits++; }
    if (*p == '.')
    {
        p++;
        while (*p >= '0' && *p <= '9') { p++; digits++; }
    }
    if (digits == 0) return INVALID_INPUT;
    if (*p == 'e' || *p == 'E')
    {
        p++;
        if (*p == '+' || *p == '-') p++;
        const char *start = p;
        while (*p >= '0' && *p <= '9') p++;
        if (p == start) return INVALID_INPUT;
    }
    if (*p != '\0') return INVALID_INPUT;
    char *end;
    double value = strtod(text, &end);
    if (*end != '\0' || !isfinite(value)) return RANGE_ERROR;
    *result = value;
    return OK;
}

Status free_employees(Employee *employees, size_t count)
{
    if (employees == NULL && count != 0) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++)
    {
        free(employees[i].name);
        free(employees[i].surname);
    }
    free(employees);
    return OK;
}

Status read_employees(FILE *file, Employee **result, size_t *count)
{
    if (file == NULL || result == NULL || count == NULL) return INVALID_INPUT;
    *result = NULL;
    *count = 0;
    Employee *employees = NULL;
    size_t used = 0, capacity = 0;
    Status status;
    char *line;
    while ((status = read_line(file, &line)) == OK)
    {
        char *fields[4];
        size_t id = 0;
        double salary = 0.0;
        status = split_fields(line, fields, 4);
        if (status == OK) status = parse_size(fields[0], &id);
        if (status == OK && id > UINT_MAX) status = RANGE_ERROR;
        if (status == OK) status = valid_name(fields[1]);
        if (status == OK) status = valid_name(fields[2]);
        if (status == OK) status = parse_salary(fields[3], &salary);
        if (status == OK && used == capacity)
        {
            size_t next = capacity == 0 ? 8 : capacity * 2;
            if (capacity > SIZE_MAX / 2 || next > SIZE_MAX / sizeof(*employees)) status = RANGE_ERROR;
            else
            {
                Employee *temporary = realloc(employees, next * sizeof(*employees));
                if (temporary == NULL) status = MEMORY_ERROR;
                else { employees = temporary; capacity = next; }
            }
        }
        Employee item = {0, NULL, NULL, 0.0};
        if (status == OK) status = copy_text(fields[1], &item.name);
        if (status == OK) status = copy_text(fields[2], &item.surname);
        if (status == OK)
        {
            item.id = (unsigned int)id;
            item.salary = salary;
            employees[used++] = item;
        }
        else { free(item.name); free(item.surname); }
        free(line);
        if (status != OK) break;
    }
    if (status != END_OF_FILE)
    {
        free_employees(employees, used);
        return status;
    }
    *result = employees;
    *count = used;
    return OK;
}

Status compare_salary(double first, double second, double epsilon, int *order)
{
    if (!isfinite(first) || !isfinite(second) || !isfinite(epsilon) || epsilon < 0.0 || order == NULL)
        return INVALID_INPUT;
    *order = fabs(first - second) <= epsilon ? 0 : (first < second ? -1 : 1);
    return OK;
}

int salary_order(const void *left, const void *right)
{
    const Employee *a = left, *b = right;
    int order = 0;
    /* Для qsort нужен транзитивный порядок: сначала точные зарплаты. */
    compare_salary(a->salary, b->salary, 0.0, &order);
    return order;
}

int name_order(const void *left, const void *right)
{
    const Employee *a = left, *b = right;
    int order = strcmp(a->surname, b->surname);
    if (order == 0) order = strcmp(a->name, b->name);
    if (order == 0) order = (a->id > b->id) - (a->id < b->id);
    return order;
}

Status sort_employees(Employee *employees, size_t count, int descending, double epsilon)
{
    if ((employees == NULL && count != 0) || (descending != 0 && descending != 1) ||
        !isfinite(epsilon) || epsilon < 0.0) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++)
        if (!isfinite(employees[i].salary) || employees[i].salary < 0.0 ||
            valid_name(employees[i].name) != OK || valid_name(employees[i].surname) != OK)
            return INVALID_INPUT;
    if (count < 2) return OK;
    qsort(employees, count, sizeof(*employees), salary_order);
    size_t start = 0;
    while (start < count)
    {
        size_t end = start + 1;
        while (end < count)
        {
            int order;
            compare_salary(employees[end].salary, employees[start].salary, epsilon, &order);
            if (order != 0) break;
            end++;
        }
        qsort(employees + start, end - start, sizeof(*employees), name_order);
        start = end;
    }
    if (descending)
        for (size_t i = 0; i < count / 2; i++)
        {
            Employee temporary = employees[i];
            employees[i] = employees[count - 1 - i];
            employees[count - 1 - i] = temporary;
        }
    return OK;
}

Status write_employees(FILE *file, const Employee *employees, size_t count)
{
    if (file == NULL || (employees == NULL && count != 0)) return INVALID_INPUT;
    for (size_t i = 0; i < count; i++)
    {
        if (valid_name(employees[i].name) != OK || valid_name(employees[i].surname) != OK ||
            !isfinite(employees[i].salary) || employees[i].salary < 0.0) return INVALID_INPUT;
        if (fprintf(file, "%u %s %s %.17g\n", employees[i].id, employees[i].name,
                    employees[i].surname, employees[i].salary) < 0) return FILE_ERROR;
    }
    return OK;
}

#ifndef LAB_NO_MAIN
int main(int argc, char **argv)
{
    if (argc != 4 || strlen(argv[2]) != 2 || (argv[2][0] != '-' && argv[2][0] != '/') ||
        (argv[2][1] != 'a' && argv[2][1] != 'd'))
    {
        fprintf(stderr, "Использование: ./lab12 вход.txt -a|-d выход.txt\n");
        return 1;
    }
    struct stat input_info, output_info;
    if (stat(argv[1], &input_info) == 0 && stat(argv[3], &output_info) == 0 &&
        input_info.st_dev == output_info.st_dev && input_info.st_ino == output_info.st_ino)
    {
        fprintf(stderr, "Входной и выходной файлы должны различаться.\n");
        return 1;
    }
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) { fprintf(stderr, "Не удалось открыть входной файл.\n"); return 1; }
    Employee *employees = NULL;
    size_t count = 0;
    Status status = read_employees(input, &employees, &count);
    if (fclose(input) != 0 && status == OK) status = FILE_ERROR;
    if (status == OK) status = sort_employees(employees, count, argv[2][1] == 'd', 1e-9);
    if (status == OK)
    {
        FILE *output = fopen(argv[3], "w");
        if (output == NULL) status = FILE_ERROR;
        else
        {
            status = write_employees(output, employees, count);
            if (fclose(output) != 0 && status == OK) status = FILE_ERROR;
        }
    }
    free_employees(employees, count);
    if (status != OK) fprintf(stderr, "Ошибка чтения, сортировки или записи (код %d).\n", status);
    return status == OK ? 0 : 1;
}
#endif
