#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATUS_OK = 0, STATUS_INVALID_ARGUMENT, STATUS_NO_ROOT,
    STATUS_NO_CONVERGENCE, STATUS_OVERFLOW
} Status;

typedef double (*Function)(double);
enum { MAX_ITERATIONS = 10000000 };

static Status find_root_bisection(Function function, double left, double right, double epsilon, double *result)
{
    double left_value, right_value;
    int iteration = 0;
    if (function == NULL || result == NULL || !isfinite(left) || !isfinite(right) ||
        !isfinite(epsilon) || left >= right || epsilon <= 0.0) {
            return STATUS_INVALID_ARGUMENT;
        }
    left_value = function(left);
    right_value = function(right);
    if (!isfinite(left_value) || !isfinite(right_value)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (left_value == 0.0)
    {
        *result = left; return STATUS_OK; 
    }
    if (right_value == 0.0) {
        *result = right; return STATUS_OK; 
    }
    if ((left_value < 0.0 && right_value < 0.0) ||
        (left_value > 0.0 && right_value > 0.0)) {
            return STATUS_NO_ROOT;
        }
    while (right - left >= epsilon && iteration < MAX_ITERATIONS) {
        double middle = left + (right - left) / 2.0;
        double middle_value = function(middle);
        if (middle == left || middle == right) {
            return STATUS_NO_CONVERGENCE;
        }
        if (!isfinite(middle_value)) {
            return STATUS_INVALID_ARGUMENT;
        }
        if (middle_value == 0.0) {
            *result = middle; return STATUS_OK; 
        }
        if ((left_value < 0.0 && middle_value < 0.0) ||
            (left_value > 0.0 && middle_value > 0.0)) {
            left = middle;
            left_value = middle_value;
        } else right = middle;
        iteration++;
    }
    if (iteration == MAX_ITERATIONS) {
        return STATUS_NO_CONVERGENCE;
    }
    *result = left + (right - left) / 2.0;
    return STATUS_OK;
}

static Status calculate_e_series(double epsilon, double *result)
{
    double sum = 1.0, term = 1.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; fabs(term) >= epsilon && n < MAX_ITERATIONS; n++) {
        sum += term;
        term /= (double)(n + 1);
    }
    if (n == MAX_ITERATIONS) {
        return STATUS_NO_CONVERGENCE;
    }
    *result = sum; {
        return STATUS_OK;
    }
}

static Status calculate_e_limit(double epsilon, double *result)
{
    int n = 1;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    while (3.0 / (2.0 * (double)n) >= epsilon) {
        if (n >= MAX_ITERATIONS) {
            return STATUS_NO_CONVERGENCE;
        }
        n++;
    }
    *result = pow(1.0 + 1.0 / (double)n, (double)n);
    if (isfinite(*result))
    {
        return STATUS_OK;
    }else {
        return STATUS_OVERFLOW;
    }
}
static double equation_e(double x) {
    return log(x) - 1.0; 
}

static Status calculate_pi_series(double epsilon, double *result)
{
    double sum = 0.0;
    int denominator = 1, sign = 1, iteration = 0;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    while (4.0 / (double)denominator >= epsilon && iteration < MAX_ITERATIONS) {
        sum += sign * 4.0 / (double)denominator; sign = -sign; denominator += 2; iteration++;
    }
    if (iteration == MAX_ITERATIONS) {
        return STATUS_NO_CONVERGENCE;
    }
    *result = sum; 
    return STATUS_OK;
}

static Status calculate_pi_limit(double epsilon, double *result)
{
    double product = 1.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; 1.0 / (double)n >= epsilon && n < MAX_ITERATIONS; n++) {
        double k = (double)n; product *= 4.0 * k * k / (4.0 * k * k - 1.0);
    }
    if (n == MAX_ITERATIONS) {
        return STATUS_NO_CONVERGENCE;
    }
    *result = 2.0 * product; 
    return STATUS_OK;
}
static double equation_pi(double x) {
    return cos(x / 2.0); 
}

static Status calculate_ln2_series(double epsilon, double *result)
{
    double sum = 0.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; 1.0 / (double)n >= epsilon && n < MAX_ITERATIONS; n++) sum += (n % 2 ? 1.0 : -1.0) / (double)n;
    if (n == MAX_ITERATIONS) {
        return STATUS_NO_CONVERGENCE;
    }
    *result = sum; 
    return STATUS_OK;
}

static Status calculate_ln2_limit(double epsilon, double *result)
{
    double current = 0.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; n < MAX_ITERATIONS; n++) {
        current = (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);
        if (1.0 / (double)n < epsilon) {
            *result = current;
            return STATUS_OK; 
        }
    }
    return STATUS_NO_CONVERGENCE;
}
static double equation_ln2(double x) {
    return exp(x) - 2.0; 
}

