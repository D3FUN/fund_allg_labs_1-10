#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, FILE_ERROR, LIMIT_ERROR } Status;

typedef struct Node
{
    char *label;
    struct Node *child;
    struct Node *next;
} Node;

static Status read_line(FILE *file, char **text, int *ended)
{
    if (file == NULL || text == NULL || ended == NULL) return INPUT_ERROR;
    *text = NULL;
    *ended = 0;
    size_t length = 0, capacity = 64;
    char *buffer = malloc(capacity);
    if (buffer == NULL) return MEMORY_ERROR;
    int c;
    Status status = OK;
    while ((c = fgetc(file)) != EOF && c != '\n')
    {
        if (c == 0 || length == 1048576) { status = INPUT_ERROR; break; }
        if (length + 1 == capacity)
        {
            size_t new_capacity = capacity * 2;
            char *temporary = realloc(buffer, new_capacity);
            if (temporary == NULL) { status = MEMORY_ERROR; break; }
            buffer = temporary;
            capacity = new_capacity;
        }
        buffer[length++] = (char)c;
    }
    if (ferror(file)) status = FILE_ERROR;
    if (c == EOF && length == 0) *ended = 1;
    if (status != OK) { free(buffer); return status; }
    if (length > 0 && buffer[length - 1] == '\r') length--;
    buffer[length] = '\0';
    *text = buffer;
    return OK;
}

static Status clear_tree(Node *tree)
{
    while (tree != NULL)
    {
        Node *next = tree->next;
        clear_tree(tree->child);
        free(tree->label);
        free(tree);
        tree = next;
    }
    return OK;
}

static Status skip_spaces(const char **position)
{
    if (position == NULL || *position == NULL) return INPUT_ERROR;
    while (isspace((unsigned char)**position)) (*position)++;
    return OK;
}

static Status parse_tree(const char **position, unsigned depth, Node **result)
{
    if (position == NULL || *position == NULL || result == NULL) return INPUT_ERROR;
    *result = NULL;
    if (depth > 256) return LIMIT_ERROR;
    skip_spaces(position);
    const char *begin = *position;
    while (**position && !isspace((unsigned char)**position) &&
           **position != '(' && **position != ')' && **position != ',') (*position)++;
    size_t length = (size_t)(*position - begin);
    if (length == 0) return INPUT_ERROR;
    Node *node = calloc(1, sizeof(*node));
    if (node == NULL) return MEMORY_ERROR;
    node->label = malloc(length + 1);
    if (node->label == NULL) { free(node); return MEMORY_ERROR; }
    memcpy(node->label, begin, length);
    node->label[length] = '\0';
    skip_spaces(position);
    Status status = OK;
    if (**position == '(')
    {
        (*position)++;
        Node **tail = &node->child;
        do
        {
            status = parse_tree(position, depth + 1, tail);
            if (status != OK) break;
            tail = &(*tail)->next;
            skip_spaces(position);
            if (**position != ',') break;
            (*position)++;
        } while (status == OK);
        if (status == OK && **position != ')') status = INPUT_ERROR;
        if (status == OK) (*position)++;
    }
    if (status != OK) { clear_tree(node); return status; }
    *result = node;
    return OK;
}

static Status print_tree(FILE *file, const Node *tree, unsigned depth)
{
    if (file == NULL || (tree != NULL && depth > 256)) return INPUT_ERROR;
    for (const Node *node = tree; node != NULL; node = node->next)
    {
        for (unsigned i = 0; i < depth; i++)
            if (fputs("  ", file) == EOF) return FILE_ERROR;
        if (fprintf(file, "%s\n", node->label) < 0) return FILE_ERROR;
        Status status = print_tree(file, node->child, depth + 1);
        if (status != OK) return status;
    }
    return OK;
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], argv[2]) == 0)
    {
        fprintf(stderr, "Usage: %s input.txt output.txt (different files)\n", argv[0]);
        return 1;
    }
    struct stat input_info, output_info;
    if (stat(argv[1], &input_info) == 0 && stat(argv[2], &output_info) == 0 &&
        input_info.st_dev == output_info.st_dev && input_info.st_ino == output_info.st_ino)
    {
        fprintf(stderr, "Input and output refer to the same file\n");
        return 1;
    }
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) { fprintf(stderr, "Cannot open input\n"); return 1; }
    /* Build the output in a temporary stream, so an invalid line leaves the destination intact. */
    FILE *temporary = tmpfile();
    Status status = temporary == NULL ? FILE_ERROR : OK;
    size_t line_number = 0;
    int ended = 0;
    while (status == OK && !ended)
    {
        char *line = NULL;
        status = read_line(input, &line, &ended);
        if (status == OK && !ended)
        {
            line_number++;
            const char *position = line;
            Node *tree = NULL;
            status = parse_tree(&position, 0, &tree);
            skip_spaces(&position);
            if (status == OK && *position != '\0') status = INPUT_ERROR;
            if (status == OK && fprintf(temporary, "Tree %zu:\n", line_number) < 0) status = FILE_ERROR;
            if (status == OK) status = print_tree(temporary, tree, 0);
            if (status == OK && fputc('\n', temporary) == EOF) status = FILE_ERROR;
            clear_tree(tree);
        }
        free(line);
    }
    if (fclose(input) != 0) status = FILE_ERROR;
    if (status == OK && fflush(temporary) != 0) status = FILE_ERROR;
    if (status == OK && fseek(temporary, 0, SEEK_SET) != 0) status = FILE_ERROR;
    FILE *output = NULL;
    if (status == OK)
    {
        output = fopen(argv[2], "w");
        if (output == NULL) status = FILE_ERROR;
    }
    if (status == OK)
    {
        int c;
        while ((c = fgetc(temporary)) != EOF)
            if (fputc(c, output) == EOF) { status = FILE_ERROR; break; }
        if (ferror(temporary)) status = FILE_ERROR;
    }
    if (output != NULL && fclose(output) != 0) status = FILE_ERROR;
    if (temporary != NULL && fclose(temporary) != 0) status = FILE_ERROR;
    if (status != OK) fprintf(stderr, "Error %d near line %zu\n", status, line_number);
    return status == OK ? 0 : 1;
}
