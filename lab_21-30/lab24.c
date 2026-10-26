#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

typedef enum { OK, INPUT, MEMORY, IO, NOT_FOUND, DUPLICATE, END } Status;
typedef struct { char *data; int length; } String;
typedef struct {
    String city, street, building, index;
    int house, apartment;
} Address;
typedef struct {
    Address recipient;
    double weight;
    String id, created, delivery;
    time_t created_time, delivery_time;
    int delivered;
} Mail;
typedef struct { Address *address; Mail *items; size_t count; } Post;

Status string_create(const char *text, String *out)
{
    if (text == NULL || out == NULL) return INPUT;
    size_t length = strlen(text);
    if (length > INT_MAX) return INPUT;
    char *copy = malloc(length + 1);
    if (copy == NULL) return MEMORY;
    memcpy(copy, text, length + 1);
    out->data = copy;
    out->length = (int)length;
    return OK;
}

Status string_clear(String *value)
{
    if (value == NULL) return INPUT;
    free(value->data);
    value->data = NULL;
    value->length = 0;
    return OK;
}

Status string_compare(const String *a, const String *b, int *result)
{
    if (a == NULL || b == NULL || result == NULL || a->data == NULL || b->data == NULL || a->length < 0 || b->length < 0) return INPUT;
    if (strlen(a->data) != (size_t)a->length || strlen(b->data) != (size_t)b->length) return INPUT;
    *result = a->length == b->length ? strcmp(a->data, b->data) : (a->length > b->length ? 1 : -1);
    return OK;
}

Status string_equal(const String *a, const String *b, int *result)
{
    if (a == NULL || b == NULL || result == NULL || a->data == NULL || b->data == NULL || a->length < 0 || b->length < 0) return INPUT;
    if (strlen(a->data) != (size_t)a->length || strlen(b->data) != (size_t)b->length) return INPUT;
    *result = strcmp(a->data, b->data) == 0;
    return OK;
}

Status string_copy(const String *source, String *destination)
{
    if (source == NULL || destination == NULL || source->data == NULL || source->length < 0) return INPUT;
    if (strlen(source->data) != (size_t)source->length) return INPUT;
    String copy = {0};
    Status status = string_create(source->data, &copy);
    if (status != OK) return status;
    string_clear(destination);
    *destination = copy;
    return OK;
}

Status string_clone(const String *source, String **out)
{
    if (source == NULL || source->data == NULL || out == NULL || source->length < 0) return INPUT;
    if (strlen(source->data) != (size_t)source->length) return INPUT;
    *out = calloc(1, sizeof(String));
    if (*out == NULL) return MEMORY;
    Status status = string_create(source->data, *out);
    if (status != OK) { free(*out); *out = NULL; }
    return status;
}

Status string_concat(String *a, const String *b)
{
    if (a == NULL || b == NULL || a->data == NULL || b->data == NULL || a->length < 0 || b->length < 0) return INPUT;
    if (strlen(a->data) != (size_t)a->length || strlen(b->data) != (size_t)b->length) return INPUT;
    if (a->length > INT_MAX - b->length) return INPUT;
    size_t length = (size_t)a->length + (size_t)b->length;
    char *joined = malloc(length + 1);
    if (joined == NULL) return MEMORY;
    memcpy(joined, a->data, (size_t)a->length);
    memcpy(joined + a->length, b->data, (size_t)b->length + 1);
    free(a->data);
    a->data = joined;
    a->length = (int)length;
    return OK;
}

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

Status positive_int(const char *text, int *out)
{
    if (text == NULL || *text == '\0' || out == NULL) return INPUT;
    int value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9' || value > (INT_MAX - (*p - '0')) / 10) return INPUT;
        value = value * 10 + (*p - '0');
    }
    if (value == 0) return INPUT;
    *out = value;
    return OK;
}

Status digits(const char *text, size_t length)
{
    if (text == NULL || strlen(text) != length) return INPUT;
    for (size_t i = 0; i < length; i++) if (text[i] < '0' || text[i] > '9') return INPUT;
    return OK;
}

Status timestamp(const char *date, const char *clock, time_t *out, char text[20])
{
    if (date == NULL || clock == NULL || out == NULL || text == NULL) return INPUT;
    if (strlen(date) != 10 || strlen(clock) != 8 || date[2] != ':' || date[5] != ':' || clock[2] != ':' || clock[5] != ':') return INPUT;
    for (size_t i = 0; i < 10; i++) if (i != 2 && i != 5 && (date[i] < '0' || date[i] > '9')) return INPUT;
    for (size_t i = 0; i < 8; i++) if (i != 2 && i != 5 && (clock[i] < '0' || clock[i] > '9')) return INPUT;
    int day, month, year, hour, minute, second;
    if (sscanf(date, "%2d:%2d:%4d", &day, &month, &year) != 3 || sscanf(clock, "%2d:%2d:%2d", &hour, &minute, &second) != 3) return INPUT;
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) days[1] = 29;
    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > days[month - 1] || hour > 23 || minute > 59 || second > 59) return INPUT;
    struct tm value = {0};
    value.tm_mday = day; value.tm_mon = month - 1; value.tm_year = year - 1900;
    value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second; value.tm_isdst = -1;
    time_t converted = mktime(&value);
    if (converted == (time_t)-1 || value.tm_mday != day || value.tm_mon != month - 1 || value.tm_year != year - 1900 || value.tm_hour != hour || value.tm_min != minute || value.tm_sec != second) return INPUT;
    memcpy(text, date, 10); text[10] = ' '; memcpy(text + 11, clock, 9);
    *out = converted;
    return OK;
}

