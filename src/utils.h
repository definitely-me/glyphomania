#pragma once
#include <stdint.h>

#define imin(a, b) ((a) < (b) ? (a) : (b))
#define imax(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
    int max_size;
    int size;
    int next_index;
    int last_index;
    int *queue;
} Queue;

Queue queue_create(int size);

double intersection(double s1, double s2, double e1, double e2);

int clamp(int val, int min, int max);

int ipow(int a, int b);

int round_up(int val, int divisor);

int bfs_nearest(const uint8_t *matrix2d, int matrix_width, int matrix_height, int start_x, int start_y,
                uint8_t trg_val);

int queue_get(Queue *queue);

int queue_dot2d_exists(Queue *q, int x, int y);

void queue_add(Queue *queue, int element);

void queue_free(Queue *queue);
