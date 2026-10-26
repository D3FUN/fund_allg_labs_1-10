#include "lab_input.h"

/* Addition through carry propagation, without arithmetic operators. */
Status bit_add(uint64_t a, uint64_t b, uint64_t *out)
{
    if (!out) return INPUT_ERROR;
    while (b) {
        uint64_t carry = (a & b) << 1;
        a ^= b;
        b = carry;
    }
    *out = a;
    return OK;
}

/* Higher groups are emitted first; at most 64 recursive calls. */
Status emit_digits(uint64_t number, unsigned r, char *out, uint64_t *length)
{
    if (!out || !length || r < 1 || r > 5 || *length > 64) return INPUT_ERROR;
    uint64_t high = number >> r;
    Status status = OK;
    if (high) status = emit_digits(high, r, out, length);
    if (status != OK || *length > 64) return status == OK ? RANGE_ERROR : status;
    uint64_t mask = ~(UINT64_MAX << r);
    out[*length] = "0123456789ABCDEFGHIJKLMNOPQRSTUV"[number & mask];
    return bit_add(*length, 1, length);
}

/* out must point to an array of at least 66 bytes. */
Status convert_base(int64_t number, unsigned r, char *out, size_t capacity)
{
    if (!out || capacity < 66 || r < 1 || r > 5) return INPUT_ERROR;
    uint64_t magnitude = (uint64_t)number, length = 0;
    if (number < 0) {
        out[length] = '-';
        bit_add(length, 1, &length);
        bit_add(~magnitude, 1, &magnitude);
    }
    Status status = emit_digits(magnitude, r, out, &length);
    if (status != OK) return status;
    out[length] = '\0';
    return OK;
}

int main(int argc, char **argv)
{
    if (argc != 1) { fprintf(stderr, "Usage: %s < input.txt\n", argv[0]); return 1; }
    char *line = NULL, *words[2];
    size_t count = 0;
    int64_t number = 0;
    uint64_t r = 0;
    Status status = read_line(stdin, &line);
    if (status == OK) status = split_words(line, words, 2, &count);
    if (status == OK && count != 2) status = INPUT_ERROR;
    if (status == OK) status = parse_signed(words[0], &number);
    if (status == OK) status = parse_unsigned(words[1], 5, &r);
    char result[66];
    if (status == OK) status = convert_base(number, (unsigned)r, result, sizeof(result));
    free(line);
    if (status != OK) { fprintf(stderr, "Error %d: expected integer and r (1..5).\n", status); return 1; }
    if (printf("%s\n", result) < 0 || fflush(stdout) == EOF) return 1;
    return 0;
}
