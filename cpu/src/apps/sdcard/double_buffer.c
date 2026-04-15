#include "double_buffer.h"

/*
 * Init
 */
void db_init(double_buffer_t *db, db_callback_t cb) {
    ft_printf("Initializing double buffer with callback");
    db->active = db->buffer_a;
    db->inactive = db->buffer_b;
    db->index = 0;
    db->total_count = 0;
    db->callback = cb;
}

/*
 * Push one element
 */
void db_push(double_buffer_t *db, uint32_t value) {
    db->active[db->index++] = value;
    db->total_count++;

    if (db->index >= DOUBLE_BUFFER_SIZE) {
        // callback on full buffer
        if (db->callback) {
            db->callback(db->active, DOUBLE_BUFFER_SIZE, db->total_count);
        }

        // swap buffers
        uint32_t *tmp = db->active;
        db->active = db->inactive;
        db->inactive = tmp;

        db->index = 0;
    }
}
