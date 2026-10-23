#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_NOT_CONVEX,
    STATUS_NO_ROOT,
    STATUS_OVERFLOW,
    STATUS_NO_CONVERGENCE
} Status;

typedef double (*Function)(double);


static Status is_convex_polygon(int count, int *answer, ...)
{
    va_list arguments;
    double *x;
    double *y;
    int i;
    int sign = 0;

    if (answer == NULL || count < 3)
    {
        return STATUS_INVALID_ARGUMENT;
    }

    x = malloc((size_t)count * sizeof(double));
    y = malloc((size_t)count * sizeof(double));
    if (x == NULL || y == NULL)
    {
        free(x);
        free(y);
        return STATUS_OVERFLOW;
    }

    va_start(arguments, answer);
    for (i = 0; i < count; i++)
    {
        x[i] = va_arg(arguments, double);
        y[i] = va_arg(arguments, double);
        if (!isfinite(x[i]) || !isfinite(y[i]))
        {
            va_end(arguments);
            free(x);
            free(y);
            return STATUS_INVALID_ARGUMENT;
        }
    }
    va_end(arguments);

    for (i = 0; i < count; i++)
    {
        int next = (i + 1) % count;
        int after_next = (i + 2) % count;
        double first_x = x[next] - x[i];
        double first_y = y[next] - y[i];
        double second_x = x[after_next] - x[next];
        double second_y = y[after_next] - y[next];
        double cross = first_x * second_y - first_y * second_x;
        int current_sign;

        if (fabs(cross) < 1e-12)
        {
            free(x);
            free(y);
            *answer = 0;
            return STATUS_NOT_CONVEX;
        }
        if (cross > 0.0)
        {
            current_sign = 1;
        }
        else
        {
            current_sign = -1;
        }
        if (sign == 0)
        {
            sign = current_sign;
        }
        else if (sign != current_sign)
        {
            free(x);
            free(y);
            *answer = 0;
            return STATUS_NOT_CONVEX;
        }
    }

    free(x);
    free(y);
    *answer = 1;
    return STATUS_OK;
}


static Status polynomial_value(double point, int degree, double *result, ...)
{
    va_list arguments;
    double value;
    int i;

    if (result == NULL || degree < 0 || !isfinite(point))
    {
        return STATUS_INVALID_ARGUMENT;
    }

    va_start(arguments, result);
    value = va_arg(arguments, double);
    if (!isfinite(value))
    {
        va_end(arguments);
        return STATUS_INVALID_ARGUMENT;
    }
    for (i = 1; i <= degree; i++)
    {
        double coefficient = va_arg(arguments, double);
        if (!isfinite(coefficient))
        {
            va_end(arguments);
            return STATUS_INVALID_ARGUMENT;
        }
        value = value * point + coefficient;
        if (!isfinite(value))
        {
            va_end(arguments);
            return STATUS_OVERFLOW;
        }
    }
    va_end(arguments);
    *result = value;
    return STATUS_OK;
}

static int digit_value(char symbol)
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

static Status parse_in_base(const char *text, int base, unsigned long long *result)
{
    unsigned long long number = 0;
    int digit;

    if (text == NULL || result == NULL || base < 2 || base > 36 || text[0] == '\0')
    {
        return STATUS_INVALID_ARGUMENT;
    }
    while (*text != '\0')
    {
        digit = digit_value(*text);
        if (digit < 0 || digit >= base || number > (ULLONG_MAX - (unsigned long long)digit) / (unsigned long long)base)
        {
            return STATUS_INVALID_ARGUMENT;
        }
        number = number * (unsigned long long)base + (unsigned long long)digit;
        text++;
    }
    *result = number;
    return STATUS_OK;
}

static int is_kaprekar(unsigned long long number, int base)
{
    __uint128_t square = (__uint128_t)number * number;
    __uint128_t power = 1;

    if (number == 0 || number == 1)
    {
        return 1;
    }
    while (power <= square / (unsigned int)base)
    {
        __uint128_t left;
        __uint128_t right;
        power *= (unsigned int)base;
        left = square / power;
        right = square % power;
        if (right != 0 && left + right == number)
        {
            return 1;
        }
    }
    return 0;
}

static Status find_kaprekar_numbers(int base, int count, int *flags, ...)
{
    va_list arguments;
    int i;

    if (flags == NULL || count < 0 || base < 2 || base > 36)
    {
        return STATUS_INVALID_ARGUMENT;
    }
    va_start(arguments, flags);
    for (i = 0; i < count; i++)
    {
        const char *text = va_arg(arguments, const char *);
        unsigned long long number;
        Status status = parse_in_base(text, base, &number);
        if (status != STATUS_OK)
        {
            va_end(arguments);
            return status;
        }
        flags[i] = is_kaprekar(number, base);
    }
    va_end(arguments);
    return STATUS_OK;
}

static Status geometric_mean(int count, double *result, ...)
{
    va_list arguments;
    double logarithm_sum = 0.0;
    int i;

    if (result == NULL || count <= 0)
    {
        return STATUS_INVALID_ARGUMENT;
    }
    va_start(arguments, result);
    for (i = 0; i < count; i++)
    {
        double number = va_arg(arguments, double);
        if (!isfinite(number) || number <= 0.0)
        {
            va_end(arguments);
            return STATUS_INVALID_ARGUMENT;
        }
        logarithm_sum += log(number);
    }
    va_end(arguments);
    *result = exp(logarithm_sum / (double)count);
    if (!isfinite(*result))
    {
        return STATUS_OVERFLOW;
    }
    return STATUS_OK;
}

