#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, FILE_ERROR, DATA_ERROR } Status;
typedef struct Stop {
    double x, y;
    int64_t arrival, departure;
    char marker;
    struct Stop *next;
} Stop;
typedef struct Vehicle {
    char *number;
    Stop *stops;
    struct Vehicle *next;
} Vehicle;
typedef struct {
    size_t routes;
    double distance, shortest, longest;
    int64_t min_stop, max_stop, waiting;
} Stats;

static Status read_line(FILE *stream, char **out)
{
    if (!stream || !out) return INPUT_ERROR;
    *out = NULL;
    size_t size = 0, capacity = 64;
    char *line = malloc(capacity);
    if (!line) return MEMORY_ERROR;
    int c;
    while ((c = fgetc(stream)) != EOF && c != '\n') {
        if (size + 1 == capacity) {
            if (capacity > SIZE_MAX / 2) { free(line); return MEMORY_ERROR; }
            size_t new_capacity = capacity * 2;
            char *temp = realloc(line, new_capacity);
            if (!temp) { free(line); return MEMORY_ERROR; }
            line = temp;
            capacity = new_capacity;
        }
        if (c == 0) { free(line); return INPUT_ERROR; }
        line[size++] = (char)c;
    }
    if (ferror(stream)) { free(line); return FILE_ERROR; }
    if (c == EOF && size == 0) { free(line); return OK; }
    line[size] = '\0';
    *out = line;
    return OK;
}

static Status split(char *line, char **parts, size_t expected)
{
    if (!line || !parts || expected == 0) return INPUT_ERROR;
    size_t count = 0;
    char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        if (count == expected) return INPUT_ERROR;
        parts[count++] = p;
        while (*p && !isspace((unsigned char)*p)) ++p;
        if (*p) *p++ = '\0';
    }
    return count == expected ? OK : INPUT_ERROR;
}

static Status number(const char *text, double *out)
{
    if (!text || !*text || !out) return INPUT_ERROR;
    char *end;
    double value = strtod(text, &end);
    if (*end || !isfinite(value)) return INPUT_ERROR;
    *out = value;
    return OK;
}

static Status timestamp(const char *date, const char *clock, int64_t *out)
{
    if (!date || !clock || !out || strlen(date) != 10 || strlen(clock) != 8) return INPUT_ERROR;
    if (date[2] != '.' || date[5] != '.' || clock[2] != ':' || clock[5] != ':') return INPUT_ERROR;
    for (size_t i = 0; i < 10; ++i)
        if (i != 2 && i != 5 && !isdigit((unsigned char)date[i])) return INPUT_ERROR;
    for (size_t i = 0; i < 8; ++i)
        if (i != 2 && i != 5 && !isdigit((unsigned char)clock[i])) return INPUT_ERROR;
    int d, m, y, h, minute, second;
    if (sscanf(date, "%d.%d.%d", &d, &m, &y) != 3 || sscanf(clock, "%d:%d:%d", &h, &minute, &second) != 3) return INPUT_ERROR;
    if (y < 1 || m < 1 || m > 12 || d < 1 || h > 23 || minute > 59 || second > 59) return INPUT_ERROR;
    int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    days[1] += leap;
    if (d > days[m - 1]) return INPUT_ERROR;
    int64_t count = (int64_t)(y - 1) * 365 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    for (int i = 0; i < m - 1; ++i) count += days[i];
    *out = ((count + d - 1) * 24 + h) * 3600 + minute * 60 + second;
    return OK;
}

static Status clear(Vehicle **head)
{
    if (!head) return INPUT_ERROR;
    while (*head) {
        Vehicle *next = (*head)->next;
        Stop *stop = (*head)->stops;
        while (stop) { Stop *after = stop->next; free(stop); stop = after; }
        free((*head)->number);
        free(*head);
        *head = next;
    }
    return OK;
}

static Status insert(Vehicle **head, const char *id, Stop *stop)
{
    if (!head || !id || !*id || !stop) return INPUT_ERROR;
    Vehicle *vehicle = *head;
    while (vehicle && strcmp(vehicle->number, id)) vehicle = vehicle->next;
    if (!vehicle) {
        vehicle = malloc(sizeof(*vehicle));
        if (!vehicle) return MEMORY_ERROR;
        vehicle->number = malloc(strlen(id) + 1);
        if (!vehicle->number) { free(vehicle); return MEMORY_ERROR; }
        strcpy(vehicle->number, id);
        vehicle->stops = NULL;
        vehicle->next = *head;
        *head = vehicle;
    }
    Stop **position = &vehicle->stops;
    while (*position && (*position)->arrival < stop->arrival) position = &(*position)->next;
    if (*position && (*position)->arrival == stop->arrival) return DATA_ERROR;
    stop->next = *position;
    *position = stop;
    return OK;
}

