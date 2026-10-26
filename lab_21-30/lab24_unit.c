#define main lab24_program_main
#include "lab24.c"
#undef main
#include <assert.h>

int main(void)
{
    String a = {0}, b = {0};
    String *clone = NULL;
    int result;
    assert(string_create("ab", &a) == OK);
    assert(string_create("cd", &b) == OK);
    assert(string_compare(&a, &b, &result) == OK && result < 0);
    assert(string_copy(&a, &a) == OK);
    assert(string_equal(&a, &a, &result) == OK && result == 1);
    assert(string_clone(&a, &clone) == OK);
    assert(clone != &a && clone->data != a.data);
    assert(string_concat(&a, &a) == OK && strcmp(a.data, "abab") == 0);
    assert(string_compare(&a, &b, &result) == OK && result > 0);
    assert(string_copy(&b, &a) == OK && strcmp(a.data, "cd") == 0);
    assert(string_clear(&a) == OK && a.data == NULL && a.length == 0);
    assert(string_clear(&b) == OK);
    assert(string_clear(clone) == OK); free(clone);
    time_t converted;
    char text[20];
    assert(timestamp("29:02:2000", "23:59:59", &converted, text) == OK);
    assert(timestamp("29:02:1900", "12:00:00", &converted, text) == INPUT);
    assert(timestamp("31:04:2026", "12:00:00", &converted, text) == INPUT);
    assert(timestamp("01:01:2026", "24:00:00", &converted, text) == INPUT);
    assert(timestamp("01:01:2026", "00:00:60", &converted, text) == INPUT);
    int number;
    assert(positive_int("2147483648", &number) == INPUT);
    assert(positive_int("0", &number) == INPUT);
    assert(string_create(NULL, &a) == INPUT);
    return 0;
}
