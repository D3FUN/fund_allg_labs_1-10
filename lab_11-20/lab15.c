#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* A destination is either a stream or a caller-owned sprintf buffer. */
typedef struct {
    FILE *file;
    char *buffer;
    size_t count;
} Output;

typedef enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL,
               LEN_J, LEN_Z, LEN_T, LEN_BIG_L } Length;

typedef struct {
    size_t size;
    Length length;
    char conversion;
    int stars;
    int decorated;
} Specification;

static int write_output(Output *out, const char *text, size_t size)
{
    if (size > (size_t)INT_MAX - out->count) return -1;
    if (out->file != NULL) {
        if (fwrite(text, 1, size, out->file) != size) return -1;
    } else {
        memcpy(out->buffer + out->count, text, size);
        out->buffer[out->count + size] = '\0';
    }
    out->count += size;
    return 0;
}

static int parse_specification(const char *format, Specification *spec)
{
    size_t i = 1;
    spec->stars = 0;
    spec->decorated = 0;
    spec->length = LEN_NONE;
    while (format[i] != '\0' && strchr("-+ #0", format[i]) != NULL) {
        spec->decorated = 1;
        i++;
    }
    if (format[i] == '*') {
        spec->stars++;
        spec->decorated = 1;
        i++;
    } else {
        while (isdigit((unsigned char)format[i])) {
            spec->decorated = 1;
            i++;
        }
    }
    if (format[i] == '.') {
        spec->decorated = 1;
        i++;
        if (format[i] == '*') {
            spec->stars++;
            i++;
        } else {
            while (isdigit((unsigned char)format[i])) i++;
        }
    }
    if (format[i] == 'h') {
        i++;
        spec->length = LEN_H;
        if (format[i] == 'h') { spec->length = LEN_HH; i++; }
    } else if (format[i] == 'l') {
        i++;
        spec->length = LEN_L;
        if (format[i] == 'l') { spec->length = LEN_LL; i++; }
    } else if (format[i] == 'j') { spec->length = LEN_J; i++; }
    else if (format[i] == 'z') { spec->length = LEN_Z; i++; }
    else if (format[i] == 't') { spec->length = LEN_T; i++; }
    else if (format[i] == 'L') { spec->length = LEN_BIG_L; i++; }
    char c = format[i];
    if (c == '\0') return -1;
    int valid = 0;
    if (strchr("diuoxX", c) != NULL) valid = spec->length != LEN_BIG_L;
    else if (strchr("aAeEfFgG", c) != NULL)
        valid = spec->length == LEN_NONE || spec->length == LEN_L ||
                spec->length == LEN_BIG_L;
    else if (c == 'c' || c == 's')
        valid = spec->length == LEN_NONE || spec->length == LEN_L;
    else if (c == 'p') valid = spec->length == LEN_NONE;
    else if (c == 'n') valid = !spec->decorated && spec->length != LEN_BIG_L;
    else if (c == '%') valid = !spec->decorated && spec->length == LEN_NONE;
    if (!valid) return -1;
    spec->conversion = c;
    spec->size = i + 1;
    return 0;
}

/* The signed counterpart of size_t, and unsigned counterpart of ptrdiff_t,
   are selected without assuming that long has the same size as a pointer. */
#define TAKE_SIGNED_SIZE(ap) _Generic((size_t)0, \
    unsigned int: va_arg(*(ap), int), \
    unsigned long: va_arg(*(ap), long), \
    unsigned long long: va_arg(*(ap), long long))
#define TAKE_UNSIGNED_DIFF(ap) _Generic((ptrdiff_t)0, \
    int: va_arg(*(ap), unsigned int), \
    long: va_arg(*(ap), unsigned long), \
    long long: va_arg(*(ap), unsigned long long))

