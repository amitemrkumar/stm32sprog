#include "sparse-buffer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HEIGHT 16

typedef struct Node Node;
struct Node {
    MemBlock block;

    int height;
    Node *prev[MAX_HEIGHT];
    Node *next[MAX_HEIGHT];
};

struct SparseBuffer {
    Node *begin;
    Node *curr;
    size_t offset;
};

/** \brief Create a copy of a data buffer.
 *
 * \param data The data to copy.
 * \param length The size of the data buffer in bytes.
 *
 * \return A copy of the given data.  The caller must free() this pointer.
 */
static void *memdup(const void *data, size_t length);

/** \brief Test whether two blocks can be merged into one contiguous block.
 *
 * \param block1 The first block.
 * \param block2 The second block.
 *
 * \return \c true if the blocks can be merged, \c false if there is a gap
 *         between them.
 */
static bool overlap(MemBlock block1, MemBlock block2);

static int randomHeight();
static void insertNode(Node *node, Node *prev[MAX_HEIGHT]);
static void removeNode(Node *node);

static Node *Node_create();
static void Node_destroy(Node *self);
static void Node_addData(Node *node, MemBlock block);

SparseBuffer *SparseBuffer_create() {
    SparseBuffer *self = malloc(sizeof(SparseBuffer));
    if(!self) abort();

    self->begin = Node_create();
    self->begin->height = MAX_HEIGHT;

    self->curr = self->begin;
    self->offset = 0;

    return self;
}

void SparseBuffer_destroy(SparseBuffer *self) {
    Node *node = self->begin;
    while(node != NULL) {
        Node *next = node->next[0];
        Node_destroy(node);
        node = next;
    }
    free(self);
}

void SparseBuffer_set(SparseBuffer *self, MemBlock block) {
    Node *prev[MAX_HEIGHT];
    Node *node = self->begin;
    int level = MAX_HEIGHT - 1;

    while(level >= 0) {
        Node *next = node->next[level];
        if(!next) {
            prev[level] = node;
            level--;
        } else if(overlap(block, next->block)) {
            Node_addData(next, block);
            node = next->next[0];
            if(node && overlap(node->block, next->block)) {
                Node_addData(next, node->block);
                removeNode(node);
            }
            return;
        } else if(block.offset > next->block.offset) {
            node = node->next[level];
        } else {
            prev[level] = node;
            level--;
        }
    }

    node = Node_create();
    Node_addData(node, block);
    insertNode(node, prev);
}

void SparseBuffer_offset(SparseBuffer *self, ptrdiff_t offset) {
    Node *node = self->begin;
    while(node) {
        node->block.offset += offset;
        node = node->next[0];
    }
    self->offset += offset;
}

MemBlock SparseBuffer_read(SparseBuffer *self, size_t length) {
    MemBlock result;

    Node *node = self->curr;
    size_t offset = self->offset;
    size_t end = node->block.offset + node->block.length;
    if(offset >= end) {
        node = node->next[0];
        if(!node) {
            result.offset = 0;
            result.length = 0;
            result.data = NULL;
            return result;
        }
        offset = node->block.offset;
        end = offset + node->block.length;
    }

    if(!length || offset + length > end) {
        length = end - offset;
    }

    size_t diff = offset - node->block.offset;
    result.offset = offset;
    result.length = length;
    result.data = node->block.data + diff;

    self->curr = node;
    self->offset = offset + length;

    return result;
}

size_t SparseBuffer_size(SparseBuffer *self) {
    size_t size = 0;
    Node *node = self->begin;
    while(node) {
        size += node->block.length;
        node = node->next[0];
    }
    return size;
}

void SparseBuffer_rewind(SparseBuffer *self) {
    self->curr = self->begin;
    self->offset = self->curr ? self->curr->block.offset : 0;
}

static void *memdup(const void *data, size_t length) {
    void *copy = malloc(length);
    if(!copy) abort();
    return memcpy(copy, data, length);
}

static bool overlap(MemBlock block1, MemBlock block2) {
    size_t end1 = block1.offset + block1.length;
    size_t end2 = block2.offset + block2.length;
    return ((block2.offset <= block1.offset) && (block1.offset <= end2)) ||
            ((block2.offset <= end1) && (end1 <= end2)) ||
            ((block1.offset <= block2.offset) && (block2.offset <= end1)) ||
            ((block1.offset <= end2) && (end2 <= end1));
}

static int randomHeight() {
    int r = rand();
    int pivot = (RAND_MAX / 2) + 1;
    int height = 1;

    while(r < pivot && height < MAX_HEIGHT) {
        height++;
        pivot /= 2;
    }

    return height;
}

static void insertNode(Node *node, Node *prev[MAX_HEIGHT]) {
    for(int i = 0; i < node->height; ++i) {
        Node *p = prev[i];
        Node *n = p->next[i];

        node->prev[i] = p;
        node->next[i] = n;
        p->next[i] = node;
        if(n) n->prev[i] = node;
    }
}

static void removeNode(Node *node) {
    for(int i = 0; i < node->height; ++i) {
        Node *p = node->prev[i];
        Node *n = node->next[i];
        p->next[i] = n;
        if(n) n->prev[i] = p;
    }
    free(node);
}

static Node *Node_create() {
    Node *self = malloc(sizeof(Node));
    if(!self) abort();

    self->block.offset = 0;
    self->block.length = 0;
    self->block.data = NULL;

    self->height = randomHeight();
    for(int i = 0; i < MAX_HEIGHT; ++i) {
        self->prev[i] = NULL;
        self->next[i] = NULL;
    }

    return self;
}

static void Node_destroy(Node *self) {
    /* const-cast */
    uint8_t *nodeData = (uint8_t *)self->block.data;

    free(nodeData);
    free(self);
}

static void Node_addData(Node *self, MemBlock block) {
    if(!self->block.data) {
        self->block.offset = block.offset;
        self->block.length = block.length;
        self->block.data = memdup(block.data, block.length);
        return;
    }

    assert(overlap(block, self->block));

    size_t end = block.offset + block.length;
    size_t nodeEnd = self->block.offset + self->block.length;

    /* const-cast */
    uint8_t *nodeData = (uint8_t *)self->block.data;

    if(block.offset >= self->block.offset) {
        if(end > nodeEnd) {
            self->block.length = end - self->block.offset;
            nodeData = realloc(nodeData, self->block.length);
            if(!nodeData) abort();
        }
        size_t diff = block.offset - self->block.offset;
        memcpy(nodeData + diff, block.data, block.length);
    } else {
        size_t nTotal = (end >= nodeEnd) ?
                block.length : nodeEnd - block.offset;
        nodeData = realloc(nodeData, nTotal);
        if(!nodeData) abort();
        if(end < nodeEnd) {
            // TODO: Avoid copying data that will be overwritten anyway.
            size_t diff = self->block.offset - block.offset;
            memmove(nodeData + diff, self->block.data, self->block.length);
        }
        memcpy(nodeData, block.data, block.length);
        self->block.offset = block.offset;
        self->block.length = nTotal;
    }

    block.data = nodeData;
}

