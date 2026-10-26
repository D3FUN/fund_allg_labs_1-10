#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, FILE_ERROR, RANGE_ERROR, DIVISION_ERROR } Status;

typedef struct Term
{
    long long coefficient;
    unsigned degree;
    struct Term *next;
} Term;

static Status clear_poly(Term *poly)
{
    while (poly != NULL)
    {
        Term *next = poly->next;
        free(poly);
        poly = next;
    }
    return OK;
}

static Status add_number(long long a, long long b, long long *result)
{
    if (result == NULL) return INPUT_ERROR;
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) return RANGE_ERROR;
    *result = a + b;
    return OK;
}

static Status multiply_number(long long a, long long b, long long *result)
{
    if (result == NULL) return INPUT_ERROR;
    if (a > 0)
    {
        if ((b > 0 && a > LLONG_MAX / b) || (b < 0 && b < LLONG_MIN / a)) return RANGE_ERROR;
    }
    else if (a < 0)
    {
        if ((b > 0 && a < LLONG_MIN / b) || (b < 0 && a < LLONG_MAX / b)) return RANGE_ERROR;
    }
    *result = a * b;
    return OK;
}

static Status insert_term(Term **poly, long long coefficient, unsigned degree)
{
    if (poly == NULL) return INPUT_ERROR;
    if (coefficient == 0) return OK;
    while (*poly != NULL && (*poly)->degree > degree) poly = &(*poly)->next;
    if (*poly != NULL && (*poly)->degree == degree)
    {
        long long sum;
        Status status = add_number((*poly)->coefficient, coefficient, &sum);
        if (status != OK) return status;
        if (sum == 0)
        {
            Term *removed = *poly;
            *poly = removed->next;
            free(removed);
        }
        else (*poly)->coefficient = sum;
        return OK;
    }
    Term *term = malloc(sizeof(*term));
    if (term == NULL) return MEMORY_ERROR;
    term->coefficient = coefficient;
    term->degree = degree;
    term->next = *poly;
    *poly = term;
    return OK;
}

static Status subtract_term(Term **poly, long long coefficient, unsigned degree)
{
    if (poly == NULL) return INPUT_ERROR;
    Term **position = poly;
    while (*position != NULL && (*position)->degree > degree) position = &(*position)->next;
    if (*position != NULL && (*position)->degree == degree)
    {
        long long old = (*position)->coefficient;
        if ((coefficient > 0 && old < LLONG_MIN + coefficient) ||
            (coefficient < 0 && old > LLONG_MAX + coefficient)) return RANGE_ERROR;
        long long difference = old - coefficient;
        if (difference == 0)
        {
            Term *removed = *position;
            *position = removed->next;
            free(removed);
        }
        else (*position)->coefficient = difference;
        return OK;
    }
    if (coefficient == LLONG_MIN) return RANGE_ERROR;
    return insert_term(poly, -coefficient, degree);
}