static Status calculate_sqrt2_series(double epsilon, double *result)
{
    double product = 1.0, factor = sqrt(sqrt(2.0));
    int k;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    /* The remaining product is factor squared; epsilon / 4 bounds its tail. */
    for (k = 2; fabs(factor - 1.0) >= epsilon / 4.0 && k < 1075; k++) {
        product *= factor; 
        factor = sqrt(factor); 
    }
    *result = product;
    return STATUS_OK;
}

static Status calculate_sqrt2_limit(double epsilon, double *result)
{
    double current = -0.5;
    int iteration;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        double next = current - (current * current - 2.0) / (current * current + 1.0);
        if (fabs(next - current) < epsilon) {
            *result = next;
            return STATUS_OK;
        }
        current = next;
    }
    return STATUS_NO_CONVERGENCE;
}
static double equation_sqrt2(double x) { 
    return x * x - 2.0; 
}

static Status calculate_gamma_series(double epsilon, double *result)
{
    double sum = 0.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; n < MAX_ITERATIONS; n++) {
        double term = 1.0 / (double)n - log1p(1.0 / (double)n); sum += term;
        if (1.0 / (2.0 * (double)n) < epsilon) { 
            *result = sum;
            return STATUS_OK; 
        }
    }
    return STATUS_NO_CONVERGENCE;
}

static Status calculate_gamma_limit(double epsilon, double *result)
{
    double harmonic = 0.0;
    int n;
    if (result == NULL || epsilon <= 0.0 || !isfinite(epsilon)) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (n = 1; n < MAX_ITERATIONS; n++) {
        harmonic += 1.0 / (double)n;
        if (1.0 / (2.0 * (double)n) < epsilon) { 
            *result = harmonic - log((double)n); 
            return STATUS_OK;
        }
    }
    return STATUS_NO_CONVERGENCE;
}
static double equation_gamma(double x) { 
    return x - 0.5772156649015329; 
}

static void print_result(const char *name, Status status, double value)
{
    if (status == STATUS_OK) {
        printf("%-18s %.10f\n", name, value);
    }
    else {
        printf("%-18s ошибка (код %d)\n", name, status);
    }
}

int main(int argc, char *argv[])
{
    char *end = NULL;
    double epsilon, value;
    Status status;
    if (argc != 2) { 
        printf("Использование: %s <epsilon>\n", argv[0]); 
        return STATUS_INVALID_ARGUMENT; 
    }
    epsilon = strtod(argv[1], &end);
    if (end == argv[1] || *end != '\0' || !isfinite(epsilon) || epsilon <= 0.0) {
        printf("Ошибка: epsilon должен быть конечным числом больше нуля.\n");
        return STATUS_INVALID_ARGUMENT;
    }
    status = calculate_e_series(epsilon, &value); 
    print_result("e (ряд):", status, value);
    
    status = calculate_e_limit(epsilon, &value);
    print_result("e (предел):", status, value);
    
    status = find_root_bisection(equation_e, 2.0, 3.0, epsilon, &value); 
    print_result("e (уравнение):", status, value);
    
    status = calculate_pi_series(epsilon, &value);
    print_result("pi (ряд):", status, value);
    
    status = calculate_pi_limit(epsilon, &value);
    print_result("pi (предел):", status, value);
    
    status = find_root_bisection(equation_pi, 2.0, 4.0, epsilon, &value);
    print_result("pi (уравнение):", status, value);
    
    status = calculate_ln2_limit(epsilon, &value);
    print_result("ln 2 (предел):", status, value);
    
    status = calculate_ln2_series(epsilon, &value); 
    print_result("ln 2 (ряд):", status, value);
    
    status = find_root_bisection(equation_ln2, 0.0, 1.0, epsilon, &value);
    print_result("ln 2 (уравнение):", status, value);
    
    status = calculate_sqrt2_series(epsilon, &value); 
    print_result("sqrt 2 (ряд):", status, value);
    
    status = calculate_sqrt2_limit(epsilon, &value);
    print_result("sqrt 2 (предел):", status, value);
    
    status = find_root_bisection(equation_sqrt2, 1.0, 2.0, epsilon, &value);
    print_result("sqrt 2 (уравнение):", status, value);
    
    status = calculate_gamma_series(epsilon, &value); 
    print_result("gamma (ряд):", status, value);
    
    status = calculate_gamma_limit(epsilon, &value); 
    print_result("gamma (предел):", status, value);
    
    status = find_root_bisection(equation_gamma, 0.0, 1.0, epsilon, &value);
    print_result("gamma (уравнение):", status, value);
    
    return STATUS_OK;
}
