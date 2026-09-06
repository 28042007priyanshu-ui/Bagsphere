#include "reports_queue.h"
#include <stdlib.h>
#include <string.h>

/* Pure C FIFO queue used by the C++ server for incoming finder reports. */

struct QueueNode {
    FinderReport report;
    struct QueueNode* next;
};

void queue_init(ReportQueue* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

int queue_push(ReportQueue* q, const FinderReport* report) {
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return 0;
    memcpy(&node->report, report, sizeof(FinderReport));
    node->next = NULL;

    if (q->rear) q->rear->next = node;
    else q->front = node;

    q->rear = node;
    q->size++;
    return 1;
}

int queue_pop(ReportQueue* q, FinderReport* out) {
    if (!q->front) return 0;

    QueueNode* node = q->front;
    if (out) memcpy(out, &node->report, sizeof(FinderReport));

    q->front = node->next;
    if (!q->front) q->rear = NULL;
    q->size--;

    free(node);
    return 1;
}

int queue_empty(const ReportQueue* q) {
    return q->front == NULL;
}

int queue_size(const ReportQueue* q) {
    return q->size;
}

void queue_clear(ReportQueue* q) {
    FinderReport tmp;
    while (queue_pop(q, &tmp)) {}
}
