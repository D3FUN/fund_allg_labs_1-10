#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    STATUS_OK,
    STATUS_WRONG_ARGUMENTS,
    STATUS_WRONG_NUMBER,
    STATUS_NO_CONVERGENCE,
    STATUS_OVERFLOW
} Status;

#define MAX_ITERATIONS 10000000
#define MAX_RECURSION 60

typedef double (*Function)(double);

static Status read_double(const char *text, double *number)
{
    char *end;
    double value;

    if (text == NULL || number == NULL)
    {
        return STATUS_WRONG_NUMBER;
    }

    value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value))
    {
        return STATUS_WRONG_NUMBER;
    }

    *number = value;
    return STATUS_OK;
}

static Status sum_a(double x, double epsilon, double *result)
{
    double sum = 1.0;
    double term = 1.0;
    int n;

    for (n = 1; n < MAX_ITERATIONS; n++)
    {
        term *= x / (double)n;
        sum += term;
        if (!isfinite(sum) || !isfinite(term))
        {
            return STATUS_OVERFLOW;
        }
        if (fabs(term) < epsilon)
        {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static Status sum_b(double x, double epsilon, double *result)
{
    double sum = 1.0;
    double term = 1.0;
    int n;

    for (n = 1; n < MAX_ITERATIONS; n++)
    {
        term *= -x * x / ((double)(2 * n - 1) * (double)(2 * n));
        sum += term;
        if (!isfinite(sum) || !isfinite(term))
        {
            return STATUS_OVERFLOW;
        }
        if (fabs(term) < epsilon)
        {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static Status sum_c(double x, double epsilon, double *result)
{
    double sum = 1.0;
    double term = 1.0;
    int n;

    for (n = 1; n < MAX_ITERATIONS; n++)
    {
        term *= 9.0 * (double)n * (double)n * x * x /
                ((double)(3 * n - 1) * (double)(3 * n - 2));
        sum += term;
        if (!isfinite(sum) || !isfinite(term))
        {
            return STATUS_OVERFLOW;
        }
        if (fabs(term) < epsilon)
        {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static Status sum_d(double x, double epsilon, double *result)
{
    double sum = 0.0;
    double term = 1.0;
    int n;

    if (fabs(x) > 1.0)
    {
        return STATUS_NO_CONVERGENCE;
    }

    for (n = 1; n < MAX_ITERATIONS; n++)
    {
        term *= -x * x * (double)(2 * n - 1) / (double)(2 * n);
        sum += term;
        if (!isfinite(sum) || !isfinite(term))
        {
            return STATUS_OVERFLOW;
        }
        if (fabs(term) < epsilon)
        {
            *result = sum;
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static double integral_a_function(double x)
{
    if (fabs(x) < 1e-12)
    {
        return 1.0;
    }
    else
    {
        return log1p(x) / x;
    }
}

static double integral_b_function(double x)
{
    return exp(-x * x / 2.0);
}

static double integral_c_function(double x)
{
    return -log1p(-x);
}

static double integral_d_function(double x)
{
    if (x == 0.0)
    {
        return 1.0;
    }
    else
    {
        return pow(x, x);
    }
}

static double simpson(Function function, double left, double right)
{
    double middle = (left + right) / 2.0;
    return (right - left) * (function(left) + 4.0 * function(middle) + function(right)) / 6.0;
}

static Status adaptive_simpson(Function function, double left, double right, double epsilon,
                               double old_value, int depth, double *result)
{
    double middle;
    double left_value;
    double right_value;
    double new_value;

    if (depth >= MAX_RECURSION)
    {
        return STATUS_NO_CONVERGENCE;
    }

    middle = (left + right) / 2.0;
    left_value = simpson(function, left, middle);
    right_value = simpson(function, middle, right);
    new_value = left_value + right_value;
    if (!isfinite(new_value))
    {
        return STATUS_OVERFLOW;
    }

    if (fabs(new_value - old_value) <= 15.0 * epsilon)
    {
        *result = new_value + (new_value - old_value) / 15.0;
        return STATUS_OK;
    }
    else
    {
        Status status;
        double first_part;
        double second_part;

        status = adaptive_simpson(function, left, middle, epsilon / 2.0,
                                  left_value, depth + 1, &first_part);
        if (status != STATUS_OK)
        {
            return status;
        }
        status = adaptive_simpson(function, middle, right, epsilon / 2.0,
                                  right_value, depth + 1, &second_part);
        if (status != STATUS_OK)
        {
            return status;
        }
        *result = first_part + second_part;
        return STATUS_OK;
    }
}

static Status calculate_integral(Function function, double epsilon, double *result)
{
    double right = 1.0;
    double old_value;

    if (function == integral_c_function)
    {
        right = 1.0 - epsilon * epsilon;
    }
    old_value = simpson(function, 0.0, right);
    return adaptive_simpson(function, 0.0, right, epsilon, old_value, 0, result);
}

static void print_status(Status status)
{
    if (status == STATUS_NO_CONVERGENCE)
    {
        printf("не удалось получить нужную точность.\n");
    }
    else if (status == STATUS_OVERFLOW)
    {
        printf("слишком большое число, произошло переполнение.\n");
    }
    else
    {
        printf("неизвестная ошибка.\n");
    }
}

static void print_sum(const char *name, Status status, double value)
{
    printf("Сумма %s: ", name);
    if (status == STATUS_OK)
    {
        printf("%.12g\n", value);
    }
    else
    {
        print_status(status);
    }
}

static void print_integral(const char *name, Status status, double value)
{
    printf("Интеграл %s: ", name);
    if (status == STATUS_OK)
    {
        printf("%.12g\n", value);
    }
    else
    {
        print_status(status);
    }
}

int main(int argc, char *argv[])
{
    double epsilon;
    double x;
    double value = 0.0;
    Status status;

    if (argc != 2 && argc != 3)
    {
        printf("Использование: %s epsilon [x]\n", argv[0]);
        printf("epsilon - положительная точность, x - аргумент рядов.\n");
        return STATUS_WRONG_ARGUMENTS;
    }

    status = read_double(argv[1], &epsilon);
    if (status != STATUS_OK || epsilon <= 0.0 || epsilon >= 1.0)
    {
        printf("Ошибка: epsilon должен быть числом от 0 до 1.\n");
        return STATUS_WRONG_NUMBER;
    }

    if (argc == 3)
    {
        status = read_double(argv[2], &x);
        if (status != STATUS_OK)
        {
            printf("Ошибка: x должен быть вещественным числом.\n");
            return STATUS_WRONG_NUMBER;
        }
    }
    else
    {
        printf("Введите x для вычисления рядов: ");
        if (scanf("%lf", &x) != 1 || !isfinite(x))
        {
            printf("Ошибка: x должен быть вещественным числом.\n");
            return STATUS_WRONG_NUMBER;
        }
    }

    printf("Точность epsilon = %.12g, x = %.12g\n\n", epsilon, x);
    status = sum_a(x, epsilon, &value);
    print_sum("a", status, value);
    status = sum_b(x, epsilon, &value);
    print_sum("b", status, value);
    status = sum_c(x, epsilon, &value);
    print_sum("c", status, value);
    status = sum_d(x, epsilon, &value);
    print_sum("d", status, value);

    printf("\nИнтегралы на отрезке [0; 1]:\n");
    status = calculate_integral(integral_a_function, epsilon, &value);
    print_integral("a", status, value);
    status = calculate_integral(integral_b_function, epsilon, &value);
    print_integral("b", status, value);
    status = calculate_integral(integral_c_function, epsilon, &value);
    print_integral("c", status, value);
    status = calculate_integral(integral_d_function, epsilon, &value);
    print_integral("d", status, value);

    return STATUS_OK;
}
