#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

typedef enum
{
    STATUS_OK,
    STATUS_INVALID_INPUT,
    STATUS_MEMORY_ERROR,
    STATUS_RANGE_ERROR,
    STATUS_NOT_RECONSTRUCTED
} Status;

typedef struct
{
    uint64_t numerator;
    uint64_t denominator;
    int finite;
} FractionResult;

Status greatest_common_divisor(uint64_t first, uint64_t second, uint64_t *result)
{
    if (result == NULL || (first == 0 && second == 0))
    {
        return STATUS_INVALID_INPUT;
    }
    while (second != 0)
    {
        uint64_t remainder = first % second;
        first = second;
        second = remainder;
    }
    *result = first;
    return STATUS_OK;
}

/* The simplest rational within eps is an explicit interpretation of double. */
Status restore_fraction(double number, double eps, uint32_t max_denominator,
                        uint64_t *numerator, uint64_t *denominator)
{
    if (numerator == NULL || denominator == NULL || !isfinite(number) ||
        number <= 0.0 || number >= 1.0 || !isfinite(eps) ||
        eps <= 0.0 || eps >= 0.5 || max_denominator < 2)
    {
        return STATUS_INVALID_INPUT;
    }
    for (uint64_t current = 2; current <= max_denominator; current++)
    {
        long double nearest = floorl((long double)number * current + 0.5L);
        if (nearest < 1.0L)
        {
            nearest = 1.0L;
        }
        if (nearest >= (long double)current)
        {
            nearest = (long double)current - 1.0L;
        }
        uint64_t top = (uint64_t)nearest;
        long double difference = fabsl((long double)number - nearest / current);
        if (difference <= (long double)eps)
        {
            uint64_t divisor = 0;
            (void)greatest_common_divisor(top, current, &divisor);
            *numerator = top / divisor;
            *denominator = current / divisor;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_RECONSTRUCTED;
}

Status finite_in_base(uint64_t denominator, int base, int *finite)
{
    if (finite == NULL || denominator == 0 || base < 2 || base > 36)
    {
        return STATUS_INVALID_INPUT;
    }
    while (denominator != 1)
    {
        uint64_t divisor = 0;
        (void)greatest_common_divisor(denominator, (uint64_t)base, &divisor);
        if (divisor == 1)
        {
            *finite = 0;
            return STATUS_OK;
        }
        denominator /= divisor;
    }
    *finite = 1;
    return STATUS_OK;
}

/* After result, pass exactly count values of type double, including 0.5, not 1/2. */
Status check_fractions(int base, double eps, uint32_t max_denominator,
                       size_t count, FractionResult **result, ...)
{
    if (result == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    *result = NULL;
    if (base < 2 || base > 36 || !isfinite(eps) || eps <= 0.0 ||
        eps >= 0.5 || max_denominator < 2)
    {
        return STATUS_INVALID_INPUT;
    }
    if (count == 0)
    {
        return STATUS_OK;
    }
    if (count > SIZE_MAX / sizeof(FractionResult))
    {
        return STATUS_RANGE_ERROR;
    }
    FractionResult *answers = malloc(count * sizeof(FractionResult));
    if (answers == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    va_list arguments;
    va_start(arguments, result);
    Status status = STATUS_OK;
    for (size_t i = 0; i < count && status == STATUS_OK; i++)
    {
        double number = va_arg(arguments, double);
        status = restore_fraction(number, eps, max_denominator,
                                  &answers[i].numerator, &answers[i].denominator);
        if (status == STATUS_OK)
        {
            status = finite_in_base(answers[i].denominator, base, &answers[i].finite);
        }
    }
    va_end(arguments);
    if (status != STATUS_OK)
    {
        free(answers);
        return status;
    }
    *result = answers;
    return STATUS_OK;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    FractionResult *answers = NULL;
    Status status = check_fractions(2, 1e-12, 1000000, 4, &answers,
                                    0.5, 0.125, 0.1, 1.0 / 3.0);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "Fraction error: %d\n", status);
        return 1;
    }
    for (size_t i = 0; i < 4; i++)
    {
        printf("%llu/%llu: %s in base 2\n",
               (unsigned long long)answers[i].numerator,
               (unsigned long long)answers[i].denominator,
               answers[i].finite ? "finite" : "infinite");
    }
    free(answers);
    return 0;
}
#endif
