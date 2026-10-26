#include "lab_input.h"
#include <inttypes.h>

typedef struct Node {
    uint64_t weight;
    int byte;
    struct Node *left, *right;
} Node;

typedef struct {
    Node *items[256];
    size_t size;
} Heap;

typedef struct {
    uint64_t frequencies[256];
    char codes[256][256];
    uint64_t original, bits, bytes;
} Report;

Status heap_push(Heap *heap, Node *node)
{
    if (!heap || !node || heap->size >= 256) return INPUT_ERROR;
    size_t i = heap->size++;
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (heap->items[parent]->weight <= node->weight) break;
        heap->items[i] = heap->items[parent];
        i = parent;
    }
    heap->items[i] = node;
    return OK;
}

Status heap_pop(Heap *heap, Node **out)
{
    if (!heap || !out || heap->size == 0 || heap->size > 256) return INPUT_ERROR;
    *out = heap->items[0];
    Node *last = heap->items[--heap->size];
    if (heap->size == 0) return OK;
    size_t i = 0;
    while (i * 2 + 1 < heap->size) {
        size_t child = i * 2 + 1;
        if (child + 1 < heap->size && heap->items[child + 1]->weight < heap->items[child]->weight)
            child++;
        if (last->weight <= heap->items[child]->weight) break;
        heap->items[i] = heap->items[child];
        i = child;
    }
    heap->items[i] = last;
    return OK;
}

Status count_bytes(FILE *file, Report *report)
{
    if (!file || !report) return INPUT_ERROR;
    memset(report, 0, sizeof(*report));
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (report->original == UINT64_MAX) return RANGE_ERROR;
        report->original++;
        report->frequencies[(unsigned char)c]++;
    }
    return ferror(file) ? IO_ERROR : OK;
}

Status assign_codes(const Node *node, char *path, size_t depth, Report *report)
{
    if (!node || !path || !report || depth > 255) return INPUT_ERROR;
    if (node->byte >= 0) {
        if (node->byte > 255) return INPUT_ERROR;
        if (depth == 0) path[depth++] = '0';
        path[depth] = '\0';
        memcpy(report->codes[node->byte], path, depth + 1);
        uint64_t frequency = report->frequencies[node->byte];
        if (frequency > (UINT64_MAX - report->bits) / depth) return RANGE_ERROR;
        report->bits += frequency * depth;
        return OK;
    }
    if (depth == 255 || !node->left || !node->right) return INPUT_ERROR;
    path[depth] = '0';
    Status status = assign_codes(node->left, path, depth + 1, report);
    if (status != OK) return status;
    path[depth] = '1';
    return assign_codes(node->right, path, depth + 1, report);
}

Status build_report(Report *report)
{
    if (!report) return INPUT_ERROR;
    report->bits = 0;
    report->bytes = 0;
    memset(report->codes, 0, sizeof(report->codes));
    uint64_t total = 0;
    for (size_t i = 0; i < 256; i++) {
        if (report->frequencies[i] > UINT64_MAX - total) return RANGE_ERROR;
        total += report->frequencies[i];
    }
    if (total != report->original) return INPUT_ERROR;
    /* The complete byte alphabet needs at most 511 nodes. */
    Node nodes[511];
    size_t used = 0;
    Heap heap = { .size = 0 };
    Status status = OK;
    for (int i = 0; i < 256 && status == OK; i++) {
        if (report->frequencies[i]) {
            nodes[used] = (Node){ report->frequencies[i], i, NULL, NULL };
            status = heap_push(&heap, &nodes[used++]);
        }
    }
    while (heap.size > 1 && status == OK) {
        Node *left = NULL, *right = NULL;
        status = heap_pop(&heap, &left);
        if (status == OK) status = heap_pop(&heap, &right);
        if (status == OK && left->weight > UINT64_MAX - right->weight) status = RANGE_ERROR;
        if (status == OK) {
            nodes[used] = (Node){ left->weight + right->weight, -1, left, right };
            status = heap_push(&heap, &nodes[used++]);
        }
    }
    if (status == OK && heap.size) {
        char path[256];
        status = assign_codes(heap.items[0], path, 0, report);
    }
    if (status == OK) report->bytes = report->bits / 8 + (report->bits % 8 != 0);
    return status;
}

Status print_report(FILE *file, const Report *report)
{
    if (!file || !report) return INPUT_ERROR;
    if (fprintf(file, "BYTE FREQUENCY CODE\n") < 0) return IO_ERROR;
    for (int i = 0; i < 256; i++) {
        if (report->frequencies[i] && fprintf(file, "%d %" PRIu64 " %s\n",
            i, report->frequencies[i], report->codes[i]) < 0) return IO_ERROR;
    }
    double saving = report->original ? 100.0 * (1.0 - (double)report->bytes / report->original) : 0.0;
    if (fprintf(file, "ORIGINAL_BYTES %" PRIu64 "\nENCODED_BITS %" PRIu64
        "\nENCODED_BYTES %" PRIu64 "\nSAVING_PERCENT %.6f\n",
        report->original, report->bits, report->bytes, saving) < 0) return IO_ERROR;
    return fflush(file) == EOF ? IO_ERROR : OK;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s binary_file\n", argv[0]); return 1; }
    FILE *file = fopen(argv[1], "rb");
    if (!file) { fprintf(stderr, "Cannot open input file.\n"); return 1; }
    Report report;
    Status status = count_bytes(file, &report);
    if (fclose(file) != 0 && status == OK) status = IO_ERROR;
    if (status == OK) status = build_report(&report);
    if (status == OK) status = print_report(stdout, &report);
    if (status != OK) { fprintf(stderr, "Huffman error: %d\n", status); return 1; }
    return 0;
}
