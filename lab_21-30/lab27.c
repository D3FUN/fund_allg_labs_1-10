#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, FILE_ERROR, NOT_FOUND } Status;
typedef struct {
    char surname[64], name[64], patronymic[64];
    int day, month, year;
    char sex;
    double income;
} Liver;
typedef struct Node { Liver data; struct Node *next; } Node;
typedef struct History { Node *snapshot; struct History *next; } History;

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
            char *temp = realloc(line, capacity * 2);
            if (!temp) { free(line); return MEMORY_ERROR; }
            line = temp;
            capacity *= 2;
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

static Status split(char *line, char **parts, size_t capacity, size_t *count)
{
    if (!line || !parts || !count || capacity == 0) return INPUT_ERROR;
    *count = 0;
    char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        if (*count == capacity) return INPUT_ERROR;
        parts[(*count)++] = p;
        while (*p && !isspace((unsigned char)*p)) ++p;
        if (*p) *p++ = '\0';
    }
    return OK;
}

static Status integer(const char *text, size_t limit, size_t *out)
{
    if (!text || !*text || !out) return INPUT_ERROR;
    size_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return INPUT_ERROR;
        size_t digit = (size_t)(*p - '0');
        if (digit > limit || value > (limit - digit) / 10) return INPUT_ERROR;
        value = value * 10 + digit;
    }
    *out = value;
    return OK;
}

static Status real_number(const char *text, double *out)
{
    if (!text || !*text || !out) return INPUT_ERROR;
    char *end;
    double value = strtod(text, &end);
    if (*end || !isfinite(value)) return INPUT_ERROR;
    *out = value;
    return OK;
}

static Status latin_name(const char *text, int empty)
{
    if (!text || strlen(text) >= 64 || (!*text && !empty)) return INPUT_ERROR;
    for (const char *p = text; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return INPUT_ERROR;
    return OK;
}

static Status date_key(int d, int m, int y, int today, int *out)
{
    if (!out || y < 1 || y > 9999 || m < 1 || m > 12 || d < 1) return INPUT_ERROR;
    int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) days[1] = 29;
    int key = y * 10000 + m * 100 + d;
    if (d > days[m - 1] || key > today) return INPUT_ERROR;
    *out = key;
    return OK;
}

static Status parse_liver(char **p, size_t count, int today, double eps, Liver *out)
{
    if (!p || !out || count != 8 || !isfinite(eps) || eps <= 0) return INPUT_ERROR;
    Liver value = {0};
    const char *patronymic = strcmp(p[2], "-") ? p[2] : "";
    if (latin_name(p[0], 0) != OK || latin_name(p[1], 0) != OK || latin_name(patronymic, 1) != OK) return INPUT_ERROR;
    size_t d, m, y;
    if (integer(p[3], 31, &d) != OK || integer(p[4], 12, &m) != OK || integer(p[5], 9999, &y) != OK) return INPUT_ERROR;
    int key;
    if (date_key((int)d, (int)m, (int)y, today, &key) != OK) return INPUT_ERROR;
    if (strlen(p[6]) != 1 || !strchr("MW", p[6][0])) return INPUT_ERROR;
    if (real_number(p[7], &value.income) != OK || value.income < -eps || signbit(value.income)) return INPUT_ERROR;
    strcpy(value.surname, p[0]);
    strcpy(value.name, p[1]);
    strcpy(value.patronymic, patronymic);
    value.day = (int)d;
    value.month = (int)m;
    value.year = (int)y;
    value.sex = p[6][0];
    *out = value;
    return OK;
}

static Status clear(Node **head)
{
    if (!head) return INPUT_ERROR;
    while (*head) { Node *next = (*head)->next; free(*head); *head = next; }
    return OK;
}

