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
    STATUS_RANGE_ERROR,
    STATUS_NOT_POSITIVE_DEFINITE
} Status;

typedef struct
{
    size_t dimension;
    const double *coordinates;
} Vector;

typedef struct
{
    double p;
    const double *matrix;
} NormParameters;

typedef Status (*NormFunction)(const Vector *, const NormParameters *, double, double *);

typedef struct
{
    NormFunction function;
    const NormParameters *parameters;
} NormRequest;

typedef struct
{
    Vector *vectors;
    size_t count;
    double maximum;
} NormResult;

Status check_vector(const Vector *vector, double eps)
{
    if (vector == NULL || vector->coordinates == NULL || vector->dimension == 0 ||
        !isfinite(eps) || eps <= 0.0)
    {
        return STATUS_INVALID_INPUT;
    }
    for (size_t i = 0; i < vector->dimension; i++)
    {
        if (!isfinite(vector->coordinates[i]))
        {
            return STATUS_INVALID_INPUT;
        }
    }
    return STATUS_OK;
}

Status norm_infinity(const Vector *vector, const NormParameters *parameters,
                     double eps, double *result)
{
    (void)parameters;
    if (result == NULL || check_vector(vector, eps) != STATUS_OK)
    {
        return STATUS_INVALID_INPUT;
    }
    double maximum = 0.0;
    for (size_t i = 0; i < vector->dimension; i++)
    {
        double current = fabs(vector->coordinates[i]);
        if (current > maximum)
        {
            maximum = current;
        }
    }
    *result = maximum;
    return STATUS_OK;
}

Status norm_p(const Vector *vector, const NormParameters *parameters,
              double eps, double *result)
{
    if (result == NULL || parameters == NULL || !isfinite(parameters->p) ||
        parameters->p < 1.0 || check_vector(vector, eps) != STATUS_OK)
    {
        return STATUS_INVALID_INPUT;
    }
    double scale = 0.0;
    (void)norm_infinity(vector, NULL, eps, &scale);
    if (scale == 0.0)
    {
        *result = 0.0;
        return STATUS_OK;
    }
    long double sum = 0.0L;
    for (size_t i = 0; i < vector->dimension; i++)
    {
        long double normalized = fabsl((long double)vector->coordinates[i]) / scale;
        sum += powl(normalized, (long double)parameters->p);
    }
    long double value = scale * powl(sum, 1.0L / parameters->p);
    if (!isfinite(value) || value > DBL_MAX)
    {
        return STATUS_RANGE_ERROR;
    }
    *result = (double)value;
    return STATUS_OK;
}

/* Matrix is stored by rows; symmetry and Cholesky pivots are checked with eps. */
Status check_positive_matrix(const double *matrix, size_t dimension, double eps)
{
    if (matrix == NULL || dimension == 0 || !isfinite(eps) || eps <= 0.0)
    {
        return STATUS_INVALID_INPUT;
    }
    if (dimension > SIZE_MAX / dimension ||
        dimension * dimension > SIZE_MAX / sizeof(long double))
    {
        return STATUS_RANGE_ERROR;
    }
    for (size_t i = 0; i < dimension; i++)
    {
        for (size_t j = 0; j < dimension; j++)
        {
            if (!isfinite(matrix[i * dimension + j]))
            {
                return STATUS_INVALID_INPUT;
            }
            long double difference = (long double)matrix[i * dimension + j] -
                                     matrix[j * dimension + i];
            if (fabsl(difference) > eps)
            {
                return STATUS_NOT_POSITIVE_DEFINITE;
            }
        }
    }
    long double *lower = calloc(dimension * dimension, sizeof(long double));
    if (lower == NULL)
    {
        return STATUS_MEMORY_ERROR;
    }
    Status status = STATUS_OK;
    for (size_t i = 0; i < dimension && status == STATUS_OK; i++)
    {
        for (size_t j = 0; j <= i; j++)
        {
            long double value = (long double)matrix[i * dimension + j] / 2.0L +
                                (long double)matrix[j * dimension + i] / 2.0L;
            for (size_t k = 0; k < j; k++)
            {
                value -= lower[i * dimension + k] * lower[j * dimension + k];
            }
            if (!isfinite(value))
            {
                status = STATUS_RANGE_ERROR;
                break;
            }
            if (i == j)
            {
                if (value <= eps)
                {
                    status = STATUS_NOT_POSITIVE_DEFINITE;
                    break;
                }
                lower[i * dimension + j] = sqrtl(value);
            }
            else
            {
                lower[i * dimension + j] = value / lower[j * dimension + j];
                if (!isfinite(lower[i * dimension + j]))
                {
                    status = STATUS_RANGE_ERROR;
                    break;
                }
            }
        }
    }
    free(lower);
    return status;
}

Status norm_matrix(const Vector *vector, const NormParameters *parameters,
                   double eps, double *result)
{
    if (result == NULL || parameters == NULL ||
        check_vector(vector, eps) != STATUS_OK)
    {
        return STATUS_INVALID_INPUT;
    }
    size_t dimension = vector->dimension;
    Status status = check_positive_matrix(parameters->matrix, dimension, eps);
    if (status != STATUS_OK)
    {
        return status;
    }
    long double square = 0.0L;
    for (size_t i = 0; i < dimension; i++)
    {
        long double row_product = 0.0L;
        for (size_t j = 0; j < dimension; j++)
        {
            row_product += (long double)parameters->matrix[i * dimension + j] *
                           vector->coordinates[j];
        }
        square += row_product * vector->coordinates[i];
    }
    if (!isfinite(square))
    {
        return STATUS_RANGE_ERROR;
    }
    if (square < -(long double)eps)
    {
        return STATUS_RANGE_ERROR;
    }
    if (square < 0.0L)
    {
        square = 0.0L;
    }
    long double value = sqrtl(square);
    if (value > DBL_MAX)
    {
        return STATUS_RANGE_ERROR;
    }
    *result = (double)value;
    return STATUS_OK;
}