static Status fast_power(double base, long long exponent, double *result)
{
    unsigned long long power;
    double half;
    Status status;

    if (result == NULL || !isfinite(base) || (base == 0.0 && exponent < 0))
    {
        return STATUS_INVALID_ARGUMENT;
    }
    if (exponent == 0)
    {
        *result = 1.0;
        return STATUS_OK;
    }
    power = exponent < 0 ? (unsigned long long)(-(exponent + 1)) + 1ULL : (unsigned long long)exponent;
    if (power == 1ULL)
    {
        *result = exponent < 0 ? 1.0 / base : base;
        return isfinite(*result) ? STATUS_OK : STATUS_OVERFLOW;
    }
    status = fast_power(base, (long long)(power / 2ULL), &half);
    if (status != STATUS_OK)
    {
        return status;
    }
    *result = half * half;
    if (power % 2ULL != 0ULL)
    {
        *result *= base;
    }
    if (exponent < 0)
    {
        *result = 1.0 / *result;
    }
    return isfinite(*result) ? STATUS_OK : STATUS_OVERFLOW;
}

static Status bisection(Function function, double left, double right, double epsilon, double *result)
{
    double left_value;
    double right_value;

    if (function == NULL || result == NULL || !isfinite(left) || !isfinite(right) ||
        !isfinite(epsilon) || left >= right || epsilon <= 0.0)
    {
        return STATUS_INVALID_ARGUMENT;
    }
    left_value = function(left);
    right_value = function(right);
    if (!isfinite(left_value) || !isfinite(right_value))
    {
        return STATUS_INVALID_ARGUMENT;
    }
    if (fabs(left_value) <= epsilon)
    {
        *result = left;
        return STATUS_OK;
    }
    if (fabs(right_value) <= epsilon)
    {
        *result = right;
        return STATUS_OK;
    }
    if ((left_value < 0.0 && right_value < 0.0) || (left_value > 0.0 && right_value > 0.0))
    {
        return STATUS_NO_ROOT;
    }
    while (right - left > epsilon)
    {
        double middle = left + (right - left) / 2.0;
        double middle_value;
        if (middle == left || middle == right)
        {
            return STATUS_NO_CONVERGENCE;
        }
        middle_value = function(middle);
        if (!isfinite(middle_value))
        {
            return STATUS_INVALID_ARGUMENT;
        }
        if (fabs(middle_value) <= epsilon)
        {
            *result = middle;
            return STATUS_OK;
        }
        if ((left_value < 0.0 && middle_value < 0.0) || (left_value > 0.0 && middle_value > 0.0))
        {
            left = middle;
            left_value = middle_value;
        }
        else
        {
            right = middle;
            right_value = middle_value;
        }
    }
    *result = (left + right) / 2.0;
    return STATUS_OK;
}

static double equation_one(double x)
{
    return x * x - 2.0;
}

static double equation_two(double x)
{
    return cos(x) - x;
}

int main(void)
{
    int convex;
    int flags[5];
    double result;
    Status status;
    const char *numbers[5] = {"1", "9", "45", "55", "10"};
    int i;

    printf("Лабораторная работа 6. Демонстрация функций.\n\n");

    status = is_convex_polygon(4, &convex, 0.0, 0.0, 3.0, 0.0, 3.0, 2.0, 0.0, 2.0);
    if (status == STATUS_OK && convex)
    {
        printf("Многоугольник (0,0), (3,0), (3,2), (0,2) выпуклый.\n");
    }
    else
    {
        printf("Не удалось проверить выпуклость многоугольника.\n");
    }

    status = polynomial_value(2.0, 3, &result, 1.0, -2.0, 0.0, 5.0);
    if (status == STATUS_OK)
    {
        printf("Значение x^3 - 2x^2 + 5 в точке x = 2: %.6f\n", result);
    }

    status = find_kaprekar_numbers(10, 5, flags, numbers[0], numbers[1], numbers[2], numbers[3], numbers[4]);
    if (status == STATUS_OK)
    {
        printf("Числа Капрекара в системе с основанием 10: ");
        for (i = 0; i < 5; i++)
        {
            if (flags[i])
            {
                printf("%s ", numbers[i]);
            }
        }
        printf("\n");
    }

    status = geometric_mean(3, &result, 2.0, 8.0, 4.0);
    if (status == STATUS_OK)
    {
        printf("Среднее геометрическое чисел 2, 8, 4: %.6f\n", result);
    }

    status = fast_power(2.0, -5, &result);
    if (status == STATUS_OK)
    {
        printf("2 в степени -5: %.6f\n", result);
    }

    status = bisection(equation_one, 1.0, 2.0, 0.000001, &result);
    if (status == STATUS_OK)
    {
        printf("Корень x^2 - 2 на [1; 2]: %.6f\n", result);
    }
    status = bisection(equation_two, 0.0, 1.0, 0.000001, &result);
    if (status == STATUS_OK)
    {
        printf("Корень cos(x) - x на [0; 1]: %.6f\n", result);
    }
    return 0;
}
