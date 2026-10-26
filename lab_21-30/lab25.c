#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

typedef enum { OK, INPUT, MEMORY, IO, END } Status;
typedef enum { BY_ID, BY_SURNAME, BY_NAME, BY_GROUP } Field;
typedef struct {
    int id;
    char *name, *surname, *group;
    unsigned char *grades;
} Student;

Status read_line(FILE *stream, char **out)
{
    if (stream == NULL || out == NULL) return INPUT;
    *out = NULL;
    size_t length = 0, capacity = 64;
    char *line = malloc(capacity);
    if (line == NULL) return MEMORY;
    int ch;
    while ((ch = fgetc(stream)) != EOF && ch != '\n') {
        if (length + 1 == capacity) {
            if (capacity > SIZE_MAX / 2) { free(line); return MEMORY; }
            capacity *= 2;
            char *grown = realloc(line, capacity);
            if (grown == NULL) { free(line); return MEMORY; }
            line = grown;
        }
        if (ch == 0) { free(line); return INPUT; }
        line[length++] = (char)ch;
    }
    if (ferror(stream)) { free(line); return IO; }
    if (ch == EOF && length == 0) { free(line); return END; }
    line[length] = '\0';
    *out = line;
    return OK;
}

Status split(char *line, char **words, size_t limit, size_t *count)
{
    if (line == NULL || words == NULL || count == NULL || limit == 0) return INPUT;
    *count = 0;
    char *p = line;
    while (*p != '\0') {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;
        if (*count == limit) return INPUT;
        words[(*count)++] = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        if (*p != '\0') *p++ = '\0';
    }
    return OK;
}

Status integer(const char *text, int minimum, int maximum, int *out)
{
    if (text == NULL || *text == '\0' || out == NULL || minimum < 0 || maximum < minimum) return INPUT;
    int value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9' || value > (INT_MAX - (*p - '0')) / 10) return INPUT;
        value = value * 10 + (*p - '0');
    }
    if (value < minimum || value > maximum) return INPUT;
    *out = value;
    return OK;
}

Status latin(const char *text)
{
    if (text == NULL || *text == '\0') return INPUT;
    for (const char *p = text; *p != '\0'; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return INPUT;
    }
    return OK;
}

Status copy_text(const char *text, char **out)
{
    if (text == NULL || out == NULL) return INPUT;
    size_t length = strlen(text);
    *out = malloc(length + 1);
    if (*out == NULL) return MEMORY;
    memcpy(*out, text, length + 1);
    return OK;
}

Status student_clear(Student *student)
{
    if (student == NULL) return INPUT;
    free(student->name); free(student->surname); free(student->group); free(student->grades);
    *student = (Student){0};
    return OK;
}

Status student_create(char *const *words, Student *out)
{
    if (words == NULL || out == NULL) return INPUT;
    for (size_t i = 0; i < 9; i++) if (words[i] == NULL) return INPUT;
    Student value = {0};
    Status status = integer(words[0], 0, INT_MAX, &value.id);
    if (status == OK) status = latin(words[1]);
    if (status == OK) status = latin(words[2]);
    if (status == OK && (words[3] == NULL || *words[3] == '\0')) status = INPUT;
    if (status == OK) {
        value.grades = malloc(5 * sizeof(unsigned char));
        if (value.grades == NULL) status = MEMORY;
    }
    for (size_t i = 0; i < 5 && status == OK; i++) {
        int grade;
        status = integer(words[4 + i], 2, 5, &grade);
        if (status == OK) value.grades[i] = (unsigned char)grade;
    }
    if (status == OK) status = copy_text(words[1], &value.name);
    if (status == OK) status = copy_text(words[2], &value.surname);
    if (status == OK) status = copy_text(words[3], &value.group);
    if (status != OK) { student_clear(&value); return status; }
    *out = value;
    return OK;
}

