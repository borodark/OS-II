#ifndef MB_HEAP_H
#define MB_HEAP_H

/**
 * @file mb_heap.h
 * @brief Per-process semi-space heap with bump allocator and Cheney's GC.
 *
 * Each process owns two equal-sized spaces (A and B).  Allocation bumps
 * a pointer in the active (from) space.  When full, Cheney's copying GC
 * copies live data to the inactive (to) space and swaps.
 *
 * Heap words are mb_term_t (uint32_t).  Tuples are stored as
 * [header, elem_0, ..., elem_{arity-1}].  Cons cells are [head, tail].
 */

#include <stddef.h>

#include "mb_term.h"

#ifndef MB_HEAP_WORDS
#define MB_HEAP_WORDS 128  /* words per semi-space (512 bytes) */
#endif

typedef struct {
    mb_term_t  space_a[MB_HEAP_WORDS];
    mb_term_t  space_b[MB_HEAP_WORDS];
    mb_term_t *from;
    mb_term_t *to;
    size_t     hp;         /* next free word offset in from-space */
    size_t     capacity;   /* = MB_HEAP_WORDS */
    uint32_t   gc_count;
} mb_heap_t;

/**
 * @brief Initialize heap (both spaces zeroed, from=A, to=B).
 */
void mb_heap_init(mb_heap_t *heap);

/**
 * @brief Allocate n_words in from-space.
 *
 * @return Pointer to allocated region, or NULL if space insufficient.
 *         Caller must trigger GC on NULL and retry.
 */
mb_term_t *mb_heap_alloc(mb_heap_t *heap, size_t n_words);

/**
 * @brief Allocate a tuple on the heap.
 *
 * @param heap Process heap.
 * @param elems Array of element terms.
 * @param arity Tuple size (0..MB_MAX_TUPLE_ARITY).
 * @return Tagged BOXED term, or 0 on allocation failure.
 */
mb_term_t mb_heap_make_tuple(mb_heap_t *heap, const mb_term_t *elems, uint8_t arity);

/**
 * @brief Allocate a cons cell on the heap.
 *
 * @return Tagged CONS term, or 0 on allocation failure.
 */
mb_term_t mb_heap_cons(mb_heap_t *heap, mb_term_t head, mb_term_t tail);

/**
 * @brief Run Cheney's copying GC.
 *
 * Copies live data reachable from roots to the to-space, then swaps.
 *
 * @param heap Process heap.
 * @param roots Array of pointers to root terms (updated in place).
 * @param n_roots Number of roots.
 */
void mb_heap_gc(mb_heap_t *heap, mb_term_t **roots, size_t n_roots);

/**
 * @brief Get the word offset for a pointer into from-space.
 */
static inline size_t mb_heap_offset(const mb_heap_t *heap, const mb_term_t *ptr) {
    return (size_t)(ptr - heap->from);
}

#endif