static int consume_standard(va_list *args, const Specification *spec)
{
    for (int i = 0; i < spec->stars; i++) (void)va_arg(*args, int);
    char c = spec->conversion;
    Length l = spec->length;
    if (c == 'd' || c == 'i') {
        switch (l) {
            case LEN_L: (void)va_arg(*args, long); break;
            case LEN_LL: (void)va_arg(*args, long long); break;
            case LEN_J: (void)va_arg(*args, intmax_t); break;
            case LEN_Z: (void)TAKE_SIGNED_SIZE(args); break;
            case LEN_T: (void)va_arg(*args, ptrdiff_t); break;
            default: (void)va_arg(*args, int); break;
        }
    } else if (strchr("uoxX", c) != NULL) {
        switch (l) {
            case LEN_L: (void)va_arg(*args, unsigned long); break;
            case LEN_LL: (void)va_arg(*args, unsigned long long); break;
            case LEN_J: (void)va_arg(*args, uintmax_t); break;
            case LEN_Z: (void)va_arg(*args, size_t); break;
            case LEN_T: (void)TAKE_UNSIGNED_DIFF(args); break;
            case LEN_HH:
                (void)_Generic(+(unsigned char)0,
                    int: va_arg(*args, int),
                    unsigned int: va_arg(*args, unsigned int)); break;
            case LEN_H:
                (void)_Generic(+(unsigned short)0,
                    int: va_arg(*args, int),
                    unsigned int: va_arg(*args, unsigned int)); break;
            default: (void)va_arg(*args, unsigned int); break;
        }
    } else if (strchr("aAeEfFgG", c) != NULL) {
        if (l == LEN_BIG_L) (void)va_arg(*args, long double);
        else (void)va_arg(*args, double);
    } else if (c == 'c') {
        if (l == LEN_L) (void)va_arg(*args, wint_t);
        else (void)va_arg(*args, int);
    } else if (c == 's') {
        if (l == LEN_L) return va_arg(*args, wchar_t *) == NULL ? -1 : 0;
        return va_arg(*args, char *) == NULL ? -1 : 0;
    } else if (c == 'p') (void)va_arg(*args, void *);
    return 0;
}

#define STORE_COUNT(type, maximum) do { \
    type *target = va_arg(*args, type *); \
    if (target == NULL || count > (uintmax_t)(maximum)) return -1; \
    *target = (type)count; \
} while (0)

static int save_count(va_list *args, Length length, size_t count)
{
    switch (length) {
        case LEN_HH: STORE_COUNT(signed char, SCHAR_MAX); break;
        case LEN_H: STORE_COUNT(short, SHRT_MAX); break;
        case LEN_L: STORE_COUNT(long, LONG_MAX); break;
        case LEN_LL: STORE_COUNT(long long, LLONG_MAX); break;
        case LEN_J: STORE_COUNT(intmax_t, INTMAX_MAX); break;
        case LEN_T: STORE_COUNT(ptrdiff_t, PTRDIFF_MAX); break;
        case LEN_Z:
            if (_Generic((size_t)0, unsigned int: 1, default: 0)) {
                STORE_COUNT(int, INT_MAX);
            } else if (_Generic((size_t)0, unsigned long: 1, default: 0)) {
                STORE_COUNT(long, LONG_MAX);
            } else { STORE_COUNT(long long, LLONG_MAX); }
            break;
        default: STORE_COUNT(int, INT_MAX); break;
    }
    return 0;
}
#undef STORE_COUNT

static int print_standard(Output *out, const char *format,
                          const Specification *spec, va_list *args)
{
    if (spec->conversion == 'n') return save_count(args, spec->length, out->count);
    char *one = malloc(spec->size + 1);
    if (one == NULL) return -1;
    memcpy(one, format, spec->size);
    one[spec->size] = '\0';
    va_list copy;
    va_copy(copy, *args);
    /* Validate string pointers while advancing the original list. */
    int status = consume_standard(args, spec);
    if (status != 0) { va_end(copy); free(one); return -1; }
    va_list measure;
    va_copy(measure, copy);
    int needed = vsnprintf(NULL, 0, one, measure);
    va_end(measure);
    if (needed < 0) { va_end(copy); free(one); return -1; }
    char *text = malloc((size_t)needed + 1);
    if (text == NULL) { va_end(copy); free(one); return -1; }
    int written = vsnprintf(text, (size_t)needed + 1, one, copy);
    va_end(copy);
    free(one);
    status = written == needed ? write_output(out, text, (size_t)needed) : -1;
    free(text);
    return status;
}

