#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

Queue queue_create(int size) {
    Queue q;
    q.max_size = size;
    q.last_index = 0;
    q.next_index = 0;
    q.queue = malloc(sizeof(int) * size);
    q.size = 0;
    return q;
}

void queue_free(Queue *queue) {
    if (queue != NULL) free(queue->queue);
}

int queue_get(Queue *q) {
    assert(q->size > 0);
    assert(q->next_index < q->last_index);
    int r = q->queue[q->next_index];
    q->next_index = (q->next_index + 1);
    q->size = q->size - 1;
    return r;
}

void queue_add(Queue *q, int element) {
    q->queue[q->last_index] = element;
    q->last_index = q->last_index + 1;
    q->size = q->size + 1;
}

double intersection(double s1, double s2, double e1, double e2) {
    return fmin(e1, e2) - fmax(s1, s2);
}

int clamp(int val, int min, int max) {
    if (val < min) val = min;
    if (val > max) val = max;
    return val;
}

int ipow(int a, int b) {
    int res = a;
    for (int i = 0; i < b - 1; i++) {
        res = res * a;
    }
    return res;
}

int round_up(int val, int divisor) {
    if (val % divisor == 0) return val / divisor;
    return val / divisor + 1;
}

int bfs_nearest(const uint8_t *matrix2d, int matrix_width, int matrix_height, int start_x, int start_y,
                uint8_t trg_val) {
    if (matrix2d[start_y * matrix_width + start_x] == trg_val) return 0;
    int dist = 0;
    Queue q = queue_create(matrix_width * matrix_height * 2);
    uint8_t visited[matrix_width * matrix_height];
    memset(visited, 0, sizeof(visited));

    queue_add(&q, start_x);
    queue_add(&q, start_y);
    visited[start_y * matrix_width + start_x] = 1;

    int q_size = q.size / 2;
    int q_taken = 0;

    int dx[4] = {-1, 0, 1, 0};
    int dy[4] = {0, 1, 0, -1};

    while (q.size > 0) {
        int curr_x = queue_get(&q);
        int curr_y = queue_get(&q);

        for (int i = 0; i < 4; i++) {
            int x = curr_x + dx[i];
            int y = curr_y + dy[i];

            if (x >= 0 && y >= 0 && x < matrix_width && y < matrix_height && !visited[y * matrix_width + x]) {
                if (matrix2d[y * matrix_width + x] == trg_val) return dist + 1;
                queue_add(&q, x);
                queue_add(&q, y);
                visited[y * matrix_width + x] = 1;
            }
        }
        q_taken++;
        if (q_taken == q_size) {
            dist++;
            q_taken = 0;
            q_size = q.size / 2;
        }
    }
    return 255;
}
