#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL,
               LEN_J, LEN_Z, LEN_T, LEN_BIG_L } Length;

typedef struct {
    int suppress;
    size_t width;
    Length length;
    char conversion;
    char custom;
} Spec;

static int read_char(FILE *stream, size_t *total)
{
    int c = fgetc(stream);
    if (c != EOF) ++*total;
    return c;
}

static void put_back(int c, FILE *stream, size_t *total)
{
    if (c != EOF && ungetc(c, stream) != EOF) --*total;
}

static int skip_spaces(FILE *stream, size_t *total)
{
    int c;
    do { c = read_char(stream, total); }
    while (c != EOF && isspace((unsigned char)c));
    put_back(c, stream, total);
    return c;
}

/* Возвращает указатель сразу после спецификатора или NULL при ошибке. */
static const char *parse_spec(const char *p, Spec *spec)
{
    *spec = (Spec){0, SIZE_MAX, LEN_NONE, 0, 0};
    if (*p == '*') { spec->suppress = 1; ++p; }
    if (isdigit((unsigned char)*p)) {
        spec->width = 0;
        while (isdigit((unsigned char)*p)) {
            unsigned digit = (unsigned)(*p++ - '0');
            if (spec->width > (SIZE_MAX - digit) / 10) return NULL;
            spec->width = spec->width * 10 + digit;
        }
        if (spec->width == 0) return NULL;
    }
    if ((p[0] == 'R' && p[1] == 'o') ||
        (p[0] == 'Z' && p[1] == 'r') ||
        (p[0] == 'C' && (p[1] == 'v' || p[1] == 'V'))) {
        spec->custom = p[0] == 'C' ? p[1] : p[0];
        return p + 2;
    }
    if (*p == 'h') {
        ++p; spec->length = LEN_H;
        if (*p == 'h') { ++p; spec->length = LEN_HH; }
    } else if (*p == 'l') {
        ++p; spec->length = LEN_L;
        if (*p == 'l') { ++p; spec->length = LEN_LL; }
    } else if (*p == 'j') { ++p; spec->length = LEN_J; }
    else if (*p == 'z') { ++p; spec->length = LEN_Z; }
    else if (*p == 't') { ++p; spec->length = LEN_T; }
    else if (*p == 'L') { ++p; spec->length = LEN_BIG_L; }
    if (*p == '\0') return NULL;
    spec->conversion = *p++;
    if (spec->conversion == '[') {
        if (*p == '^') ++p;
        if (*p == ']') ++p;
        while (*p != '\0' && *p != ']') ++p;
        if (*p != ']') return NULL;
        ++p;
    }
    if (strchr("diouxXn", spec->conversion) != NULL) {
        if (spec->length == LEN_BIG_L) return NULL;
    } else if (strchr("aAeEfFgG", spec->conversion) != NULL) {
        if (spec->length != LEN_NONE && spec->length != LEN_L &&
            spec->length != LEN_BIG_L) return NULL;
    } else if (strchr("cs[", spec->conversion) != NULL) {
        if (spec->length != LEN_NONE && spec->length != LEN_L) return NULL;
    } else if (spec->conversion == 'p') {
        if (spec->length != LEN_NONE) return NULL;
    } else return NULL;
    return p;
}

static int roman_value(int c)
{
    switch (c) {
        case 'I': return 1; case 'V': return 5; case 'X': return 10;
        case 'L': return 50; case 'C': return 100;
        case 'D': return 500; case 'M': return 1000;
        default: return 0;
    }
}

static int parse_roman(const char *text, int *answer)
{
    int value = 0;
    size_t len = strlen(text);
    if (len == 0) return 0;
    for (size_t i = 0; i < len; ++i) {
        int a = roman_value((unsigned char)text[i]);
        int b = roman_value((unsigned char)text[i + 1]);
        value += a < b ? -a : a;
    }
    if (value < 1 || value > 3999) return 0;
    const int values[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
    const char *letters[] = {"M","CM","D","CD","C","XC","L","XL",
                             "X","IX","V","IV","I"};
    char canonical[32] = "";
    int rest = value;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        while (rest >= values[i]) {
            strcat(canonical, letters[i]);
            rest -= values[i];
        }
    }
    if (strcmp(text, canonical) != 0) return 0;
    *answer = value;
    return 1;
}