static int roman_number(int number, char *result)
{
    if (number < 1 || number > 3999 || result == NULL) return -1;
    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *letters[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL",
                             "X", "IX", "V", "IV", "I"};
    size_t pos = 0;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        while (number >= values[i]) {
            size_t len = strlen(letters[i]);
            memcpy(result + pos, letters[i], len);
            pos += len;
            number -= values[i];
        }
    }
    result[pos] = '\0';
    return 0;
}

static int zeckendorf(unsigned int number, char *result)
{
    if (result == NULL) return -1;
    if (number == 0) { strcpy(result, "01"); return 0; }
    unsigned int fib[sizeof(unsigned int) * CHAR_BIT * 2] = {1, 2};
    size_t count = 2;
    while (fib[count - 1] <= UINT_MAX - fib[count - 2]) {
        fib[count] = fib[count - 1] + fib[count - 2];
        count++;
    }
    while (count > 1 && fib[count - 1] > number) count--;
    memset(result, '0', count);
    for (size_t i = count; i > 0; i--) {
        if (fib[i - 1] <= number) {
            result[i - 1] = '1';
            number -= fib[i - 1];
        }
    }
    result[count] = '1';
    result[count + 1] = '\0';
    return 0;
}

static int normalized_base(int base)
{
    return base >= 2 && base <= 36 ? base : 10;
}

static int format_base(int number, int base, int uppercase, char *result)
{
    if (result == NULL) return -1;
    const char *digits = uppercase ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   : "0123456789abcdefghijklmnopqrstuvwxyz";
    char reverse[sizeof(int) * CHAR_BIT + 1];
    size_t count = 0, pos = 0;
    base = normalized_base(base);
    unsigned int value = number < 0 ? 0u - (unsigned int)number : (unsigned int)number;
    do {
        reverse[count++] = digits[value % (unsigned int)base];
        value /= (unsigned int)base;
    } while (value != 0);
    if (number < 0) result[pos++] = '-';
    while (count > 0) result[pos++] = reverse[--count];
    result[pos] = '\0';
    return 0;
}

static int parse_base(const char *text, int base, int uppercase, int *result)
{
    if (text == NULL || result == NULL) return -1;
    base = normalized_base(base);
    int negative = *text == '-';
    if (*text == '-' || *text == '+') text++;
    if (*text == '\0') return -1;
    unsigned int limit = negative ? 0u - (unsigned int)INT_MIN : (unsigned int)INT_MAX;
    unsigned int value = 0;
    while (*text != '\0') {
        unsigned char c = (unsigned char)*text++;
        unsigned int digit;
        if (c >= '0' && c <= '9') digit = (unsigned int)(c - '0');
        else if (uppercase && c >= 'A' && c <= 'Z') digit = (unsigned int)(c - 'A') + 10u;
        else if (!uppercase && c >= 'a' && c <= 'z') digit = (unsigned int)(c - 'a') + 10u;
        else return -1;
        if (digit >= (unsigned int)base || value > (limit - digit) / (unsigned int)base)
            return -1;
        value = value * (unsigned int)base + digit;
    }
    *result = negative ? (value == 0u - (unsigned int)INT_MIN ? INT_MIN : -(int)value)
                       : (int)value;
    return 0;
}

static int memory_dump(const void *value, size_t size, char *result)
{
    if (value == NULL || result == NULL) return -1;
    const unsigned char *bytes = value;
    size_t pos = 0;
    for (size_t i = 0; i < size; i++) {
        if (i != 0) result[pos++] = ' ';
        for (int bit = CHAR_BIT - 1; bit >= 0; bit--)
            result[pos++] = (bytes[i] & (1u << bit)) ? '1' : '0';
    }
    result[pos] = '\0';
    return 0;
}

static int is_custom(const char *f)
{
    return (f[0] == 'R' && f[1] == 'o') || (f[0] == 'Z' && f[1] == 'r') ||
           (f[0] == 'C' && (f[1] == 'v' || f[1] == 'V')) ||
           (f[0] == 't' && f[1] == 'o') || (f[0] == 'T' && f[1] == 'O') ||
           (f[0] == 'm' && (f[1] == 'i' || f[1] == 'u' ||
                             f[1] == 'd' || f[1] == 'f'));
}