Status load_students(const char *path, Student **out, size_t *count)
{
    if (path == NULL || out == NULL || count == NULL) return INPUT;
    *out = NULL; *count = 0;
    FILE *file = fopen(path, "r");
    if (file == NULL) return IO;
    Status status = OK;
    char *line = NULL;
    while (status == OK) {
        status = read_line(file, &line);
        if (status == END) { status = OK; break; }
        if (status != OK) break;
        char *words[10];
        size_t length;
        status = split(line, words, 10, &length);
        if (status == OK && length != 0) {
            Student student = {0};
            if (length != 9) status = INPUT;
            else status = student_create(words, &student);
            if (status == OK) {
                if (*count == SIZE_MAX / sizeof(Student)) status = MEMORY;
                else {
                    Student *grown = realloc(*out, (*count + 1) * sizeof(Student));
                    if (grown == NULL) status = MEMORY;
                    else { *out = grown; (*out)[(*count)++] = student; student = (Student){0}; }
                }
            }
            student_clear(&student);
        }
        free(line); line = NULL;
    }
    free(line);
    if (fclose(file) != 0 && status == OK) status = IO;
    if (status != OK) {
        for (size_t i = 0; i < *count; i++) student_clear(&(*out)[i]);
        free(*out); *out = NULL; *count = 0;
    }
    return status;
}

Status parse_field(const char *text, Field *out)
{
    if (text == NULL || out == NULL) return INPUT;
    if (strcmp(text, "id") == 0) *out = BY_ID;
    else if (strcmp(text, "surname") == 0) *out = BY_SURNAME;
    else if (strcmp(text, "name") == 0) *out = BY_NAME;
    else if (strcmp(text, "group") == 0) *out = BY_GROUP;
    else return INPUT;
    return OK;
}

int compare_id(const void *left, const void *right)
{
    const Student *a = left, *b = right;
    return (a->id > b->id) - (a->id < b->id);
}

int compare_surname(const void *left, const void *right)
{
    const Student *a = left, *b = right;
    return strcmp(a->surname, b->surname);
}

int compare_name(const void *left, const void *right)
{
    const Student *a = left, *b = right;
    return strcmp(a->name, b->name);
}

int compare_group(const void *left, const void *right)
{
    const Student *a = left, *b = right;
    return strcmp(a->group, b->group);
}

Status sort_students(Student *students, size_t count, int (*comparator)(const void *, const void *))
{
    if ((students == NULL && count != 0) || comparator == NULL) return INPUT;
    if (count > 1) qsort(students, count, sizeof(Student), comparator);
    return OK;
}

Status student_average(const Student *student, double *out)
{
    if (student == NULL || student->grades == NULL || out == NULL) return INPUT;
    int sum = 0;
    for (size_t i = 0; i < 5; i++) {
        if (student->grades[i] < 2 || student->grades[i] > 5) return INPUT;
        sum += student->grades[i];
    }
    *out = sum / 5.0;
    return OK;
}

Status find_students(const Student *students, size_t count, Field field, const char *value, const Student ***out, size_t *found)
{
    if ((students == NULL && count != 0) || value == NULL || out == NULL || found == NULL || field < BY_ID || field > BY_GROUP) return INPUT;
    *out = NULL; *found = 0;
    int id = 0;
    if (field == BY_ID && integer(value, 0, INT_MAX, &id) != OK) return INPUT;
    if ((field == BY_NAME || field == BY_SURNAME) && latin(value) != OK) return INPUT;
    if (*value == '\0') return INPUT;
    if (count == 0) return OK;
    const Student **selected = malloc(count * sizeof(Student *));
    if (selected == NULL) return MEMORY;
    for (size_t i = 0; i < count; i++) {
        const Student *s = &students[i];
        int matches = field == BY_ID ? s->id == id : field == BY_NAME ? strcmp(s->name, value) == 0 : field == BY_SURNAME ? strcmp(s->surname, value) == 0 : strcmp(s->group, value) == 0;
        if (matches) selected[(*found)++] = s;
    }
    *out = selected;
    return OK;
}

Status above_average(const Student *students, size_t count, double eps, const Student ***out, size_t *found)
{
    if ((students == NULL && count != 0) || out == NULL || found == NULL || !isfinite(eps) || signbit(eps)) return INPUT;
    *out = NULL; *found = 0;
    if (count == 0) return OK;
    double total = 0.0;
    Status status = OK;
    for (size_t i = 0; i < count && status == OK; i++) {
        double average;
        status = student_average(&students[i], &average);
        if (status == OK) total += average / (double)count;
    }
    if (status != OK) return status;
    const Student **selected = malloc(count * sizeof(Student *));
    if (selected == NULL) return MEMORY;
    for (size_t i = 0; i < count && status == OK; i++) {
        double average;
        status = student_average(&students[i], &average);
        if (status == OK && average - total > eps) selected[(*found)++] = &students[i];
    }
    if (status != OK) { free(selected); *found = 0; return status; }
    *out = selected;
    return OK;
}

