#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NOT_A_NUMBER
} Status;

static Status read_double(const char *text, double *number)
{
    char *end = NULL;
    double value;

    if (text == NULL || number == NULL)
    {
        return STATUS_INVALID_ARGUMENT;
    }

    value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value))
    {
        return STATUS_NOT_A_NUMBER;
    }

    *number = value;
    return STATUS_OK;
}

static Status read_long_long(const char *text, long long *number)
{
    const unsigned long long positive_limit = LLONG_MAX;
    const unsigned long long negative_limit = (unsigned long long)LLONG_MAX + (unsigned long long) 1;
    unsigned long long value = 0;
    unsigned long long limit;
    int is_negative = 0;
    size_t index = 0;

    if (text == NULL || number == NULL)
    {
        return STATUS_INVALID_ARGUMENT;
    }

    if (text[index] == '+' || text[index] == '-')
    {
        if (text[index] == '-')
        {
            is_negative = 1;
        }
        index++;
    }

    if (text[index] == '\0')
    {
        return STATUS_NOT_A_NUMBER;
    }

    if (is_negative != 0)
    {
        limit = negative_limit;
    }
    else
    {
        limit = positive_limit;
    }

    while (text[index] != '\0')
    {
        unsigned int digit;

        if (!isdigit((unsigned char)text[index]))
        {
            return STATUS_NOT_A_NUMBER;
        }
        digit = (unsigned int)(text[index] - '0');
        if (value > (limit - digit) / (unsigned long long) 10)
        {
            return STATUS_NOT_A_NUMBER;
        }
        value = value * (unsigned long long) 10 + digit;
        index++;
    }

    if (is_negative != 0)
    {
        if (value == negative_limit)
        {
            *number = LLONG_MIN;
        }
        else
        {
            *number = -(long long)value;
        }
    }
    else
    {
        *number = (long long)value;
    }

    return STATUS_OK;
}

static int nearly_equal(double first, double second, double epsilon)
{
    return fabs(first - second) <= epsilon;
}

static void print_equation_solution(double a, double b, double c, double epsilon)
{
    double discriminant;
    double root1;
    double root2;

    printf("Уравнение: %.10g*x^2 %+.10g*x %+.10g = 0\n", a, b, c);

    if (nearly_equal(a, 0.0, epsilon))
    {
        if (nearly_equal(b, 0.0, epsilon))
        {
            if (nearly_equal(c, 0.0, epsilon))
            {
                printf("Любое число является решением.\n");
            }
            else
            {
                printf("Решений нет.\n");
            }
        }
        else
        {
            root1 = -c / b;
            printf("Это линейное уравнение. x = %.10g\n", root1);
        }
        return;
    }

    discriminant = b * b - 4.0 * a * c;
    if (discriminant < -epsilon)
    {
        printf("Действительных корней нет.\n");
    }
    else if (nearly_equal(discriminant, 0.0, epsilon))
    {
        root1 = -b / (2.0 * a);
        printf("Один корень: x = %.10g\n", root1);
    }
    else
    {
        root1 = (-b + sqrt(discriminant)) / (2.0 * a);
        root2 = (-b - sqrt(discriminant)) / (2.0 * a);
        printf("Два корня: x1 = %.10g, x2 = %.10g\n", root1, root2);
    }
}

static void solve_all_permutations(double first, double second, double third, double epsilon)
{
    double values[3] = {first, second, third};
    int permutations[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
        {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    int used[6] = {0, 0, 0, 0, 0, 0};
    int i;
    int j;

    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < i; j++)
        {
            if (nearly_equal(values[permutations[i][0]], values[permutations[j][0]], epsilon) &&
                nearly_equal(values[permutations[i][1]], values[permutations[j][1]], epsilon) &&
                nearly_equal(values[permutations[i][2]], values[permutations[j][2]], epsilon))
            {
                used[i] = 1;
            }
        }

        if (used[i] == 0)
        {
            print_equation_solution(values[permutations[i][0]], values[permutations[i][1]],
                                    values[permutations[i][2]], epsilon);
        }
    }
}

static void check_triangle(double a, double b, double c, double epsilon)
{
    double hypotenuse;
    double leg1;
    double leg2;

    if (a <= epsilon || b <= epsilon || c <= epsilon)
    {
        printf("Длины сторон должны быть положительными числами.\n");
        return;
    }

    hypotenuse = a;
    leg1 = b;
    leg2 = c;
    if (b > hypotenuse)
    {
        hypotenuse = b;
        leg1 = a;
        leg2 = c;
    }
    if (c > hypotenuse)
    {
        hypotenuse = c;
        leg1 = a;
        leg2 = b;
    }

    if (nearly_equal(hypotenuse * hypotenuse, leg1 * leg1 + leg2 * leg2, epsilon))
    {
        printf("Да, эти числа могут быть длинами сторон прямоугольного треугольника.\n");
    }
    else
    {
        printf("Нет, эти числа не являются длинами сторон прямоугольного треугольника.\n");
    }
}


int main(int argc, char *argv[])
{
    Status status;
    double epsilon;
    double a;
    double b;
    double c;
    long long first_number;
    long long second_number;

    if (argc < 2 || strlen(argv[1]) != 2 ||
        (argv[1][0] != '-' && argv[1][0] != '/'))
    {
        printf("Ошибка: введен неверный флаг.\n");
        return 1;
    }

    if (argv[1][1] == 'm')
    {
        if (argc != 4)
        {
            printf("Ошибка: для флага -m нужно ровно два целых числа.\n");
            return 1;
        }

        status = read_long_long(argv[2], &first_number);
        if (status != STATUS_OK)
        {
            printf("Ошибка: первое число введено неверно.\n");
            return 1;
        }
        status = read_long_long(argv[3], &second_number);
        if (status != STATUS_OK)
        {
            printf("Ошибка: второе число введено неверно.\n");
            return 1;
        }
        if (first_number == 0 || second_number == 0)
        {
            printf("Ошибка: числа для проверки кратности не должны быть нулем.\n");
            return 1;
        }

        if ((first_number == LLONG_MIN && second_number == -1) ||
            first_number % second_number == 0)
        {
            printf("Число %lld кратно числу %lld.\n", first_number, second_number);
        }
        else
        {
            printf("Число %lld не кратно числу %lld.\n", first_number, second_number);
        }
        return 0;
    }

    if (argv[1][1] != 'q' && argv[1][1] != 't')
    {
        printf("Ошибка: программа поддерживает только флаги -q, -m и -t.\n");
        return 1;
    }
    if (argc != 6)
    {
        printf("Ошибка: для этого флага нужны epsilon и три вещественных числа.\n");
        return 1;
    }

    status = read_double(argv[2], &epsilon);
    if (status != STATUS_OK || epsilon <= 0.0)
    {
        printf("Ошибка: epsilon должен быть положительным вещественным числом.\n");
        return 1;
    }
    status = read_double(argv[3], &a);
    if (status != STATUS_OK)
    {
        printf("Ошибка: первое вещественное число введено неверно.\n");
        return 1;
    }
    status = read_double(argv[4], &b);
    if (status != STATUS_OK)
    {
        printf("Ошибка: второе вещественное число введено неверно.\n");
        return 1;
    }
    status = read_double(argv[5], &c);
    if (status != STATUS_OK)
    {
        printf("Ошибка: третье вещественное число введено неверно.\n");
        return 1;
    }

    if (argv[1][1] == 'q')
    {
        solve_all_permutations(a, b, c, epsilon);
    }
    else
    {
        check_triangle(a, b, c, epsilon);
    }

    return 0;
}