Status address_clear(Address *address)
{
    if (address == NULL) return INPUT;
    string_clear(&address->city); string_clear(&address->street);
    string_clear(&address->building); string_clear(&address->index);
    return OK;
}

Status address_create(char *const *words, Address *out)
{
    if (words == NULL || out == NULL) return INPUT;
    for (size_t i = 0; i < 6; i++) if (words[i] == NULL) return INPUT;
    if (*words[0] == '\0' || *words[1] == '\0') return INPUT;
    Address value = {0};
    Status status = positive_int(words[2], &value.house);
    if (status == OK) status = positive_int(words[4], &value.apartment);
    if (status == OK) status = digits(words[5], 6);
    if (status == OK) status = string_create(words[0], &value.city);
    if (status == OK) status = string_create(words[1], &value.street);
    if (status == OK) status = string_create(strcmp(words[3], "-") == 0 ? "" : words[3], &value.building);
    if (status == OK) status = string_create(words[5], &value.index);
    if (status != OK) { address_clear(&value); return status; }
    *out = value;
    return OK;
}

Status mail_clear(Mail *mail)
{
    if (mail == NULL) return INPUT;
    address_clear(&mail->recipient);
    string_clear(&mail->id); string_clear(&mail->created); string_clear(&mail->delivery);
    return OK;
}

Status mail_create(char *const *words, Mail *out)
{
    if (words == NULL || out == NULL) return INPUT;
    for (size_t i = 0; i < 12; i++) if (words[i] == NULL) return INPUT;
    Mail value = {0};
    Status status = address_create(words, &value.recipient);
    char *end = NULL;
    if (status == OK) {
        if (*words[6] == '-' || *words[6] == '\0') status = INPUT;
        else {
            value.weight = strtod(words[6], &end);
            if (*end != '\0' || !isfinite(value.weight)) status = INPUT;
        }
    }
    if (status == OK) status = digits(words[7], 14);
    char created[20], delivery[20];
    if (status == OK) status = timestamp(words[8], words[9], &value.created_time, created);
    if (status == OK) status = timestamp(words[10], words[11], &value.delivery_time, delivery);
    if (status == OK && value.delivery_time < value.created_time) status = INPUT;
    if (status == OK) status = string_create(words[7], &value.id);
    if (status == OK) status = string_create(created, &value.created);
    if (status == OK) status = string_create(delivery, &value.delivery);
    if (status != OK) { mail_clear(&value); return status; }
    *out = value;
    return OK;
}

Status post_find(const Post *post, const char *id, size_t *index)
{
    if (post == NULL || id == NULL || index == NULL || digits(id, 14) != OK) return INPUT;
    for (size_t i = 0; i < post->count; i++) {
        if (strcmp(post->items[i].id.data, id) == 0) { *index = i; return OK; }
    }
    return NOT_FOUND;
}

int mail_order(const void *left, const void *right)
{
    const Mail *a = left, *b = right;
    int result = strcmp(a->recipient.index.data, b->recipient.index.data);
    return result != 0 ? result : strcmp(a->id.data, b->id.data);
}

Status post_add(Post *post, Mail *mail)
{
    if (post == NULL || mail == NULL || mail->id.data == NULL) return INPUT;
    size_t index;
    if (post_find(post, mail->id.data, &index) == OK) return DUPLICATE;
    if (post->count == SIZE_MAX / sizeof(Mail)) return MEMORY;
    Mail *grown = realloc(post->items, (post->count + 1) * sizeof(Mail));
    if (grown == NULL) return MEMORY;
    post->items = grown;
    post->items[post->count++] = *mail;
    *mail = (Mail){0};
    qsort(post->items, post->count, sizeof(Mail), mail_order);
    return OK;
}

Status post_remove(Post *post, const char *id)
{
    if (post == NULL || id == NULL) return INPUT;
    size_t index;
    Status status = post_find(post, id, &index);
    if (status != OK) return status;
    mail_clear(&post->items[index]);
    memmove(post->items + index, post->items + index + 1, (post->count - index - 1) * sizeof(Mail));
    post->count--;
    return OK;
}

int creation_order(const void *left, const void *right)
{
    const Mail *a = *(const Mail *const *)left, *b = *(const Mail *const *)right;
    if (a->created_time != b->created_time) return a->created_time > b->created_time ? 1 : -1;
    return strcmp(a->id.data, b->id.data);
}

