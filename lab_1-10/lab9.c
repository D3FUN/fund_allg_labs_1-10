#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define FIXED_ARRAY_SIZE 10
#define MIN_DYNAMIC_SIZE 10
#define MAX_DYNAMIC_SIZE 10000
#define MIN_VALUE -1000
#define MAX_VALUE 1000

typedef enum
{
    STATUS_OK,
    STATUS_WRONG_ARGUMENTS,
    STATUS_INVALID_NUMBER,
    STATUS_MEMORY_ERROR
} Status;

static Status read_int(const char *text, int *number)
{
    size_t i = 0;
    int is_negative = 0;
    unsigned long long value = 0;
    unsigned long long limit;

    if (text == NULL || number == NULL || text[0] == '\0')
    {
        return STATUS_INVALID_NUMBER;
    }

    if (text[i] == '-' || text[i] == '+')
    {
        if (text[i] == '-')
        {
            is_negative = 1;
        }
        i++;
    }

    if (text[i] == '\0')
    {
        return STATUS_INVALID_NUMBER;
    }

    if (is_negative)
    {
        limit = (unsigned long long)INT_MAX + 1ULL;
    }
    else
    {
        limit = (unsigned long long)INT_MAX;
    }

    for (; text[i] != '\0'; i++)
    {
        unsigned int digit;

        if (text[i] < '0' || text[i] > '9')
        {
            return STATUS_INVALID_NUMBER;
        }
        digit = (unsigned int)(text[i] - '0');
        if (value > (limit - digit) / 10ULL)
        {
            return STATUS_INVALID_NUMBER;
        }
        value = value * 10ULL + digit;
    }

    if (is_negative)
    {
        if (value == (unsigned long long)INT_MAX + 1ULL)
        {
            *number = INT_MIN;
        }
        else
        {
            *number = -(int)value;
        }
    }
    else
    {
        *number = (int)value;
    }
    return STATUS_OK;
}


static int random_in_range(int left, int right)
{
    unsigned long long count;

    count = (unsigned long long)((long long)right - (long long)left + 1);
    return left + (int)((unsigned long long)rand() % count);
}

static void print_array(const char *name, const int *array, size_t size)
{
    size_t i;

    if (name == NULL || array == NULL)
    {
        return;
    }

    printf("%s: ", name);
    for (i = 0; i < size; i++)
    {
        printf("%d", array[i]);
        if (i + 1 < size)
        {
            printf(" ");
        }
    }
    printf("\n");
}


static Status swap_min_and_max(int *array, size_t size, int *min_value, int *max_value)
{
    size_t i;
    size_t min_index;
    size_t max_index;
    int temporary;

    if (array == NULL || min_value == NULL || max_value == NULL || size == 0)
    {
        return STATUS_INVALID_NUMBER;
    }

    min_index = 0;
    max_index = 0;
    for (i = 1; i < size; i++)
    {
        if (array[i] < array[min_index])
        {
            min_index = i;
        }
        if (array[i] > array[max_index])
        {
            max_index = i;
        }
    }

    *min_value = array[min_index];
    *max_value = array[max_index];
    temporary = array[min_index];
    array[min_index] = array[max_index];
    array[max_index] = temporary;
    return STATUS_OK;
}

static Status make_random_array(int **array, size_t size, int left, int right)
{
    size_t i;
    int *new_array;

    if (array == NULL || size == 0 || left > right)
    {
        return STATUS_INVALID_NUMBER;
    }

    new_array = malloc(size * sizeof(int));
    if (new_array == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }

    for (i = 0; i < size; i++)
    {
        new_array[i] = random_in_range(left, right);
    }
    *array = new_array;
    return STATUS_OK;
}

static int find_nearest(const int *array, size_t size, int number)
{
    size_t i;
    int nearest;
    long long difference;
    long long nearest_difference;

    nearest = array[0];
    nearest_difference = llabs((long long)number - (long long)array[0]);
    for (i = 1; i < size; i++)
    {
        difference = llabs((long long)number - (long long)array[i]);
        if (difference < nearest_difference)
        {
            nearest_difference = difference;
            nearest = array[i];
        }
    }
    return nearest;
}

static Status make_array_c(const int *array_a, size_t size_a, const int *array_b,
                           size_t size_b, int **array_c)
{
    size_t i;
    int *new_array;

    if (array_a == NULL || array_b == NULL || array_c == NULL || size_a == 0 || size_b == 0)
    {
        return STATUS_INVALID_NUMBER;
    }

    new_array = malloc(size_a * sizeof(int));
    if (new_array == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }

    for (i = 0; i < size_a; i++)
    {
        new_array[i] = array_a[i] + find_nearest(array_b, size_b, array_a[i]);
    }
    *array_c = new_array;
    return STATUS_OK;
}

int main(int argc, char *argv[])
{
    int left;
    int right;
    int fixed_array[FIXED_ARRAY_SIZE];
    int min_value;
    int max_value;
    int *array_a = NULL;
    int *array_b = NULL;
    int *array_c = NULL;
    size_t size_a;
    size_t size_b;
    size_t i;
    Status status;

    if (argc != 3)
    {
        printf("Ошибка: укажите две границы диапазона.\n");
        printf("Пример запуска: ./lab9 -10 20\n");
        return STATUS_WRONG_ARGUMENTS;
    }

    status = read_int(argv[1], &left);
    if (status == STATUS_OK)
    {
        status = read_int(argv[2], &right);
    }
    if (status != STATUS_OK || left > right)
    {
        printf("Ошибка: границы диапазона должны быть целыми числами, a <= b.\n");
        return STATUS_INVALID_NUMBER;
    }

    srand((unsigned int)time(NULL));

    for (i = 0; i < FIXED_ARRAY_SIZE; i++)
    {
        fixed_array[i] = random_in_range(left, right);
    }

    print_array("Исходный массив", fixed_array, FIXED_ARRAY_SIZE);
    status = swap_min_and_max(fixed_array, FIXED_ARRAY_SIZE, &min_value, &max_value);
    if (status != STATUS_OK)
    {
        printf("Ошибка: не удалось найти минимум и максимум.\n");
        return status;
    }
    printf("Минимальный элемент: %d, максимальный элемент: %d.\n", min_value, max_value);
    print_array("Массив после обмена", fixed_array, FIXED_ARRAY_SIZE);

    size_a = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    size_b = (size_t)random_in_range(MIN_DYNAMIC_SIZE, MAX_DYNAMIC_SIZE);
    status = make_random_array(&array_a, size_a, MIN_VALUE, MAX_VALUE);
    if (status == STATUS_OK)
    {
        status = make_random_array(&array_b, size_b, MIN_VALUE, MAX_VALUE);
    }
    if (status == STATUS_OK)
    {
        status = make_array_c(array_a, size_a, array_b, size_b, &array_c);
    }
    if (status != STATUS_OK)
    {
        printf("Ошибка: не удалось выделить память для динамических массивов.\n");
        free(array_a);
        free(array_b);
        free(array_c);
        return status;
    }

    printf("Размер массива A: %zu, размер массива B: %zu.\n", size_a, size_b);
    printf("Массив C создан: C[i] = A[i] + ближайший по значению элемент B.\n");
    printf("Первые элементы массива C: ");
    for (i = 0; i < 10 && i < size_a; i++)
    {
        printf("%d ", array_c[i]);
    }
    printf("\n");

    free(array_a);
    free(array_b);
    free(array_c);
    return STATUS_OK;
}