static Status clone(const Node *head, Node **out)
{
    if (!out) return INPUT_ERROR;
    *out = NULL;
    Node **tail = out;
    for (const Node *p = head; p; p = p->next) {
        Node *copy = malloc(sizeof(*copy));
        if (!copy) { clear(out); return MEMORY_ERROR; }
        copy->data = p->data;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return OK;
}

static Status insert(Node **head, const Liver *liver)
{
    if (!head || !liver) return INPUT_ERROR;
    Node *node = malloc(sizeof(*node));
    if (!node) return MEMORY_ERROR;
    node->data = *liver;
    int key = liver->year * 10000 + liver->month * 100 + liver->day;
    Node **position = head;
    while (*position) {
        const Liver *other = &(*position)->data;
        int other_key = other->year * 10000 + other->month * 100 + other->day;
        if (key > other_key) break;
        position = &(*position)->next;
    }
    node->next = *position;
    *position = node;
    return OK;
}

static Status remove_at(Node **head, size_t index)
{
    if (!head || index == 0) return INPUT_ERROR;
    Node **position = head;
    for (size_t i = 1; *position && i < index; ++i) position = &(*position)->next;
    if (!*position) return NOT_FOUND;
    Node *node = *position;
    *position = node->next;
    free(node);
    return OK;
}

static Status clear_history(History **history)
{
    if (!history) return INPUT_ERROR;
    while (*history) {
        History *next = (*history)->next;
        clear(&(*history)->snapshot);
        free(*history);
        *history = next;
    }
    return OK;
}

static Status modify(Node **head, History **history, size_t *changes, const char *command, size_t index, const Liver *liver)
{
    if (!head || !history || !changes || !command || *changes == SIZE_MAX) return INPUT_ERROR;
    if (strcmp(command, "add") && strcmp(command, "del") && strcmp(command, "change")) return INPUT_ERROR;
    if (strcmp(command, "del") && !liver) return INPUT_ERROR;
    History *entry = calloc(1, sizeof(*entry));
    if (!entry) return MEMORY_ERROR;
    Status status = clone(*head, &entry->snapshot);
    Node *updated = NULL;
    if (status == OK) status = clone(*head, &updated);
    if (status == OK && strcmp(command, "add")) status = remove_at(&updated, index);
    if (status == OK && strcmp(command, "del")) status = insert(&updated, liver);
    if (status == OK) {
        clear(head);
        *head = updated;
        entry->next = *history;
        *history = entry;
        ++*changes;
    } else {
        clear(&updated);
        clear(&entry->snapshot);
        free(entry);
    }
    return status;
}

static Status undo(Node **head, History **history, size_t *changes, size_t *undone)
{
    if (!head || !history || !changes || !undone) return INPUT_ERROR;
    *undone = *changes / 2;
    for (size_t i = 0; i < *undone; ++i) {
        if (!*history) return INPUT_ERROR;
        History *entry = *history;
        clear(head);
        *head = entry->snapshot;
        *history = entry->next;
        free(entry);
    }
    clear_history(history);
    *changes = 0;
    return OK;
}

static Status load(const char *path, int today, double eps, Node **out)
{
    if (!path || !out) return INPUT_ERROR;
    *out = NULL;
    FILE *file = fopen(path, "r");
    if (!file) return FILE_ERROR;
    Status status = OK;
    while (status == OK) {
        char *line = NULL, *p[8];
        size_t count;
        status = read_line(file, &line);
        if (status != OK || !line) { free(line); break; }
        status = split(line, p, 8, &count);
        Liver value;
        if (status == OK) status = parse_liver(p, count, today, eps, &value);
        if (status == OK) status = insert(out, &value);
        free(line);
    }
    if (fclose(file) != 0 && status == OK) status = FILE_ERROR;
    if (status != OK) clear(out);
    return status;
}

static Status write_liver(FILE *file, const Liver *liver)
{
    if (!file || !liver) return INPUT_ERROR;
    if (fprintf(file, "%s %s %s %d %d %d %c %.17g\n", liver->surname, liver->name,
                *liver->patronymic ? liver->patronymic : "-", liver->day, liver->month,
                liver->year, liver->sex, liver->income) < 0) return FILE_ERROR;
    return OK;
}

static Status save(const char *path, const Node *head)
{
    if (!path || !*path) return INPUT_ERROR;
    FILE *file = fopen(path, "w");
    if (!file) return FILE_ERROR;
    Status status = OK;
    for (const Node *p = head; p && status == OK; p = p->next) status = write_liver(file, &p->data);
    if (fclose(file) != 0 && status == OK) status = FILE_ERROR;
    return status;
}

static Status match(const Liver *liver, const char *field, const char *value, double eps, int *out)
{
    if (!liver || !field || !value || !out || !isfinite(eps) || eps <= 0) return INPUT_ERROR;
    if ((!strcmp(field, "surname") || !strcmp(field, "name")) && latin_name(value, 0) != OK) return INPUT_ERROR;
    if (!strcmp(field, "patronymic") && strcmp(value, "-") && latin_name(value, 1) != OK) return INPUT_ERROR;
    if (!strcmp(field, "surname")) *out = !strcmp(liver->surname, value);
    else if (!strcmp(field, "name")) *out = !strcmp(liver->name, value);
    else if (!strcmp(field, "patronymic")) *out = !strcmp(liver->patronymic, strcmp(value, "-") ? value : "");
    else if (!strcmp(field, "sex")) {
        if (strlen(value) != 1 || !strchr("MW", *value)) return INPUT_ERROR;
        *out = liver->sex == *value;
    } else if (!strcmp(field, "income")) {
        double income;
        if (real_number(value, &income) != OK || income < -eps || signbit(income)) return INPUT_ERROR;
        *out = fabs(liver->income - income) <= eps;
    } else if (!strcmp(field, "birth")) {
        if (strlen(value) != 10) return INPUT_ERROR;
        for (size_t i = 0; i < 10; ++i)
            if ((i == 2 || i == 5) ? value[i] != '.' : !isdigit((unsigned char)value[i])) return INPUT_ERROR;
        int d, m, y, key;
        if (sscanf(value, "%d.%d.%d", &d, &m, &y) != 3 || date_key(d, m, y, 99991231, &key) != OK) return INPUT_ERROR;
        *out = liver->day == d && liver->month == m && liver->year == y;
    } else return INPUT_ERROR;
    return OK;
}

static Status select_people(const Node *head, const char *field, const char *value, double eps, const Node ***out, size_t *count)
{
    if (!out || !count) return INPUT_ERROR;
    *out = NULL;
    *count = 0;
    Liver probe = {0};
    int found;
    Status status = match(&probe, field, value, eps, &found);
    for (const Node *p = head; p && status == OK; p = p->next) {
        status = match(&p->data, field, value, eps, &found);
        if (status == OK && found) {
            if (*count >= SIZE_MAX / sizeof(**out)) status = MEMORY_ERROR;
            else {
                const Node **temp = realloc(*out, (*count + 1) * sizeof(**out));
                if (!temp) status = MEMORY_ERROR;
                else { *out = temp; (*out)[(*count)++] = p; }
            }
        }
    }
    if (status != OK) { free(*out); *out = NULL; *count = 0; }
    return status;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s residents_file\n", argv[0]); return 1; }
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (now == (time_t)-1 || !local) { fprintf(stderr, "Clock error\n"); return 1; }
    int today = (local->tm_year + 1900) * 10000 + (local->tm_mon + 1) * 100 + local->tm_mday;
    const double eps = 1e-9;
    Node *head = NULL;
    History *history = NULL;
    size_t changes = 0;
    Status status = load(argv[1], today, eps, &head);
    if (status == OK) puts("list | find FIELD VALUE | add RECORD | del INDEX | change INDEX RECORD | undo | save PATH | quit");
    while (status == OK) {
        char *line = NULL, *p[10];
        size_t count = 0;
        status = read_line(stdin, &line);
        if (status != OK || !line) { free(line); break; }
        Status command = split(line, p, 10, &count);
        if (command == OK && count == 1 && !strcmp(p[0], "quit")) { free(line); break; }
        if (command != OK || count == 0) command = INPUT_ERROR;
        else if (count == 1 && !strcmp(p[0], "list")) {
            size_t index = 1;
            for (const Node *node = head; node && command == OK; node = node->next, ++index) {
                if (printf("%zu: ", index) < 0) command = FILE_ERROR;
                if (command == OK) command = write_liver(stdout, &node->data);
            }
        } else if (count == 3 && !strcmp(p[0], "find")) {
            const Node **found = NULL;
            size_t total = 0;
            command = select_people(head, p[1], p[2], eps, &found, &total);
            for (size_t i = 0; i < total && command == OK; ++i) command = write_liver(stdout, &found[i]->data);
            if (command == OK) printf("FOUND %zu\n", total);
            free(found);
        } else if (count == 2 && !strcmp(p[0], "save")) {
            command = save(p[1], head);
            if (command == OK) puts("SAVED");
        } else if (count == 1 && !strcmp(p[0], "undo")) {
            size_t undone = 0;
            command = undo(&head, &history, &changes, &undone);
            if (command == OK) printf("UNDONE %zu\n", undone);
        } else if ((!strcmp(p[0], "add") && count == 9) || (!strcmp(p[0], "change") && count == 10) || (!strcmp(p[0], "del") && count == 2)) {
            size_t index = 0;
            Liver value;
            if (strcmp(p[0], "add")) command = integer(p[1], SIZE_MAX, &index);
            size_t offset = !strcmp(p[0], "add") ? 1 : 2;
            if (command == OK && strcmp(p[0], "del")) command = parse_liver(p + offset, count - offset, today, eps, &value);
            if (command == OK) command = modify(&head, &history, &changes, p[0], index, !strcmp(p[0], "del") ? NULL : &value);
            if (command == OK) puts("MODIFIED");
        } else command = INPUT_ERROR;
        if (command != OK) fprintf(stderr, "Command error: %d (1 input, 2 memory, 3 file, 4 not found)\n", command);
        if (command == MEMORY_ERROR || command == FILE_ERROR) status = command;
        free(line);
    }
    clear_history(&history);
    clear(&head);
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    if (fflush(stdout) == EOF && status == OK) status = FILE_ERROR;
    return status == OK ? 0 : 1;
}