static int print_custom(Output *out, const char *f, va_list *args)
{
    /* Large enough for a binary int, Fibonacci code or any dump below. */
    char text[sizeof(double) * (CHAR_BIT + 1) + sizeof(int) * CHAR_BIT * 2 + 32];
    int status = 0;
    if (f[0] == 'R') status = roman_number(va_arg(*args, int), text);
    else if (f[0] == 'Z') status = zeckendorf(va_arg(*args, unsigned int), text);
    else if (f[0] == 'C') {
        int value = va_arg(*args, int);
        int base = va_arg(*args, int);
        status = format_base(value, base, f[1] == 'V', text);
    } else if (f[0] == 't' || f[0] == 'T') {
        const char *value = va_arg(*args, char *);
        int base = va_arg(*args, int), number;
        status = parse_base(value, base, f[0] == 'T', &number);
        if (status == 0) status = format_base(number, 10, 0, text);
    } else if (f[1] == 'i') {
        int value = va_arg(*args, int);
        if (sizeof(value) != 4 || CHAR_BIT != 8) return -1;
        status = memory_dump(&value, sizeof(value), text);
    } else if (f[1] == 'u') {
        unsigned int value = va_arg(*args, unsigned int);
        if (sizeof(value) != 4 || CHAR_BIT != 8) return -1;
        status = memory_dump(&value, sizeof(value), text);
    } else if (f[1] == 'd') {
        double value = va_arg(*args, double);
        status = memory_dump(&value, sizeof(value), text);
    } else {
        /* float is promoted to double in variadic arguments. */
        double argument = va_arg(*args, double);
        if (isfinite(argument) && (argument > FLT_MAX || argument < -FLT_MAX)) return -1;
        float value = (float)argument;
        status = memory_dump(&value, sizeof(value), text);
    }
    return status == 0 ? write_output(out, text, strlen(text)) : -1;
}

static int format_output(Output *out, const char *format, va_list *args)
{
    if (format == NULL) return -1;
    while (*format != '\0') {
        if (*format != '%') {
            const char *start = format;
            while (*format != '\0' && *format != '%') format++;
            if (write_output(out, start, (size_t)(format - start)) != 0) return -1;
        } else if (is_custom(format + 1)) {
            if (print_custom(out, format + 1, args) != 0) return -1;
            format += 3;
        } else {
            Specification spec;
            if (parse_specification(format, &spec) != 0 ||
                print_standard(out, format, &spec, args) != 0) return -1;
            format += spec.size;
        }
    }
    return (int)out->count;
}

int overfprintf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) return -1;
    Output out = {stream, NULL, 0};
    va_list args;
    va_start(args, format);
    int result = format_output(&out, format, &args);
    va_end(args);
    return result;
}

int oversprintf(char *buffer, const char *format, ...)
{
    if (buffer == NULL || format == NULL) return -1;
    buffer[0] = '\0';
    Output out = {NULL, buffer, 0};
    va_list args;
    va_start(args, format);
    int result = format_output(&out, format, &args);
    va_end(args);
    return result;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    char text[2048];
    int count = oversprintf(text,
        "Roman: %Ro; Fibonacci: %Zr; bases: %Cv %CV; decimal: %to %TO\n"
        "int: %mi\nunsigned: %mu\ndouble: %md\nfloat: %mf\nStandard: %08d %.2f\n",
        2026, 10u, -255, 16, 255, 16, "ff", 16, "FF", 16,
        -1, 1u, 1.5, (double)1.5f, 42, 3.14159);
    if (count < 0) { fputs("Formatting error.\n", stderr); return 1; }
    if (overfprintf(stdout, "%sCharacters: %d; %Ro %Zr %Cv %CV %to %TO\n"
                           "%mi\n%mu\n%md\n%mf\n", text, count,
                    4, 3u, 31, 16, 31, 16, "10", 2, "A", 16,
                    0, 0u, 0.0, (double)0.0f) < 0) return 1;
    return fflush(stdout) == EOF;
}
#endif
