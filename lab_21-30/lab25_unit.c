#define main lab25_program_main
#include "lab25.c"
#undef main
#include <assert.h>

int main(void)
{
    int number;
    assert(integer("2147483648", 0, INT_MAX, &number) == INPUT);
    assert(integer("-1", 0, INT_MAX, &number) == INPUT);
    assert(integer("0", 0, INT_MAX, &number) == OK && number == 0);
    char *fields[] = {"0", "Ivan", "Petrov", "G1", "5", "5", "5", "5", "5"};
    Student student = {0};
    assert(student_create(fields, &student) == OK);
    double average;
    assert(student_average(&student, &average) == OK && fabs(average - 5.0) < 1e-12);
    const Student **selected = NULL;
    size_t found;
    assert(find_students(&student, 1, BY_ID, "0", &selected, &found) == OK && found == 1);
    free(selected);
    assert(above_average(&student, 1, 1e-9, &selected, &found) == OK && found == 0);
    free(selected);
    assert(student_clear(&student) == OK);
    fields[4] = "6";
    assert(student_create(fields, &student) == INPUT && student.grades == NULL);
    assert(above_average(NULL, 0, 1e-9, &selected, &found) == OK && found == 0 && selected == NULL);
    assert(find_students(NULL, 0, BY_ID, "bad", &selected, &found) == INPUT);
    return 0;
}