static int digit_value(int c, int uppercase)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (!uppercase && c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (uppercase && c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

/* Успех 1, несовпадение 0, конец ввода EOF. */
static int read_custom(FILE *stream, size_t *total, const Spec *spec,
                       int base, int *signed_answer, unsigned *unsigned_answer)
{
    if (skip_spaces(stream, total) == EOF) return EOF;
    size_t count = 0;
    int c = read_char(stream, total);
    if (spec->custom == 'v' || spec->custom == 'V') {
        int negative = 0;
        if (c == '+' || c == '-') {
            negative = c == '-'; ++count;
            c = count < spec->width ? read_char(stream, total) : EOF;
        }
        uintmax_t value = 0;
        uintmax_t limit = negative ? (uintmax_t)INT_MAX + 1 : INT_MAX;
        int digits = 0, overflow = 0;
        while (c != EOF && count < spec->width) {
            int digit = digit_value(c, spec->custom == 'V');
            if (digit < 0 || digit >= base) break;
            if (value > (limit - (unsigned)digit) / (unsigned)base) overflow = 1;
            else if (!overflow) value = value * (unsigned)base + (unsigned)digit;
            digits = 1; ++count;
            c = count < spec->width ? read_char(stream, total) : EOF;
        }
        put_back(c, stream, total);
        if (!digits || overflow) return 0;
        *signed_answer = negative ? (value == (uintmax_t)INT_MAX + 1 ?
                                     INT_MIN : -(int)value) : (int)value;
        return 1;
    }
    char token[128];
    size_t stored = 0;
    int too_long = 0;
    while (c != EOF && count < spec->width) {
        int valid = spec->custom == 'R' ? roman_value(c) != 0 : (c == '0' || c == '1');
        if (!valid) break;
        if (stored + 1 < sizeof(token)) token[stored++] = (char)c;
        else too_long = 1;
        ++count;
        if (spec->custom == 'Z' && stored >= 2 &&
            token[stored - 1] == '1' && token[stored - 2] == '1') {
            c = EOF; /* Оба символа терминатора уже прочитаны. */
            break;
        }
        c = count < spec->width ? read_char(stream, total) : EOF;
    }
    put_back(c, stream, total);
    token[stored] = '\0';
    if (too_long || stored == 0) return 0;
    if (spec->custom == 'R') return parse_roman(token, signed_answer);
    if (stored < 2 || token[stored - 1] != '1') return 0;
    if (strcmp(token, "01") == 0) { *unsigned_answer = 0; return 1; }
    if (token[stored - 2] != '1') return 0;
    uintmax_t a = 1, b = 2, value = 0;
    for (size_t i = 0; i + 1 < stored; ++i) {
        if (i > 0 && token[i] == '1' && token[i - 1] == '1') return 0;
        if (token[i] == '1') {
            if (a > UINT_MAX || value > UINT_MAX - a) return 0;
            value += a;
        }
        if (i + 2 < stored) {
            if (a > UINTMAX_MAX - b) return 0;
            uintmax_t next = a + b;
            a = b; b = next;
        }
    }
    *unsigned_answer = (unsigned)value;
    return 1;
}

/* Тип указателя надо извлекать из va_list в точности как у вызывающего кода. */
#define STANDARD(type) do { \
    type *destination = va_arg(*args, type *); \
    if (destination == NULL) return -2; \
    return fscanf(stream, format, destination, used); \
} while (0)

#define SIGNED_SIZE_POINTER(args) _Generic((size_t)0, \
    unsigned int: va_arg(*(args), int *), \
    unsigned long: va_arg(*(args), long *), \
    unsigned long long: va_arg(*(args), long long *))
