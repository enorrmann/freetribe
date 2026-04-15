#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#define DOUBLE_BUFFER_SIZE (24*32)



typedef void (*db_callback_t)(uint32_t *buf, uint32_t len,uint32_t total);


/*
 * Double buffer state
 */
typedef struct {
    uint32_t *active;
    uint32_t *inactive;
    uint32_t buffer_a[DOUBLE_BUFFER_SIZE];
    uint32_t buffer_b[DOUBLE_BUFFER_SIZE];

    uint32_t index;       // index inside current buffer
    uint32_t total_count; // total elements pushed (never resets)

    db_callback_t callback;

} double_buffer_t;

/*
 * Init
 */
void db_init(double_buffer_t *db,db_callback_t cb);

/*
 * Push one element
 */
void db_push(double_buffer_t *db, uint32_t value);
#endif // DOUBLE_BUFFER_H