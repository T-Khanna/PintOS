#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

// 32 bit integer representing a 17.14 fixed point number.
typedef int32_t fixed_point;

#define INTEGER_BITS 17
#define FRACTIONAL_BITS 14
#define FIXED_POINT_FACTOR (1 << FRACTIONAL_BITS)

#define INT_TO_FIXED_POINT(n) (n * FIXED_POINT_FACTOR)

#define TO_INT_ROUND_0(x) (x / FIXED_POINT_FACTOR)
#define TO_INT_ROUND_NEAREST(x) (x >= 0 ? \
    (x + FIXED_POINT_FACTOR / 2) / FIXED_POINT_FACTOR : \
    (x + FIXED_POINT_FACTOR / 2) / FIXED_POINT_FACTOR)

#define ADD_FIXED_POINT_FIXED_POINT(x, y) (x + y)
#define SUB_FIXED_POINT_FIXED_POINT(x, y) (x - y)
#define ADD_FIXED_POINT_INT(x, y) (x + INT_TO_FIXED_POINT(y))
#define SUB_FIXED_POINT_INT(x, y) (x - INT_TO_FIXED_POINT(y))

#define MUL_FIXED_POINT_FIXED_POINT(x, y) (((int64_t) x) * y / f)
#define MUL_FIXED_POINT_INT(x, n) (x * n)
#define DIV_FIXED_POINT_FIXED_POINT(x, y) (((int64_t) x) * f / y)
#define DIV_FIXED_POINT_INT(x, n) (x / n)

#endif /* threads/fixed-point.h */