#define UNSIGNED_DIFF_POINTER(args) _Generic((ptrdiff_t)0, \
    int: va_arg(*(args), unsigned int *), \
    long: va_arg(*(args), unsigned long *), \
    long long: va_arg(*(args), unsigned long long *))

static int read_standard(FILE *stream, const char *format,
                         const Spec *spec, va_list *args, int *used)
{
    if (spec->suppress) return fscanf(stream, format, used);
    char c = spec->conversion;
    if (strchr("di", c) != NULL) {
        switch (spec->length) {
            case LEN_HH: STANDARD(signed char);
            case LEN_H: STANDARD(short);
            case LEN_L: STANDARD(long);
            case LEN_LL: STANDARD(long long);
            case LEN_J: STANDARD(intmax_t);
            case LEN_Z: {
                void *p = SIGNED_SIZE_POINTER(args);
                if (p == NULL) return -2;
                return fscanf(stream, format, p, used);
            }
            case LEN_T: STANDARD(ptrdiff_t);
            default: STANDARD(int);
        }
    }
    if (strchr("ouxX", c) != NULL) {
        switch (spec->length) {
            case LEN_HH: STANDARD(unsigned char);
            case LEN_H: STANDARD(unsigned short);
            case LEN_L: STANDARD(unsigned long);
            case LEN_LL: STANDARD(unsigned long long);
            case LEN_J: STANDARD(uintmax_t);
            case LEN_Z: STANDARD(size_t);
            case LEN_T: {
                void *p = UNSIGNED_DIFF_POINTER(args);
                if (p == NULL) return -2;
                return fscanf(stream, format, p, used);
            }
            default: STANDARD(unsigned int);
        }
    }
    if (strchr("aAeEfFgG", c) != NULL) {
        if (spec->length == LEN_BIG_L) STANDARD(long double);
        if (spec->length == LEN_L) STANDARD(double);
        STANDARD(float);
    }
    if (c == 'p') STANDARD(void *);
    if (spec->length == LEN_L) STANDARD(wchar_t);
    STANDARD(char);
}
#undef STANDARD

#define STORE_COUNT(type, maximum) do { \
    type *p = va_arg(*args, type *); \
    if (p == NULL || (uintmax_t)total > (uintmax_t)(maximum)) return 0; \
    *p = (type)total; return 1; \
} while (0)

static int store_count(const Spec *spec, va_list *args, size_t total)
{
    if (spec->suppress) return 1;
    switch (spec->length) {
        case LEN_HH: STORE_COUNT(signed char, SCHAR_MAX);
        case LEN_H: STORE_COUNT(short, SHRT_MAX);
        case LEN_L: STORE_COUNT(long, LONG_MAX);
        case LEN_LL: STORE_COUNT(long long, LLONG_MAX);
        case LEN_J: STORE_COUNT(intmax_t, INTMAX_MAX);
        case LEN_Z: {
            /* signed counterpart of size_t: chosen at compile time. */
            void *p = SIGNED_SIZE_POINTER(args);
            uintmax_t maximum = _Generic((size_t)0,
                unsigned int: (uintmax_t)INT_MAX,
                unsigned long: (uintmax_t)LONG_MAX,
                unsigned long long: (uintmax_t)LLONG_MAX);
            if (p == NULL || (uintmax_t)total > maximum) return 0;
            _Generic((size_t)0,
                unsigned int: (*(int *)p = (int)total),
                unsigned long: (*(long *)p = (long)total),
                unsigned long long: (*(long long *)p = (long long)total));
            return 1;
        }
        case LEN_T: STORE_COUNT(ptrdiff_t, PTRDIFF_MAX);
        default: STORE_COUNT(int, INT_MAX);
    }
}
#undef STORE_COUNT

