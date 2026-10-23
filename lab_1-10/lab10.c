#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_BIG 1000000000U

typedef enum {
    OK,
    MEMORY_ERROR,
    INPUT_ERROR,
    WRONG_NUMBER
} Status;


typedef struct {
    uint32_t *part;
    size_t size;
    size_t capacity;
    int sign;
} BigInt;

static void bigint_init(BigInt *number) {
    number->part = NULL;
    number->size = 0;
    number->capacity = 0;
    number->sign = 0;
}

static void bigint_free(BigInt *number) {
    free(number->part);
    bigint_init(number);
}

static Status bigint_reserve(BigInt *number, size_t needed) {
    uint32_t *new_part;
    size_t new_capacity;

    if (needed <= number->capacity) {
        return OK;
    }
    new_capacity = number->capacity;
    if (new_capacity == 0) {
        new_capacity = 4;
    }
    while (new_capacity < needed) {
        if (new_capacity > (size_t)-1 / 2) {
            return MEMORY_ERROR;
        }
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(*new_part)) {
        return MEMORY_ERROR;
    }
    new_part = realloc(number->part, new_capacity * sizeof(*new_part));
    if (new_part == NULL) {
        return MEMORY_ERROR;
    }
    number->part = new_part;
    number->capacity = new_capacity;
    return OK;
}

static void bigint_normalize(BigInt *number) {
    while (number->size > 0 && number->part[number->size - 1] == 0) {
        number->size--;
    }
    if (number->size == 0) {
        number->sign = 0;
    }
}

static Status bigint_copy(BigInt *to, const BigInt *from) {
    Status status = bigint_reserve(to, from->size);
    if (status != OK) {
        return status;
    }
    if (from->size > 0) {
        memcpy(to->part, from->part, from->size * sizeof(*to->part));
    }
    to->size = from->size;
    to->sign = from->sign;
    return OK;
}

static Status bigint_multiply_small(BigInt *number, unsigned int multiplier) {
    uint64_t carry = 0;
    size_t i;
    Status status;

    if (number->sign == 0 || multiplier == 1) {
        return OK;
    }
    if (multiplier == 0) {
        number->size = 0;
        number->sign = 0;
        return OK;
    }
    for (i = 0; i < number->size; i++) {
        uint64_t value = (uint64_t)number->part[i] * multiplier + carry;
        number->part[i] = (uint32_t)(value % BASE_BIG);
        carry = value / BASE_BIG;
    }
    if (carry > 0) {
        status = bigint_reserve(number, number->size + 1);
        if (status != OK) {
            return status;
        }
        number->part[number->size] = (uint32_t)carry;
        number->size++;
    }
    return OK;
}

static Status bigint_add_small(BigInt *number, unsigned int value) {
    uint64_t carry;
    size_t i;
    Status status;

    if (value == 0) {
        return OK;
    }
    if (number->sign == 0) {
        status = bigint_reserve(number, 1);
        if (status != OK) {
            return status;
        }
        number->part[0] = value;
        number->size = 1;
        number->sign = 1;
        return OK;
    }
    carry = value;
    i = 0;
    while (carry > 0 && i < number->size) {
        uint64_t result = number->part[i] + carry;
        number->part[i] = (uint32_t)(result % BASE_BIG);
        carry = result / BASE_BIG;
        i++;
    }
    if (carry > 0) {
        status = bigint_reserve(number, number->size + 1);
        if (status != OK) {
            return status;
        }
        number->part[number->size] = (uint32_t)carry;
        number->size++;
    }
    return OK;
}

static int compare_abs(const BigInt *first, const BigInt *second) {
    size_t i;
    if (first->size != second->size) {
        return first->size > second->size ? 1 : -1;
    }
    i = first->size;
    while (i > 0) {
        i--;
        if (first->part[i] != second->part[i]) {
            return first->part[i] > second->part[i] ? 1 : -1;
        }
    }
    return 0;
}

static Status add_abs(BigInt *result, const BigInt *first, const BigInt *second) {
    size_t i, max_size;
    uint64_t carry = 0;
    Status status;
    max_size = first->size > second->size ? first->size : second->size;
    status = bigint_reserve(result, max_size + 1);
    if (status != OK) {
        return status;
    }
    for (i = 0; i < max_size; i++) {
        uint64_t value = carry;
        if (i < first->size) {
            value += first->part[i];
        }
        if (i < second->size) {
            value += second->part[i];
        }
        result->part[i] = (uint32_t)(value % BASE_BIG);
        carry = value / BASE_BIG;
    }
    result->size = max_size;
    if (carry > 0) {
        result->part[result->size] = (uint32_t)carry;
        result->size++;
    }
    result->sign = 1;
    return OK;
}