static Status load(const char *path, Vehicle **head)
{
    if (!path || !head) return INPUT_ERROR;
    FILE *file = fopen(path, "r");
    if (!file) return FILE_ERROR;
    char *line = NULL, *fields[6];
    double x = 0, y = 0;
    Status status = read_line(file, &line);
    if (status == OK && !line) status = INPUT_ERROR;
    if (status == OK) status = split(line, fields, 2);
    if (status == OK) status = number(fields[0], &x);
    if (status == OK) status = number(fields[1], &y);
    free(line);
    line = NULL;
    while (status == OK) {
        status = read_line(file, &line);
        if (status != OK || !line) break;
        status = split(line, fields, 6);
        Stop *stop = NULL;
        if (status == OK) {
            stop = calloc(1, sizeof(*stop));
            if (!stop) status = MEMORY_ERROR;
        }
        if (status == OK) status = timestamp(fields[1], fields[2], &stop->arrival);
        if (status == OK) status = timestamp(fields[3], fields[4], &stop->departure);
        if (status == OK && (strlen(fields[5]) != 1 || !strchr("SIF", fields[5][0]) || stop->arrival > stop->departure)) status = INPUT_ERROR;
        if (status == OK) {
            stop->marker = fields[5][0];
            stop->x = x;
            stop->y = y;
            status = insert(head, fields[0], stop);
        }
        if (status != OK) free(stop);
        free(line);
        line = NULL;
    }
    free(line);
    if (fclose(file) != 0 && status == OK) status = FILE_ERROR;
    return status;
}

static Status statistics(const Vehicle *vehicle, double eps, Stats *out)
{
    if (!vehicle || !vehicle->stops || !out || !isfinite(eps) || eps <= 0) return INPUT_ERROR;
    Stats stats = {0};
    stats.min_stop = INT64_MAX;
    stats.shortest = INFINITY;
    int active = 0;
    double length = 0;
    const Stop *previous = NULL;
    for (const Stop *s = vehicle->stops; s; s = s->next) {
        if (previous && previous->departure > s->arrival) return DATA_ERROR;
        if (s->marker == 'S') {
            if (active) return DATA_ERROR;
            active = 1;
            length = 0;
        } else {
            if (!active || !previous) return DATA_ERROR;
            double segment = hypot(s->x - previous->x, s->y - previous->y);
            length += segment;
            if (!isfinite(length)) return DATA_ERROR;
        }
        int64_t duration = s->departure - s->arrival;
        if (duration < stats.min_stop) stats.min_stop = duration;
        if (duration > stats.max_stop) stats.max_stop = duration;
        if (stats.waiting > INT64_MAX - duration) return DATA_ERROR;
        stats.waiting += duration;
        if (s->marker == 'F') {
            active = 0;
            ++stats.routes;
            stats.distance += length;
            if (!isfinite(stats.distance)) return DATA_ERROR;
            if (length < stats.shortest - eps) stats.shortest = length;
            if (length > stats.longest + eps) stats.longest = length;
        }
        previous = s;
    }
    if (active || stats.routes == 0) return DATA_ERROR;
    *out = stats;
    return OK;
}

static Status search(const Vehicle *head, int query, double eps, const Vehicle **out, double *value)
{
    if (!head || !out || !value || query < 1 || query > 9 || !isfinite(eps) || eps <= 0) return INPUT_ERROR;
    *out = NULL;
    for (const Vehicle *v = head; v; v = v->next) {
        Stats stats;
        Status status = statistics(v, eps, &stats);
        if (status != OK) return status;
        double candidate = 0;
        switch (query) {
            case 1: case 2: candidate = (double)stats.routes; break;
            case 3: case 4: candidate = stats.distance; break;
            case 5: candidate = stats.longest; break;
            case 6: candidate = stats.shortest; break;
            case 7: candidate = (double)stats.max_stop; break;
            case 8: candidate = (double)stats.min_stop; break;
            case 9: candidate = (double)stats.waiting; break;
        }
        int maximum = query == 1 || query == 3 || query == 5 || query == 7 || query == 9;
        if (!*out || (maximum && candidate > *value + eps) || (!maximum && candidate < *value - eps)) {
            *out = v;
            *value = candidate;
        }
    }
    return OK;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Usage: %s stop_file...\n", argv[0]); return 1; }
    Vehicle *head = NULL;
    Status status = OK;
    const double eps = 1e-9;
    for (int i = 1; i < argc && status == OK; ++i) status = load(argv[i], &head);
    if (status == OK && !head) status = DATA_ERROR;
    for (const Vehicle *v = head; v && status == OK; v = v->next) {
        Stats stats;
        status = statistics(v, eps, &stats);
    }
    if (status == OK) puts("1 max routes; 2 min routes; 3 max total distance; 4 min total distance;\n5 longest route; 6 shortest route; 7 longest stop; 8 shortest stop; 9 max waiting; 0 quit");
    while (status == OK) {
        char *line = NULL;
        status = read_line(stdin, &line);
        if (status != OK || !line) { free(line); break; }
        char *p[1];
        Status command = split(line, p, 1);
        int query = -1;
        if (command == OK && strlen(p[0]) == 1 && p[0][0] >= '0' && p[0][0] <= '9') query = p[0][0] - '0';
        if (query == 0) { free(line); break; }
        if (query < 1) fprintf(stderr, "Invalid command\n");
        else {
            const Vehicle *best = NULL;
            double value = 0;
            command = search(head, query, eps, &best, &value);
            if (command == OK) printf("%s %.10g\n", best->number, value);
            else status = command;
        }
        free(line);
    }
    if (status != OK) fprintf(stderr, "Error: %d (1 input, 2 memory, 3 file, 4 inconsistent routes)\n", status);
    clear(&head);
    if (fflush(stdout) == EOF && status == OK) status = FILE_ERROR;
    return status == OK ? 0 : 1;
}