static int scan_stream(FILE *stream, const char *format, va_list *args)
{
    if (stream == NULL || format == NULL) return EOF;
    size_t length = strlen(format);
    if (length > SIZE_MAX - 3) return EOF;
    char *part = malloc(length + 3);
    if (part == NULL) return EOF;
    size_t total = 0;
    int assigned = 0, input_failure = 0, invalid_argument = 0;
    const char *p = format;
    while (*p != '\0') {
        if (isspace((unsigned char)*p)) {
            do { ++p; } while (isspace((unsigned char)*p));
            skip_spaces(stream, &total);
        } else if (*p != '%' || p[1] == '%') {
            int expected = *p;
            if (*p == '%') { ++p; skip_spaces(stream, &total); }
            ++p;
            int c = read_char(stream, &total);
            if (c != expected) {
                put_back(c, stream, &total);
                input_failure = c == EOF;
                break;
            }
        } else {
            const char *start = p++;
            Spec spec;
            p = parse_spec(p, &spec);
            if (p == NULL) { invalid_argument = 1; break; }
            if (spec.custom != 0) {
                int *signed_target = NULL;
                unsigned *unsigned_target = NULL;
                if (!spec.suppress) {
                    if (spec.custom == 'Z') unsigned_target = va_arg(*args, unsigned *);
                    else signed_target = va_arg(*args, int *);
                    if (signed_target == NULL && unsigned_target == NULL) {
                        invalid_argument = 1; break;
                    }
                }
                int base = 10;
                if (spec.custom == 'v' || spec.custom == 'V') {
                    base = va_arg(*args, int);
                    if (base < 2 || base > 36) base = 10;
                }
                int signed_value = 0;
                unsigned unsigned_value = 0;
                int result = read_custom(stream, &total, &spec, base,
                                         &signed_value, &unsigned_value);
                if (result != 1) { input_failure = result == EOF; break; }
                if (!spec.suppress) {
                    if (unsigned_target != NULL) *unsigned_target = unsigned_value;
                    else *signed_target = signed_value;
                    ++assigned;
                }
            } else if (spec.conversion == 'n') {
                if (!store_count(&spec, args, total)) { invalid_argument = 1; break; }
            } else {
                size_t size = (size_t)(p - start);
                memcpy(part, start, size);
                memcpy(part + size, "%n", 3);
                int used = -1;
                int result = read_standard(stream, part, &spec, args, &used);
                if (used < 0) {
                    input_failure = result == EOF;
                    invalid_argument = result == -2;
                    break;
                }
                total += (size_t)used;
                if (!spec.suppress) ++assigned;
            }
        }
    }
    free(part);
    return invalid_argument || (input_failure && assigned == 0) ? EOF : assigned;
}

int overfscanf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = scan_stream(stream, format, &args);
    va_end(args);
    return result;
}

int oversscanf(const char *text, const char *format, ...)
{
    if (text == NULL || format == NULL) return EOF;
    FILE *stream = tmpfile();
    if (stream == NULL) return EOF;
    size_t size = strlen(text);
    if (fwrite(text, 1, size, stream) != size || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return EOF;
    }
    va_list args;
    va_start(args, format);
    int result = scan_stream(stream, format, &args);
    va_end(args);
    fclose(stream);
    return result;
}

#ifndef LAB_NO_MAIN
int main(void)
{
    int roman = 0, lower = 0, upper = 0, usual = 0;
    unsigned zeckendorf = 0;
    int result = oversscanf("XIV 1011 -ff FF 42", "%Ro %Zr %Cv %CV %d",
                            &roman, &zeckendorf, &lower, 16, &upper, 16, &usual);
    if (result != 5) {
        fprintf(stderr, "Ошибка чтения демонстрационных данных.\n");
        return 1;
    }
    printf("Строка: %d %u %d %d %d\n", roman, zeckendorf, lower, upper, usual);
    FILE *stream = tmpfile();
    if (stream == NULL) return 1;
    if (fputs("IX 01 10 A", stream) == EOF || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream); return 1;
    }
    result = overfscanf(stream, "%Ro %Zr %Cv %CV", &roman, &zeckendorf,
                        &lower, 2, &upper, 16);
    fclose(stream);
    if (result != 4) return 1;
    printf("Файл: %d %u %d %d\n", roman, zeckendorf, lower, upper);
    return 0;
}
#endif