static Status subtract_abs(BigInt *result, const BigInt *first, const BigInt *second) {
    size_t i;
    int64_t borrow = 0;
    Status status = bigint_reserve(result, first->size);
    if (status != OK) {
        return status;
    }
    for (i = 0; i < first->size; i++) {
        int64_t value = (int64_t)first->part[i] - borrow;
        if (i < second->size) {
            value -= second->part[i];
        }
        if (value < 0) {
            value += BASE_BIG;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result->part[i] = (uint32_t)value;
    }
    result->size = first->size;
    result->sign = 1;
    bigint_normalize(result);
    return OK;
}

static Status bigint_add(BigInt *answer, const BigInt *first, const BigInt *second) {
    BigInt temporary;
    Status status;
    int comparison;

    bigint_init(&temporary);
    if (first->sign == 0) {
        status = bigint_copy(&temporary, second);
    } else if (second->sign == 0) {
        status = bigint_copy(&temporary, first);
    } else if (first->sign == second->sign) {
        status = add_abs(&temporary, first, second);
        temporary.sign = first->sign;
    } else {
        comparison = compare_abs(first, second);
        if (comparison == 0) {
            status = OK;
        } else if (comparison > 0) {
            status = subtract_abs(&temporary, first, second);
            temporary.sign = first->sign;
        } else {
            status = subtract_abs(&temporary, second, first);
            temporary.sign = second->sign;
        }
    }
    if (status == OK) {
        bigint_free(answer);
        *answer = temporary;
    } else {
        bigint_free(&temporary);
    }
    return status;
}

static int digit_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'Z') {
        return character - 'A' + 10;
    }
    return -1;
}

static Status bigint_from_string(BigInt *number, const char *text, int base) {
    size_t i = 0;
    int sign = 1;
    Status status;

    bigint_free(number);
    if (text[0] == '+' || text[0] == '-') {
        if (text[0] == '-') {
            sign = -1;
        }
        i++;
    }
    if (text[i] == '\0') {
        return WRONG_NUMBER;
    }
    while (text[i] != '\0') {
        int digit = digit_value(text[i]);
        if (digit < 0 || digit >= base) {
            return WRONG_NUMBER;
        }
        status = bigint_multiply_small(number, (unsigned int)base);
        if (status != OK) {
            return status;
        }
        status = bigint_add_small(number, (unsigned int)digit);
        if (status != OK) {
            return status;
        }
        i++;
    }
    if (number->size > 0) {
        number->sign = sign;
    }
    return OK;
}

static unsigned int bigint_divide_small(BigInt *number, unsigned int divisor) {
    uint64_t remainder = 0;
    size_t i = number->size;
    while (i > 0) {
        uint64_t value;
        i--;
        value = remainder * BASE_BIG + number->part[i];
        number->part[i] = (uint32_t)(value / divisor);
        remainder = value % divisor;
    }
    bigint_normalize(number);
    return (unsigned int)remainder;
}

static Status bigint_to_string(const BigInt *number, int base, char **text) {
    BigInt temporary;
    char *result;
    size_t length = 0, capacity = 16;
    Status status;
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    bigint_init(&temporary);
    status = bigint_copy(&temporary, number);
    if (status != OK) {
        return status;
    }
    result = malloc(capacity);
    if (result == NULL) {
        bigint_free(&temporary);
        return MEMORY_ERROR;
    }
    if (temporary.sign == 0) {
        result[length++] = '0';
    } else {
        while (temporary.size > 0) {
            unsigned int remainder = bigint_divide_small(&temporary, (unsigned int)base);
            if (length + 3 > capacity) {
                char *new_result;
                capacity *= 2;
                new_result = realloc(result, capacity);
                if (new_result == NULL) {
                    free(result);
                    bigint_free(&temporary);
                    return MEMORY_ERROR;
                }
                result = new_result;
            }
            result[length++] = digits[remainder];
        }
        if (number->sign < 0) {
            result[length++] = '-';
        }
    }
    result[length] = '\0';
    for (size_t i = 0; i < length / 2; i++) {
        char saved = result[i];
        result[i] = result[length - 1 - i];
        result[length - 1 - i] = saved;
    }
    bigint_free(&temporary);
    *text = result;
    return OK;
}

