#include "ring_buf.h"

extern inline uint8_t ring_buf_size(const ring_buf *buf);
extern inline bool ring_buf_is_empty(const ring_buf *buf);
extern inline bool ring_buf_is_full(const ring_buf *buf);
extern inline char ring_buf_shift(ring_buf *buf);
extern inline char ring_buf_peek(const ring_buf *buf);
extern inline void ring_buf_push(ring_buf *buf, char value);