void free_norm_results(NormResult *results, size_t norm_count)
{
    if (results != NULL)
    {
        for (size_t i = 0; i < norm_count; i++)
        {
            free(results[i].vectors);
        }
        free(results);
    }
}

/* Variadic order: vector_count Vector values, then norm_count NormRequest values. */
Status longest_vectors(size_t dimension, size_t vector_count, size_t norm_count,
                       double eps, NormResult **result, ...)
{
    if (result == NULL)
    {
        return STATUS_INVALID_INPUT;
    }
    *result = NULL;
    if (dimension == 0 || vector_count == 0 || norm_count == 0 ||
        !isfinite(eps) || eps <= 0.0)
    {
        return STATUS_INVALID_INPUT;
    }
    if (vector_count > SIZE_MAX / sizeof(Vector) ||
        vector_count > SIZE_MAX / sizeof(double) ||
        norm_count > SIZE_MAX / sizeof(NormRequest) ||
        norm_count > SIZE_MAX / sizeof(NormResult))
    {
        return STATUS_RANGE_ERROR;
    }
    Vector *vectors = malloc(vector_count * sizeof(Vector));
    NormRequest *requests = malloc(norm_count * sizeof(NormRequest));
    double *values = malloc(vector_count * sizeof(double));
    NormResult *answers = calloc(norm_count, sizeof(NormResult));
    if (vectors == NULL || requests == NULL || values == NULL || answers == NULL)
    {
        free(vectors);
        free(requests);
        free(values);
        free(answers);
        return STATUS_MEMORY_ERROR;
    }
    va_list arguments;
    va_start(arguments, result);
    Status status = STATUS_OK;
    for (size_t i = 0; i < vector_count; i++)
    {
        vectors[i] = va_arg(arguments, Vector);
        if (vectors[i].dimension != dimension || check_vector(&vectors[i], eps) != STATUS_OK)
        {
            status = STATUS_INVALID_INPUT;
        }
    }
    for (size_t i = 0; i < norm_count; i++)
    {
        requests[i] = va_arg(arguments, NormRequest);
        if (requests[i].function == NULL)
        {
            status = STATUS_INVALID_INPUT;
        }
    }
    va_end(arguments);
    for (size_t i = 0; i < norm_count && status == STATUS_OK; i++)
    {
        double maximum = 0.0;
        for (size_t j = 0; j < vector_count; j++)
        {
            status = requests[i].function(&vectors[j], requests[i].parameters, eps, &values[j]);
            if (status != STATUS_OK)
            {
                break;
            }
            if (!isfinite(values[j]) || values[j] < 0.0)
            {
                status = STATUS_RANGE_ERROR;
                break;
            }
            if (values[j] > maximum)
            {
                maximum = values[j];
            }
        }
        if (status != STATUS_OK)
        {
            break;
        }
        answers[i].vectors = malloc(vector_count * sizeof(Vector));
        if (answers[i].vectors == NULL)
        {
            status = STATUS_MEMORY_ERROR;
            break;
        }
        answers[i].maximum = maximum;
        for (size_t j = 0; j < vector_count; j++)
        {
            if (fabs(values[j] - maximum) <= eps)
            {
                answers[i].vectors[answers[i].count] = vectors[j];
                answers[i].count++;
            }
        }
    }
    free(vectors);
    free(requests);
    free(values);
    if (status != STATUS_OK)
    {
        free_norm_results(answers, norm_count);
        return status;
    }
    *result = answers;
    return STATUS_OK;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    double first[] = {3.0, 4.0};
    double second[] = {-3.0, -4.0};
    double third[] = {5.0, 0.0};
    double matrix[] = {2.0, 0.0, 0.0, 1.0};
    Vector v1 = {2, first};
    Vector v2 = {2, second};
    Vector v3 = {2, third};
    NormParameters p_parameters = {2.0, NULL};
    NormParameters matrix_parameters = {0.0, matrix};
    NormRequest infinity_request = {norm_infinity, NULL};
    NormRequest p_request = {norm_p, &p_parameters};
    NormRequest matrix_request = {norm_matrix, &matrix_parameters};
    NormResult *answers = NULL;
    Status status = longest_vectors(2, 3, 3, 1e-10, &answers,
                                    v1, v2, v3,
                                    infinity_request, p_request, matrix_request);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "Norm error: %d\n", status);
        return 1;
    }
    for (size_t i = 0; i < 3; i++)
    {
        printf("Norm %zu, maximum %.10g:\n", i + 1, answers[i].maximum);
        for (size_t j = 0; j < answers[i].count; j++)
        {
            printf("  (%.10g, %.10g)\n", answers[i].vectors[j].coordinates[0],
                   answers[i].vectors[j].coordinates[1]);
        }
    }
    free_norm_results(answers, 3);
    return 0;
}
#endif