static Status read_word(char **word) {
    size_t length = 0, capacity = 32;
    int character;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return MEMORY_ERROR;
    }
    do {
        character = getchar();
    } while (character != EOF && isspace((unsigned char)character));
    if (character == EOF) {
        free(buffer);
        return INPUT_ERROR;
    }
    while (character != EOF && !isspace((unsigned char)character)) {
        if (length + 1 >= capacity) {
            char *new_buffer;
            capacity *= 2;
            new_buffer = realloc(buffer, capacity);
            if (new_buffer == NULL) {
                free(buffer);
                return MEMORY_ERROR;
            }
            buffer = new_buffer;
        }
        buffer[length++] = (char)character;
        character = getchar();
    }
    buffer[length] = '\0';
    *word = buffer;
    return OK;
}

static void print_number(const char *name, const BigInt *number, int input_base) {
    int bases[] = {input_base, 9, 18, 27, 36};
    size_t i;
    printf("%s:\n", name);
    for (i = 0; i < sizeof(bases) / sizeof(bases[0]); i++) {
        char *text = NULL;
        if (bigint_to_string(number, bases[i], &text) == OK) {
            if (i == 0) {
                printf("  в исходной системе (основание %d): %s\n", bases[i], text);
            } else {
                printf("  в системе с основанием %d: %s\n", bases[i], text);
            }
            free(text);
        } else {
            printf("  Не удалось перевести число в систему с основанием %d.\n", bases[i]);
        }
    }
}

int main(void) {
    char *word = NULL;
    char *end;
    long input_base_long;
    int input_base;
    BigInt sum, maximum, current;
    int have_numbers = 0;
    Status status;

    bigint_init(&sum);
    bigint_init(&maximum);
    bigint_init(&current);

    printf("Введите основание системы счисления от 2 до 36: ");
    status = read_word(&word);
    if (status != OK) {
        printf("Ошибка: не удалось прочитать основание системы счисления.\n");
        return 1;
    }
    input_base_long = strtol(word, &end, 10);
    if (*word == '\0' || *end != '\0' || input_base_long < 2 || input_base_long > 36) {
        printf("Ошибка: основание должно быть целым числом от 2 до 36.\n");
        free(word);
        return 1;
    }
    input_base = (int)input_base_long;
    free(word);
    word = NULL;

    printf("Введите целые числа. Для окончания введите Stop.\n");
    while ((status = read_word(&word)) == OK) {
        if (strcmp(word, "Stop") == 0) {
            free(word);
            word = NULL;
            break;
        }
        status = bigint_from_string(&current, word, input_base);
        if (status == WRONG_NUMBER) {
            printf("Число \"%s\" не подходит для системы с основанием %d и пропущено.\n", word, input_base);
            free(word);
            word = NULL;
            continue;
        }
        free(word);
        word = NULL;
        if (status != OK) {
            printf("Ошибка: не удалось выделить память.\n");
            bigint_free(&sum);
            bigint_free(&maximum);
            bigint_free(&current);
            return 1;
        }
        if (!have_numbers || compare_abs(&current, &maximum) > 0) {
            status = bigint_copy(&maximum, &current);
            if (status != OK) {
                printf("Ошибка: не удалось выделить память.\n");
                bigint_free(&sum);
                bigint_free(&maximum);
                bigint_free(&current);
                return 1;
            }
        }
        status = bigint_add(&sum, &sum, &current);
        if (status != OK) {
            printf("Ошибка: не удалось выделить память.\n");
            bigint_free(&sum);
            bigint_free(&maximum);
            bigint_free(&current);
            return 1;
        }
        have_numbers = 1;
    }
    if (status == MEMORY_ERROR) {
        printf("Ошибка: не удалось выделить память.\n");
        bigint_free(&sum);
        bigint_free(&maximum);
        bigint_free(&current);
        return 1;
    }
    if (!have_numbers) {
        printf("Не введено ни одного корректного числа.\n");
    } else {
        print_number("Максимальное по модулю число", &maximum, input_base);
        print_number("Сумма всех введённых чисел", &sum, input_base);
    }
    bigint_free(&sum);
    bigint_free(&maximum);
    bigint_free(&current);
    return 0;
}