Status post_select(const Post *post, int delivered, time_t now, const Mail ***out, size_t *count)
{
    if (post == NULL || out == NULL || count == NULL || (delivered != 0 && delivered != 1)) return INPUT;
    *out = NULL; *count = 0;
    if (post->count == 0) return OK;
    const Mail **selected = malloc(post->count * sizeof(Mail *));
    if (selected == NULL) return MEMORY;
    for (size_t i = 0; i < post->count; i++) {
        const Mail *mail = &post->items[i];
        if ((delivered && mail->delivered) || (!delivered && !mail->delivered && mail->delivery_time < now)) selected[(*count)++] = mail;
    }
    qsort(selected, *count, sizeof(Mail *), creation_order);
    *out = selected;
    return OK;
}

Status print_mail(const Mail *mail)
{
    if (mail == NULL || mail->id.data == NULL) return INPUT;
    const Address *a = &mail->recipient;
    if (printf("%s %s %s %d %s %d %s %.6g %s %s %s\n", mail->id.data, a->city.data, a->street.data, a->house, *a->building.data ? a->building.data : "-", a->apartment, a->index.data, mail->weight, mail->created.data, mail->delivery.data, mail->delivered ? "DELIVERED" : "PENDING") < 0) return IO;
    return OK;
}

Status string_demo(void)
{
    String first = {0}, second = {0}, copy = {0};
    String *clone = NULL;
    Status status = string_create("Hello", &first);
    if (status == OK) status = string_create("Post", &second);
    int order = 0, equal = 0;
    if (status == OK) status = string_compare(&first, &second, &order);
    if (status == OK) status = string_copy(&first, &copy);
    if (status == OK) status = string_equal(&first, &copy, &equal);
    if (status == OK) status = string_clone(&second, &clone);
    if (status == OK) status = string_concat(&first, clone);
    if (status == OK && printf("STRING %s copy=%s clone=%s order=%d equal=%d\n", first.data, copy.data, clone->data, order, equal) < 0) status = IO;
    string_clear(&first); string_clear(&second); string_clear(&copy);
    if (clone != NULL) { string_clear(clone); free(clone); }
    return status;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) { fprintf(stderr, "Usage: lab24 < commands.txt\n"); return 1; }
    Status status = string_demo();
    Address address = {0};
    Post post = {&address, NULL, 0};
    char *line = NULL, *words[14];
    size_t count = 0;
    if (status == OK && puts("First: post city street house building apartment index") == EOF) status = IO;
    if (status == OK) status = read_line(stdin, &line);
    if (status == OK) status = split(line, words, 14, &count);
    if (status == OK && (count != 7 || strcmp(words[0], "post") != 0)) status = INPUT;
    if (status == OK) status = address_create(words + 1, &address);
    free(line); line = NULL;
    if (status == OK && puts("Commands: add, delete ID, find ID, mark ID, list, delivered, overdue, quit") == EOF) status = IO;
    while (status == OK) {
        status = read_line(stdin, &line);
        if (status == END) { status = OK; break; }
        if (status != OK) break;
        Status command = split(line, words, 14, &count);
        if (command == OK && count == 0) { free(line); line = NULL; continue; }
        if (command == OK && count == 1 && strcmp(words[0], "quit") == 0) { free(line); line = NULL; break; }
        if (command == OK && count == 13 && strcmp(words[0], "add") == 0) {
            Mail mail = {0};
            command = mail_create(words + 1, &mail);
            if (command == OK) command = post_add(&post, &mail);
            mail_clear(&mail);
        } else if (command == OK && count == 2 && strcmp(words[0], "delete") == 0) {
            command = post_remove(&post, words[1]);
        } else if (command == OK && count == 2 && (strcmp(words[0], "find") == 0 || strcmp(words[0], "mark") == 0)) {
            size_t index;
            command = post_find(&post, words[1], &index);
            if (command == OK && strcmp(words[0], "mark") == 0) post.items[index].delivered = 1;
            else if (command == OK) command = print_mail(&post.items[index]);
        } else if (command == OK && count == 1 && strcmp(words[0], "list") == 0) {
            for (size_t i = 0; i < post.count && command == OK; i++) command = print_mail(&post.items[i]);
        } else if (command == OK && count == 1 && (strcmp(words[0], "delivered") == 0 || strcmp(words[0], "overdue") == 0)) {
            const Mail **selected = NULL;
            size_t selected_count = 0;
            time_t now = time(NULL);
            command = now == (time_t)-1 ? IO : post_select(&post, strcmp(words[0], "delivered") == 0, now, &selected, &selected_count);
            for (size_t i = 0; i < selected_count && command == OK; i++) command = print_mail(selected[i]);
            free(selected);
        } else command = INPUT;
        free(line); line = NULL;
        if (command != OK) fprintf(stderr, "Command error: %d (1=input, 2=memory, 3=IO, 4=not found, 5=duplicate)\n", command);
        if (command == MEMORY || command == IO) status = command;
    }
    free(line);
    for (size_t i = 0; i < post.count; i++) mail_clear(&post.items[i]);
    free(post.items); address_clear(&address);
    if (fflush(stdout) == EOF) status = IO;
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status == OK ? 0 : 1;
}
