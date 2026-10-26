#include "lab_input.h"

typedef struct {
    uint64_t id;
    int start, finish;
} Talk;

Status parse_time(const char *text, int *out)
{
    if (!text || !out || strlen(text) != 5 || text[2] != ':') return INPUT_ERROR;
    for (int i = 0; i < 5; i++)
        if (i != 2 && (text[i] < '0' || text[i] > '9')) return INPUT_ERROR;
    int hour = (text[0] - '0') * 10 + text[1] - '0';
    int minute = (text[3] - '0') * 10 + text[4] - '0';
    if (hour > 23 || minute > 59) return INPUT_ERROR;
    *out = hour * 60 + minute;
    return OK;
}

/* qsort requires a comparator returning an ordering, not a status. */
int by_finish(const void *left, const void *right)
{
    const Talk *a = left, *b = right;
    if (a->finish != b->finish) return (a->finish > b->finish) - (a->finish < b->finish);
    return (a->start > b->start) - (a->start < b->start);
}

Status max_talks(const Talk *talks, size_t count, size_t halls, size_t *out)
{
    if (!talks || !count || !halls || !out) return INPUT_ERROR;
    *out = 0;
    if (count > SIZE_MAX / sizeof(Talk)) return RANGE_ERROR;
    for (size_t i = 0; i < count; i++) {
        if (!talks[i].id || talks[i].start < 0 || talks[i].finish > 1439 ||
            talks[i].start >= talks[i].finish) return INPUT_ERROR;
        for (size_t j = 0; j < i; j++)
            if (talks[i].id == talks[j].id) return INPUT_ERROR;
    }
    if (halls > count) halls = count;
    if (halls > SIZE_MAX / sizeof(int)) return RANGE_ERROR;
    Talk *sorted = malloc(count * sizeof(Talk));
    int *ends = calloc(halls, sizeof(int));
    if (!sorted || !ends) { free(sorted); free(ends); return MEMORY_ERROR; }
    memcpy(sorted, talks, count * sizeof(Talk));
    qsort(sorted, count, sizeof(Talk), by_finish);
    for (size_t i = 0; i < count; i++) {
        size_t best = halls;
        for (size_t j = 0; j < halls; j++) {
            if (ends[j] <= sorted[i].start && (best == halls || ends[j] > ends[best]))
                best = j;
        }
        if (best != halls) { ends[best] = sorted[i].finish; (*out)++; }
    }
    free(sorted);
    free(ends);
    return OK;
}

Status read_talks(FILE *file, Talk **out, size_t *count, size_t *halls)
{
    if (!file || !out || !count || !halls) return INPUT_ERROR;
    *out = NULL;
    char *line = NULL, *words[3];
    size_t fields = 0;
    uint64_t n = 0, k = 0;
    Status status = read_line(file, &line);
    if (status == OK) status = split_words(line, words, 3, &fields);
    if (status == OK && fields != 2) status = INPUT_ERROR;
    if (status == OK) status = parse_unsigned(words[0], SIZE_MAX / sizeof(Talk), &n);
    if (status == OK) status = parse_unsigned(words[1], SIZE_MAX, &k);
    free(line);
    if (status != OK || !n || !k) return status == OK ? INPUT_ERROR : status;
    Talk *talks = malloc((size_t)n * sizeof(Talk));
    if (!talks) return MEMORY_ERROR;
    for (size_t i = 0; i < n && status == OK; i++) {
        line = NULL;
        status = read_line(file, &line);
        if (status == OK) status = split_words(line, words, 3, &fields);
        if (status == OK && fields != 3) status = INPUT_ERROR;
        if (status == OK) status = parse_unsigned(words[0], UINT64_MAX, &talks[i].id);
        if (status == OK) status = parse_time(words[1], &talks[i].start);
        if (status == OK) status = parse_time(words[2], &talks[i].finish);
        free(line);
    }
    if (status == OK) {
        int c;
        while ((c = fgetc(file)) != EOF)
            if (!isspace((unsigned char)c)) { status = INPUT_ERROR; break; }
        if (ferror(file)) status = IO_ERROR;
    }
    if (status != OK) { free(talks); return status; }
    *out = talks;
    *count = (size_t)n;
    *halls = (size_t)k;
    return OK;
}

int main(int argc, char **argv)
{
    if (argc != 1) { fprintf(stderr, "Usage: %s < input.txt\n", argv[0]); return 1; }
    Talk *talks = NULL;
    size_t count = 0, halls = 0, result = 0;
    Status status = read_talks(stdin, &talks, &count, &halls);
    if (status == OK) status = max_talks(talks, count, halls, &result);
    free(talks);
    if (status != OK) { fprintf(stderr, "Input/schedule error: %d\n", status); return 1; }
    return printf("%zu\n", result) < 0 || fflush(stdout) == EOF;
}
