#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

typedef enum { OK, INPUT_ERROR, MEMORY_ERROR, FILE_ERROR, LIMIT_ERROR } Status;

typedef struct Node
{
    char *word;
    size_t count;
    struct Node *left;
    struct Node *right;
} Node;

static Status read_line(FILE *file, char **result, int *ended)
{
    if (file == NULL || result == NULL || ended == NULL) return INPUT_ERROR;
    *result = NULL;
    *ended = 0;
    size_t capacity = 64, length = 0;
    char *text = malloc(capacity);
    if (text == NULL) return MEMORY_ERROR;
    int c;
    Status status = OK;
    while ((c = fgetc(file)) != EOF && c != '\n')
    {
        if (c == 0 || length == 2097184) { status = LIMIT_ERROR; break; }
        if (length + 1 == capacity)
        {
            char *temporary = realloc(text, capacity * 2);
            if (temporary == NULL) { status = MEMORY_ERROR; break; }
            text = temporary;
            capacity *= 2;
        }
        text[length++] = (char)c;
    }
    if (ferror(file)) status = FILE_ERROR;
    if (c == EOF && length == 0) *ended = 1;
    if (status != OK) { free(text); return status; }
    if (length && text[length - 1] == '\r') length--;
    text[length] = '\0';
    *result = text;
    return OK;
}

static Status parse_size(const char *text, size_t *result)
{
    if (text == NULL || *text == '\0' || result == NULL) return INPUT_ERROR;
    size_t value = 0;
    for (; *text; text++)
    {
        if (*text < '0' || *text > '9') return INPUT_ERROR;
        size_t digit = (size_t)(*text - '0');
        if (value > (SIZE_MAX - digit) / 10) return LIMIT_ERROR;
        value = value * 10 + digit;
    }
    *result = value;
    return OK;
}

static Status clear_tree(Node *tree)
{
    if (tree != NULL)
    {
        clear_tree(tree->left);
        clear_tree(tree->right);
        free(tree->word);
        free(tree);
    }
    return OK;
}

static Status insert(Node **tree, const char *word)
{
    if (tree == NULL || word == NULL || *word == '\0') return INPUT_ERROR;
    unsigned depth = 1;
    while (*tree != NULL)
    {
        int order = strcmp(word, (*tree)->word);
        if (order == 0)
        {
            if ((*tree)->count == SIZE_MAX) return LIMIT_ERROR;
            (*tree)->count++;
            return OK;
        }
        tree = order < 0 ? &(*tree)->left : &(*tree)->right;
        depth++;
    }
    if (depth > 512) return LIMIT_ERROR;
    Node *node = calloc(1, sizeof(*node));
    if (node == NULL) return MEMORY_ERROR;
    node->word = malloc(strlen(word) + 1);
    if (node->word == NULL) { free(node); return MEMORY_ERROR; }
    strcpy(node->word, word);
    node->count = 1;
    *tree = node;
    return OK;
}

static Status find_word(const Node *tree, const char *word, size_t *count)
{
    if (word == NULL || count == NULL) return INPUT_ERROR;
    *count = 0;
    while (tree != NULL)
    {
        int order = strcmp(word, tree->word);
        if (order == 0) { *count = tree->count; return OK; }
        tree = order < 0 ? tree->left : tree->right;
    }
    return OK;
}

static Status read_words(FILE *file, const unsigned char separators[256], Node **tree)
{
    if (file == NULL || separators == NULL || tree == NULL) return INPUT_ERROR;
    size_t capacity = 64, length = 0;
    char *word = malloc(capacity);
    if (word == NULL) return MEMORY_ERROR;
    Status status = OK;
    int c;
    while (status == OK && (c = fgetc(file)) != EOF)
    {
        if (c == 0) { status = INPUT_ERROR; break; }
        if (separators[(unsigned char)c])
        {
            if (length)
            {
                word[length] = '\0';
                status = insert(tree, word);
                length = 0;
            }
        }
        else
        {
            if (length == 1048576) { status = LIMIT_ERROR; break; }
            if (length + 1 == capacity)
            {
                char *temporary = realloc(word, capacity * 2);
                if (temporary == NULL) { status = MEMORY_ERROR; break; }
                word = temporary;
                capacity *= 2;
            }
            word[length++] = (char)c;
        }
    }
    if (status == OK && length)
    {
        word[length] = '\0';
        status = insert(tree, word);
    }
    if (ferror(file)) status = FILE_ERROR;
    free(word);
    return status;
}

static Status print_tree(FILE *file, const Node *tree)
{
    if (file == NULL) return INPUT_ERROR;
    if (tree == NULL) return OK;
    Status status = print_tree(file, tree->left);
    if (status == OK && fprintf(file, "%s %zu\n", tree->word, tree->count) < 0) status = FILE_ERROR;
    if (status == OK) status = print_tree(file, tree->right);
    return status;
}