static Status copy_poly(const Term *source, Term **result)
{
    if (result == NULL) return INPUT_ERROR;
    *result = NULL;
    Status status = OK;
    for (; source != NULL && status == OK; source = source->next)
        status = insert_term(result, source->coefficient, source->degree);
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status add_poly(const Term *a, const Term *b, int sign, Term **result)
{
    if (result == NULL || (sign != 1 && sign != -1)) return INPUT_ERROR;
    Status status = copy_poly(a, result);
    for (; b != NULL && status == OK; b = b->next)
    {
        if (sign == 1) status = insert_term(result, b->coefficient, b->degree);
        else status = subtract_term(result, b->coefficient, b->degree);
    }
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status multiply_poly(const Term *a, const Term *b, Term **result)
{
    if (result == NULL) return INPUT_ERROR;
    *result = NULL;
    Status status = OK;
    for (const Term *left = a; left != NULL && status == OK; left = left->next)
        for (const Term *right = b; right != NULL && status == OK; right = right->next)
        {
            long long coefficient;
            status = multiply_number(left->coefficient, right->coefficient, &coefficient);
            if (status == OK && left->degree > UINT_MAX - right->degree) status = RANGE_ERROR;
            if (status == OK) status = insert_term(result, coefficient, left->degree + right->degree);
        }
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status divide_poly(const Term *a, const Term *b, Term **quotient, Term **remainder)
{
    if (quotient == NULL || remainder == NULL || quotient == remainder) return INPUT_ERROR;
    *quotient = *remainder = NULL;
    if (b == NULL) return DIVISION_ERROR;
    Status status = copy_poly(a, remainder);
    while (status == OK && *remainder != NULL && (*remainder)->degree >= b->degree)
    {
        long long leading = (*remainder)->coefficient;
        if (leading == LLONG_MIN && b->coefficient == -1) { status = RANGE_ERROR; break; }
        if (leading % b->coefficient != 0) { status = DIVISION_ERROR; break; }
        long long coefficient = leading / b->coefficient;
        unsigned degree = (*remainder)->degree - b->degree;
        status = insert_term(quotient, coefficient, degree);
        for (const Term *term = b; term != NULL && status == OK; term = term->next)
        {
            long long product;
            status = multiply_number(coefficient, term->coefficient, &product);
            if (status == OK) status = subtract_term(remainder, product, degree + term->degree);
        }
    }
    if (status != OK)
    {
        clear_poly(*quotient);
        clear_poly(*remainder);
        *quotient = *remainder = NULL;
    }
    return status;
}

static Status derivative(const Term *poly, Term **result)
{
    if (result == NULL) return INPUT_ERROR;
    *result = NULL;
    Status status = OK;
    for (; poly != NULL && status == OK; poly = poly->next)
        if (poly->degree != 0)
        {
            long long coefficient;
            status = multiply_number(poly->coefficient, poly->degree, &coefficient);
            if (status == OK) status = insert_term(result, coefficient, poly->degree - 1);
        }
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status power_poly(const Term *poly, unsigned power, Term **result)
{
    if (result == NULL) return INPUT_ERROR;
    *result = NULL;
    Term *base = NULL, *value = NULL;
    Status status = copy_poly(poly, &base);
    if (status == OK) status = insert_term(&value, 1, 0);
    while (status == OK && power != 0)
    {
        Term *temporary = NULL;
        if (power & 1U)
        {
            status = multiply_poly(value, base, &temporary);
            if (status == OK) { clear_poly(value); value = temporary; }
        }
        power >>= 1;
        if (status == OK && power)
        {
            temporary = NULL;
            status = multiply_poly(base, base, &temporary);
            if (status == OK) { clear_poly(base); base = temporary; }
        }
    }
    clear_poly(base);
    if (status != OK) clear_poly(value);
    else *result = value;
    return status;
}

static Status compose_poly(const Term *a, const Term *b, Term **result)
{
    if (result == NULL) return INPUT_ERROR;
    *result = NULL;
    Status status = OK;
    for (; a != NULL && status == OK; a = a->next)
    {
        Term *power = NULL;
        status = power_poly(b, a->degree, &power);
        for (const Term *term = power; term != NULL && status == OK; term = term->next)
        {
            long long coefficient;
            status = multiply_number(a->coefficient, term->coefficient, &coefficient);
            if (status == OK) status = insert_term(result, coefficient, term->degree);
        }
        clear_poly(power);
    }
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status evaluate(const Term *poly, double point, double *result)
{
    if (result == NULL || !isfinite(point)) return INPUT_ERROR;
    double value = 0.0;
    for (; poly != NULL; poly = poly->next)
    {
        double term = (double)poly->coefficient * pow(point, (double)poly->degree);
        value += term;
        if (!isfinite(term) || !isfinite(value)) return RANGE_ERROR;
    }
    *result = value;
    return OK;
}

static Status parse_poly(const char *text, Term **result)
{
    if (text == NULL || *text == '\0' || result == NULL) return INPUT_ERROR;
    *result = NULL;
    Status status = OK;
    while (*text && status == OK)
    {
        int negative = 0;
        if (*text == '+' || *text == '-') { negative = *text == '-'; text++; }
        if (*text == '\0') { status = INPUT_ERROR; break; }
        unsigned long long magnitude = 0;
        unsigned long long limit = (unsigned long long)LLONG_MAX + (unsigned)negative;
        int digits = 0;
        while (*text >= '0' && *text <= '9')
        {
            unsigned digit = (unsigned)(*text - '0');
            if (magnitude > (limit - digit) / 10) { status = RANGE_ERROR; break; }
            magnitude = magnitude * 10 + digit;
            digits = 1;
            text++;
        }
        if (status != OK) break;
        unsigned degree = 0;
        if (*text == 'x')
        {
            text++;
            degree = 1;
            if (!digits) magnitude = 1;
            if (*text == '^')
            {
                text++;
                degree = 0;
                if (*text < '0' || *text > '9') { status = INPUT_ERROR; break; }
                while (*text >= '0' && *text <= '9')
                {
                    unsigned digit = (unsigned)(*text - '0');
                    if (degree > (UINT_MAX - digit) / 10) { status = RANGE_ERROR; break; }
                    degree = degree * 10 + digit;
                    text++;
                }
            }
        }
        else if (!digits) status = INPUT_ERROR;
        if (status != OK) break;
        long long coefficient;
        if (negative && magnitude == (unsigned long long)LLONG_MAX + 1ULL) coefficient = LLONG_MIN;
        else coefficient = negative ? -(long long)magnitude : (long long)magnitude;
        status = insert_term(result, coefficient, degree);
        if (*text && *text != '+' && *text != '-') status = INPUT_ERROR;
    }
    if (status != OK) { clear_poly(*result); *result = NULL; }
    return status;
}

static Status print_poly(FILE *file, const Term *poly)
{
    if (file == NULL) return INPUT_ERROR;
    if (poly == NULL) return fputs("0\n", file) == EOF ? FILE_ERROR : OK;
    int first = 1;
    for (; poly != NULL; poly = poly->next)
    {
        long long coefficient = poly->coefficient;
        unsigned long long magnitude = coefficient < 0 ? 0ULL - (unsigned long long)coefficient : (unsigned long long)coefficient;
        if (coefficient < 0) { if (fputc('-', file) == EOF) return FILE_ERROR; }
        else if (!first && fputc('+', file) == EOF) return FILE_ERROR;
        if (magnitude != 1 || poly->degree == 0)
            if (fprintf(file, "%llu", magnitude) < 0) return FILE_ERROR;
        if (poly->degree != 0 && fputc('x', file) == EOF) return FILE_ERROR;
        if (poly->degree > 1 && fprintf(file, "^%u", poly->degree) < 0) return FILE_ERROR;
        first = 0;
    }
    return fputc('\n', file) == EOF ? FILE_ERROR : OK;
}

static Status execute(char *instruction, Term **accumulator, int *is_value, double *value)
{
    if (instruction == NULL || accumulator == NULL || is_value == NULL || value == NULL) return INPUT_ERROR;
    *is_value = 0;
    char *opening = strchr(instruction, '(');
    size_t length = strlen(instruction);
    if (opening == NULL || length == 0 || instruction[length - 1] != ')') return INPUT_ERROR;
    *opening = '\0';
    instruction[length - 1] = '\0';
    char *first = opening + 1, *second = strchr(first, ',');
    if (second != NULL) { *second++ = '\0'; if (strchr(second, ',') != NULL) return INPUT_ERROR; }
    Term *a = NULL, *b = NULL, *result = NULL, *remainder = NULL;
    Status status = OK;
    if (strcmp(instruction, "Diff") == 0)
    {
        if (second != NULL) status = INPUT_ERROR;
        else status = *first ? parse_poly(first, &a) : copy_poly(*accumulator, &a);
        if (status == OK) status = derivative(a, &result);
    }
    else if (strcmp(instruction, "Eval") == 0)
    {
        status = second ? parse_poly(first, &a) : copy_poly(*accumulator, &a);
        const char *point_text = second ? second : first;
        char *end = NULL;
        double point = strtod(point_text, &end);
        if (*point_text == '\0' || *end != '\0' || !isfinite(point)) status = INPUT_ERROR;
        if (status == OK) status = evaluate(a, point, value);
        if (status == OK) *is_value = 1;
    }
    else
    {
        status = second ? parse_poly(first, &a) : copy_poly(*accumulator, &a);
        if (status == OK) status = parse_poly(second ? second : first, &b);
        if (status == OK)
        {
            if (strcmp(instruction, "Add") == 0) status = add_poly(a, b, 1, &result);
            else if (strcmp(instruction, "Sub") == 0) status = add_poly(a, b, -1, &result);
            else if (strcmp(instruction, "Mult") == 0) status = multiply_poly(a, b, &result);
            else if (strcmp(instruction, "Cmps") == 0) status = compose_poly(a, b, &result);
            else if (strcmp(instruction, "Div") == 0 || strcmp(instruction, "Mod") == 0)
            {
                status = divide_poly(a, b, &result, &remainder);
                if (status == OK && strcmp(instruction, "Mod") == 0)
                {
                    clear_poly(result);
                    result = remainder;
                    remainder = NULL;
                }
            }
            else status = INPUT_ERROR;
        }
    }
    clear_poly(a);
    clear_poly(b);
    clear_poly(remainder);
    if (status == OK && !*is_value) { clear_poly(*accumulator); *accumulator = result; }
    else clear_poly(result);
    return status;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s instructions.txt\n", argv[0]); return 1; }
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) { fprintf(stderr, "Cannot open input\n"); return 1; }
    size_t capacity = 64, length = 0, number = 0;
    char *instruction = malloc(capacity);
    Status status = instruction == NULL ? MEMORY_ERROR : OK;
    Term *accumulator = NULL;
    int line_comment = 0, block_comment = 0, c;
    while (status == OK && (c = fgetc(file)) != EOF)
    {
        if (line_comment) { if (c == '\n') line_comment = 0; continue; }
        if (block_comment)
        {
            if (c == '[') status = INPUT_ERROR;
            if (c == ']') block_comment = 0;
            continue;
        }
        if (c == '%') { line_comment = 1; continue; }
        if (c == '[') { block_comment = 1; continue; }
        if (isspace((unsigned char)c)) continue;
        if (c == 0 || c == ']') { status = INPUT_ERROR; break; }
        if (c == ';')
        {
            instruction[length] = '\0';
            int is_value;
            double value = 0.0;
            number++;
            status = execute(instruction, &accumulator, &is_value, &value);
            if (status == OK)
            {
                if (is_value) { if (printf("%.12g\n", value) < 0) status = FILE_ERROR; }
                else status = print_poly(stdout, accumulator);
            }
            length = 0;
        }
        else
        {
            if (length == 1048576) { status = RANGE_ERROR; break; }
            if (length + 1 == capacity)
            {
                char *temporary = realloc(instruction, capacity * 2);
                if (temporary == NULL) { status = MEMORY_ERROR; break; }
                instruction = temporary;
                capacity *= 2;
            }
            instruction[length++] = (char)c;
        }
    }
    if (status == OK && (block_comment || length)) status = INPUT_ERROR;
    if (ferror(file)) status = FILE_ERROR;
    if (fclose(file) != 0) status = FILE_ERROR;
    if (fflush(stdout) != 0) status = FILE_ERROR;
    free(instruction);
    clear_poly(accumulator);
    if (status != OK) fprintf(stderr, "Error %d near instruction %zu\n", status, number + (length != 0));
    return status == OK ? 0 : 1;
}