Status print_student(FILE *stream, const Student *student, int short_form)
{
    if (stream == NULL || student == NULL || student->name == NULL || student->surname == NULL || student->group == NULL) return INPUT;
    double average;
    Status status = student_average(student, &average);
    if (status != OK) return status;
    int written = short_form ? fprintf(stream, "%s %s\n", student->surname, student->name) : fprintf(stream, "%d %s %s %s %.2f\n", student->id, student->surname, student->name, student->group, average);
    return written < 0 ? IO : OK;
}

Status write_trace(const char *path, const Student *const *selected, size_t count, int short_form)
{
    if (path == NULL || (selected == NULL && count != 0)) return INPUT;
    FILE *file = fopen(path, "a");
    if (file == NULL) return IO;
    Status status = OK;
    for (size_t i = 0; i < count && status == OK; i++) status = print_student(file, selected[i], short_form);
    if (fclose(file) != 0) status = IO;
    return status;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "Usage: lab25 students.txt trace.txt < commands.txt\n"); return 1; }
    struct stat input_info, trace_info;
    if (stat(argv[1], &input_info) == 0 && stat(argv[2], &trace_info) == 0 && input_info.st_dev == trace_info.st_dev && input_info.st_ino == trace_info.st_ino) {
        fprintf(stderr, "Input and trace must be different files\n"); return 1;
    }
    Student *students = NULL;
    size_t count = 0;
    Status status = load_students(argv[1], &students, &count);
    if (status == OK && puts("Commands: list; find FIELD VALUE; sort FIELD; trace ID; above; quit. FIELD: id surname name group") == EOF) status = IO;
    char *line = NULL;
    while (status == OK) {
        status = read_line(stdin, &line);
        if (status == END) { status = OK; break; }
        if (status != OK) break;
        char *words[4]; size_t length = 0;
        Status command = split(line, words, 4, &length);
        if (command == OK && length == 0) { free(line); line = NULL; continue; }
        if (command == OK && length == 1 && strcmp(words[0], "quit") == 0) { free(line); line = NULL; break; }
        if (command == OK && length == 1 && strcmp(words[0], "list") == 0) {
            for (size_t i = 0; i < count && command == OK; i++) command = print_student(stdout, &students[i], 0);
        } else if (command == OK && length == 2 && strcmp(words[0], "sort") == 0) {
            Field field;
            command = parse_field(words[1], &field);
            if (command == OK) command = sort_students(students, count, field == BY_ID ? compare_id : field == BY_SURNAME ? compare_surname : field == BY_NAME ? compare_name : compare_group);
        } else if (command == OK && ((length == 3 && strcmp(words[0], "find") == 0) || (length == 2 && strcmp(words[0], "trace") == 0) || (length == 1 && strcmp(words[0], "above") == 0))) {
            const Student **selected = NULL;
            size_t found = 0;
            if (strcmp(words[0], "above") == 0) command = above_average(students, count, 1e-9, &selected, &found);
            else {
                Field field = BY_ID;
                if (strcmp(words[0], "find") == 0) command = parse_field(words[1], &field);
                if (command == OK) command = find_students(students, count, field, words[length - 1], &selected, &found);
            }
            if (command == OK && strcmp(words[0], "find") == 0) {
                for (size_t i = 0; i < found && command == OK; i++) command = print_student(stdout, selected[i], 0);
                if (found == 0 && puts("NOT_FOUND") == EOF) command = IO;
            } else if (command == OK) command = write_trace(argv[2], selected, found, strcmp(words[0], "above") == 0);
            free(selected);
        } else command = INPUT;
        free(line); line = NULL;
        if (command != OK) fprintf(stderr, "Command error: %d (1=input, 2=memory, 3=IO)\n", command);
        if (command == MEMORY || command == IO) status = command;
    }
    free(line);
    for (size_t i = 0; i < count; i++) student_clear(&students[i]);
    free(students);
    if (fflush(stdout) == EOF) status = IO;
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status == OK ? 0 : 1;
}