static Status statistics(const Node *tree, size_t *size, unsigned *depth,
                         const Node **shortest, const Node **longest)
{
    if (size == NULL || depth == NULL || shortest == NULL || longest == NULL) return INPUT_ERROR;
    if (tree == NULL) { *size = 0; *depth = 0; *shortest = NULL; *longest = NULL; return OK; }
    size_t left_size, right_size;
    unsigned left_depth, right_depth;
    const Node *left_short, *left_long, *right_short, *right_long;
    Status status = statistics(tree->left, &left_size, &left_depth, &left_short, &left_long);
    if (status == OK) status = statistics(tree->right, &right_size, &right_depth, &right_short, &right_long);
    if (status != OK) return status;
    if (left_size > SIZE_MAX - right_size - 1) return LIMIT_ERROR;
    *size = left_size + right_size + 1;
    *depth = (left_depth > right_depth ? left_depth : right_depth) + 1;
    *shortest = *longest = tree;
    if (left_short != NULL && strlen(left_short->word) < strlen((*shortest)->word)) *shortest = left_short;
    if (right_short != NULL && strlen(right_short->word) < strlen((*shortest)->word)) *shortest = right_short;
    if (left_long != NULL && strlen(left_long->word) > strlen((*longest)->word)) *longest = left_long;
    if (right_long != NULL && strlen(right_long->word) > strlen((*longest)->word)) *longest = right_long;
    return OK;
}

static Status collect(const Node *tree, const Node **items, size_t *position)
{
    if (items == NULL || position == NULL) return INPUT_ERROR;
    if (tree == NULL) return OK;
    collect(tree->left, items, position);
    items[(*position)++] = tree;
    collect(tree->right, items, position);
    return OK;
}

static int by_frequency(const void *first, const void *second)
{
    const Node *a = *(const Node * const *)first;
    const Node *b = *(const Node * const *)second;
    if (a->count != b->count) return a->count > b->count ? -1 : 1;
    return strcmp(a->word, b->word);
}

static Status top_words(const Node *tree, size_t limit, const Node ***result, size_t *count)
{
    if (result == NULL || count == NULL) return INPUT_ERROR;
    *result = NULL;
    *count = 0;
    size_t size;
    unsigned depth;
    const Node *shortest, *longest;
    Status status = statistics(tree, &size, &depth, &shortest, &longest);
    if (status != OK || size == 0 || limit == 0) return status;
    if (size > SIZE_MAX / sizeof(Node *)) return LIMIT_ERROR;
    const Node **items = malloc(size * sizeof(*items));
    if (items == NULL) return MEMORY_ERROR;
    size_t position = 0;
    collect(tree, items, &position);
    qsort(items, size, sizeof(*items), by_frequency);
    *result = items;
    *count = limit < size ? limit : size;
    return OK;
}

static Status save_tree(FILE *file, const Node *tree)
{
    if (file == NULL) return INPUT_ERROR;
    if (tree == NULL) return fputs("#\n", file) == EOF ? FILE_ERROR : OK;
    if (fprintf(file, "%zu ", tree->count) < 0) return FILE_ERROR;
    for (const unsigned char *p = (const unsigned char *)tree->word; *p; p++)
        if (fprintf(file, "%02X", *p) < 0) return FILE_ERROR;
    if (fputc('\n', file) == EOF) return FILE_ERROR;
    Status status = save_tree(file, tree->left);
    if (status == OK) status = save_tree(file, tree->right);
    return status;
}

static Status hex_digit(char c, unsigned *value)
{
    if (value == NULL) return INPUT_ERROR;
    if (c >= '0' && c <= '9') *value = (unsigned)(c - '0');
    else if (c >= 'A' && c <= 'F') *value = (unsigned)(c - 'A' + 10);
    else return INPUT_ERROR;
    return OK;
}

static Status load_tree(FILE *file, unsigned depth, const char *lower, const char *upper, Node **result)
{
    if (file == NULL || result == NULL) return INPUT_ERROR;
    *result = NULL;
    char *line = NULL;
    int ended;
    Status status = read_line(file, &line, &ended);
    if (status != OK) return status;
    if (ended) { free(line); return INPUT_ERROR; }
    if (strcmp(line, "#") == 0) { free(line); return OK; }
    if (depth > 512) { free(line); return LIMIT_ERROR; }
    char *space = strchr(line, ' ');
    size_t count = 0;
    if (space == NULL) status = INPUT_ERROR;
    if (status == OK) { *space = '\0'; status = parse_size(line, &count); }
    size_t hex_length = space == NULL ? 0 : strlen(space + 1);
    if (count == 0 || hex_length == 0 || hex_length % 2) status = INPUT_ERROR;
    Node *node = NULL;
    if (status == OK)
    {
        node = calloc(1, sizeof(*node));
        if (node == NULL) status = MEMORY_ERROR;
    }
    if (status == OK)
    {
        node->word = malloc(hex_length / 2 + 1);
        if (node->word == NULL) status = MEMORY_ERROR;
    }
    if (status == OK)
    {
        for (size_t i = 0; i < hex_length; i += 2)
        {
            unsigned high, low;
            status = hex_digit(space[1 + i], &high);
            if (status == OK) status = hex_digit(space[2 + i], &low);
            if (status != OK || (high == 0 && low == 0)) { status = INPUT_ERROR; break; }
            node->word[i / 2] = (char)(16 * high + low);
        }
        node->word[hex_length / 2] = '\0';
        node->count = count;
    }
    free(line);
    if (status == OK && ((lower != NULL && strcmp(node->word, lower) <= 0) ||
                         (upper != NULL && strcmp(node->word, upper) >= 0))) status = INPUT_ERROR;
    if (status == OK) status = load_tree(file, depth + 1, lower, node->word, &node->left);
    if (status == OK) status = load_tree(file, depth + 1, node->word, upper, &node->right);
    if (status != OK) { clear_tree(node); return status; }
    *result = node;
    return OK;
}

