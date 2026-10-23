#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

typedef enum
{
    STATUS_OK,
    STATUS_INVALID_INPUT,
    STATUS_MEMORY_ERROR,
    STATUS_RANGE_ERROR
} Status;

/* Coefficients are passed as double in ascending powers: f0, f1, ..., fn. */
Status shift_polynomial(double eps, double a, double **result, int degree, ...)
{
    if (result == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    *result = NULL;
    if (!isfinite(eps) || eps <= 0.0 || !isfinite(a) || degree < 0)
    {
        return STATUS_INVALID_INPUT;
    }
    size_t count = (size_t)degree + 1;
    if (count > SIZE_MAX / sizeof(double))
    {
        return STATUS_RANGE_ERROR;
    }
    double *coefficients = malloc(count * sizeof(double));
    double *shifted = calloc(count, sizeof(double));
    if (coefficients == NULL || shifted == NULL)
    {
        free(coefficients);
        free(shifted);
        return STATUS_MEMORY_ERROR;
    }
    va_list arguments;
    va_start(arguments, degree);
    Status status = STATUS_OK;
    for (size_t i = 0; i < count; i++)
    {
        coefficients[i] = va_arg(arguments, double);
        if (!isfinite(coefficients[i]))
        {
            status = STATUS_INVALID_INPUT;
        }
    }
    va_end(arguments);
    if (status != STATUS_OK)
    {
        free(coefficients);
        free(shifted);
        return status;
    }
    shifted[0] = coefficients[degree];
    size_t current_degree = 0;
    for (int k = degree - 1; k >= 0 && status == STATUS_OK; k--)
    {
        shifted[current_degree + 1] = shifted[current_degree];
        for (size_t j = current_degree; j > 0; j--)
        {
            long double value = (long double)a * shifted[j] + shifted[j - 1];
            if (!isfinite(value) || fabsl(value) > DBL_MAX)
            {
                status = STATUS_RANGE_ERROR;
                break;
            }
            shifted[j] = (double)value;
        }
        if (status == STATUS_OK)
        {
            long double value = (long double)a * shifted[0] + coefficients[k];
            if (!isfinite(value) || fabsl(value) > DBL_MAX)
            {
                status = STATUS_RANGE_ERROR;
            }
            else
            {
                shifted[0] = (double)value;
            }
        }
        current_degree++;
    }
    free(coefficients);
    if (status != STATUS_OK)
    {
        free(shifted);
        return status;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (fabs(shifted[i]) <= eps)
        {
            shifted[i] = 0.0;
        }
    }
    *result = shifted;
    return STATUS_OK;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    double *coefficients = NULL;
    Status status = shift_polynomial(1e-12, 2.0, &coefficients, 3,
                                     -1.0, 2.0, -3.0, 1.0);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "Polynomial error: %d\n", status);
        return 1;
    }
    printf("f(x) = -1 + 2x - 3x^2 + x^3; a = 2\n");
    for (size_t i = 0; i < 4; i++)
    {
        printf("g[%zu] = %.10g\n", i, coefficients[i]);
    }
    free(coefficients);
    return 0;
}
#endif
