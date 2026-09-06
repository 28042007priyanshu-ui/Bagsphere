#ifndef BAGSPHERE_REPORTS_QUEUE_H
#define BAGSPHERE_REPORTS_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FinderReport {
    char bag_id[32];
    double latitude;
    double longitude;
    long long timestamp;
    char report_id[40];
} FinderReport;

typedef struct QueueNode QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int size;
} ReportQueue;

void queue_init(ReportQueue* q);
int queue_push(ReportQueue* q, const FinderReport* report);
int queue_pop(ReportQueue* q, FinderReport* out);
int queue_empty(const ReportQueue* q);
int queue_size(const ReportQueue* q);
void queue_clear(ReportQueue* q);

#ifdef __cplusplus
}
#endif

#endif