static Status file_tree(const char *path, Node **tree, int loading)
{
    if (path == NULL || *path == '\0' || tree == NULL) return INPUT_ERROR;
    FILE *file = fopen(path, loading ? "r" : "w");
    if (file == NULL) return FILE_ERROR;
    Node *replacement = NULL;
    Status status = loading ? load_tree(file, 1, NULL, NULL, &replacement) : save_tree(file, *tree);
    if (loading && status == OK && fgetc(file) != EOF) status = INPUT_ERROR;
    if (ferror(file)) status = FILE_ERROR;
    if (fclose(file) != 0) status = FILE_ERROR;
    if (loading && status == OK) { clear_tree(*tree); *tree = replacement; }
    else clear_tree(replacement);
    return status;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "Usage: %s text.txt separator ...\n", argv[0]); return 1; }
    unsigned char separators[256] = {0};
    for (int i = 2; i < argc; i++)
    {
        if (strlen(argv[i]) != 1) { fprintf(stderr, "Each separator must be one byte\n"); return 1; }
        separators[(unsigned char)argv[i][0]] = 1;
    }
    FILE *input = fopen(argv[1], "rb");
    if (input == NULL) { fprintf(stderr, "Cannot open input\n"); return 1; }
    Node *tree = NULL;
    Status status = read_words(input, separators, &tree);
    if (fclose(input) != 0) status = FILE_ERROR;
    if (status == OK) status = print_tree(stdout, tree);
    puts("Commands: count, top, extremes, depth, print, save, load, quit.");
    puts("After count/top/save/load enter its argument on the next line.");
    int ended = 0;
    while (status == OK && !ended)
    {
        char *command = NULL, *argument = NULL;
        status = read_line(stdin, &command, &ended);
        if (status == OK && !ended && strcmp(command, "quit") == 0) ended = 1;
        else if (status == OK && !ended)
        {
            int needs_argument = strcmp(command, "count") == 0 || strcmp(command, "top") == 0 ||
                                 strcmp(command, "save") == 0 || strcmp(command, "load") == 0;
            if (needs_argument)
            {
                status = read_line(stdin, &argument, &ended);
                if (status == OK && ended) status = INPUT_ERROR;
            }
            if (status == OK && strcmp(command, "count") == 0)
            {
                size_t count;
                status = find_word(tree, argument, &count);
                if (status == OK && printf("COUNT %zu\n", count) < 0) status = FILE_ERROR;
            }
            else if (status == OK && strcmp(command, "top") == 0)
            {
                size_t limit, count = 0;
                const Node **items = NULL;
                status = parse_size(argument, &limit);
                if (status == OK) status = top_words(tree, limit, &items, &count);
                for (size_t i = 0; status == OK && i < count; i++)
                    if (printf("%s %zu\n", items[i]->word, items[i]->count) < 0) status = FILE_ERROR;
                free(items);
            }
            else if (status == OK && (strcmp(command, "depth") == 0 || strcmp(command, "extremes") == 0))
            {
                size_t size;
                unsigned depth;
                const Node *shortest, *longest;
                status = statistics(tree, &size, &depth, &shortest, &longest);
                if (status == OK && strcmp(command, "depth") == 0) printf("DEPTH %u\n", depth);
                else if (status == OK && size) printf("SHORTEST %s\nLONGEST %s\n", shortest->word, longest->word);
                else if (status == OK) puts("EMPTY");
            }
            else if (status == OK && strcmp(command, "print") == 0) status = print_tree(stdout, tree);
            else if (status == OK && strcmp(command, "save") == 0) status = file_tree(argument, &tree, 0);
            else if (status == OK && strcmp(command, "load") == 0) status = file_tree(argument, &tree, 1);
            else if (status == OK) status = INPUT_ERROR;
        }
        free(command);
        free(argument);
    }
    if (fflush(stdout) != 0) status = FILE_ERROR;
    clear_tree(tree);
    if (status != OK) fprintf(stderr, "Error %d\n", status);
    return status == OK ? 0 : 1;
}
