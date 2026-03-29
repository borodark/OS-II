#ifndef MB_TERM_H
#define MB_TERM_H

/**
 * @file mb_term.h
 * @brief Tagged 32-bit term representation (AtomVM-inspired 4-bit tags).
 *
 * Term layout on 32-bit:
 *   SMALLINT:  (value << 4) | 0xF   -- 28-bit signed integer
 *   ATOM:      (index << 4) | 0xB   -- 28-bit atom index
 *   PID:       (pid   << 4) | 0x3   -- 28-bit process ID
 *   BOXED:     (offset<< 4) | 0x2   -- heap word offset -> tuple header
 *   CONS:      (offset<< 4) | 0x1   -- heap word offset -> [head, tail]
 *
 * 28-bit signed integer range: -134,217,728 to +134,217,727.
 */

#include <stdint.h>

typedef uint32_t mb_term_t;

/* --- primary tag masks --- */

#define MB_TAG_MASK       0xFU
#define MB_TAG_SMALLINT   0xFU
#define MB_TAG_ATOM       0xBU
#define MB_TAG_PID        0x3U
#define MB_TAG_BOXED      0x2U
#define MB_TAG_CONS       0x1U

/* --- type predicates --- */

#define MB_IS_SMALLINT(t)  (((t) & MB_TAG_MASK) == MB_TAG_SMALLINT)
#define MB_IS_ATOM(t)      (((t) & MB_TAG_MASK) == MB_TAG_ATOM)
#define MB_IS_PID(t)       (((t) & MB_TAG_MASK) == MB_TAG_PID)
#define MB_IS_BOXED(t)     (((t) & MB_TAG_MASK) == MB_TAG_BOXED)
#define MB_IS_CONS(t)      (((t) & MB_TAG_MASK) == MB_TAG_CONS)
#define MB_IS_IMMEDIATE(t) (MB_IS_SMALLINT(t) || MB_IS_ATOM(t) || MB_IS_PID(t))

/* --- small integer --- */

#define MB_MAKE_SMALLINT(v)  ((mb_term_t)(((int32_t)(v) << 4) | MB_TAG_SMALLINT))
#define MB_GET_SMALLINT(t)   ((int32_t)(t) >> 4)

#define MB_SMALLINT_MIN  (-134217728)   /* -(1 << 27) */
#define MB_SMALLINT_MAX  (134217727)    /* (1 << 27) - 1 */

/* --- atom --- */

#define MB_MAKE_ATOM(idx)  ((mb_term_t)(((uint32_t)(idx) << 4) | MB_TAG_ATOM))
#define MB_GET_ATOM(t)     ((uint32_t)(t) >> 4)

/* Well-known atoms */
#define MB_NIL    MB_MAKE_ATOM(0)
#define MB_TRUE   MB_MAKE_ATOM(1)
#define MB_FALSE  MB_MAKE_ATOM(2)

/* --- PID --- */

#define MB_MAKE_PID(p)   ((mb_term_t)(((uint32_t)(p) << 4) | MB_TAG_PID))
#define MB_GET_PID(t)    ((uint32_t)(t) >> 4)

/* --- heap pointers (word offset into process heap) --- */

#define MB_MAKE_BOXED(off)  ((mb_term_t)(((uint32_t)(off) << 4) | MB_TAG_BOXED))
#define MB_GET_BOXED(t)     ((uint32_t)(t) >> 4)

#define MB_MAKE_CONS(off)   ((mb_term_t)(((uint32_t)(off) << 4) | MB_TAG_CONS))
#define MB_GET_CONS(t)      ((uint32_t)(t) >> 4)

/* --- tuple header (stored as first word of boxed object on heap) --- */

#define MB_TUPLE_TAG         0x0U
#define MB_MAKE_TUPLE_HDR(arity) ((mb_term_t)(((uint32_t)(arity) << 6) | MB_TUPLE_TAG))
#define MB_GET_TUPLE_ARITY(hdr)  ((uint32_t)(hdr) >> 6)
#define MB_IS_TUPLE_HDR(w)       (((w) & 0x3FU) == MB_TUPLE_TAG)
#define MB_MAX_TUPLE_ARITY  16

/* --- GC forwarding marker --- */

#define MB_MOVED_MARKER  0xDEAD0002U
#define MB_IS_MOVED(w)   ((w) == MB_MOVED_MARKER)

#endif
