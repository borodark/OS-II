#include "mb_heap.h"

#include <string.h>

void mb_heap_init(mb_heap_t *heap) {
    memset(heap, 0, sizeof(*heap));
    heap->from = heap->space_a;
    heap->to = heap->space_b;
    heap->capacity = MB_HEAP_WORDS;
}

mb_term_t *mb_heap_alloc(mb_heap_t *heap, size_t n_words) {
    mb_term_t *ptr;
    if (heap->hp + n_words > heap->capacity) {
        return NULL;
    }
    ptr = &heap->from[heap->hp];
    heap->hp += n_words;
    return ptr;
}

mb_term_t mb_heap_make_tuple(mb_heap_t *heap, const mb_term_t *elems, uint8_t arity) {
    size_t need = 1 + (size_t)arity;  /* header + elements */
    mb_term_t *ptr;
    size_t offset;
    uint8_t i;

    ptr = mb_heap_alloc(heap, need);
    if (ptr == NULL) {
        return 0;
    }

    offset = mb_heap_offset(heap, ptr);
    ptr[0] = MB_MAKE_TUPLE_HDR(arity);
    for (i = 0; i < arity; i++) {
        ptr[1 + i] = elems[i];
    }
    return MB_MAKE_BOXED(offset);
}

mb_term_t mb_heap_cons(mb_heap_t *heap, mb_term_t head, mb_term_t tail) {
    mb_term_t *ptr;
    size_t offset;

    ptr = mb_heap_alloc(heap, 2);
    if (ptr == NULL) {
        return 0;
    }

    offset = mb_heap_offset(heap, ptr);
    ptr[0] = head;
    ptr[1] = tail;
    return MB_MAKE_CONS(offset);
}

/* --- Cheney's copying GC --- */

/**
 * Copy a single term from old from-space to new from-space.
 * Returns the updated term (with new offset if heap pointer).
 * If the object was already moved, follows the forwarding pointer.
 */
static mb_term_t mb_gc_copy_term(mb_heap_t *heap,
                                 mb_term_t *old_space,
                                 mb_term_t term) {
    size_t old_off, new_off, size;
    mb_term_t *old_ptr;
    mb_term_t hdr;

    /* Immediates (smallint, atom, pid) and zero need no copying. */
    if (MB_IS_IMMEDIATE(term) || term == 0) {
        return term;
    }

    if (MB_IS_BOXED(term)) {
        old_off = MB_GET_BOXED(term);
        old_ptr = &old_space[old_off];

        /* Already forwarded? */
        if (MB_IS_MOVED(old_ptr[0])) {
            return MB_MAKE_BOXED(old_ptr[1]);
        }

        /* Tuple: header + arity elements */
        hdr = old_ptr[0];
        if (!MB_IS_TUPLE_HDR(hdr)) {
            return term; /* unknown boxed type, don't move */
        }
        size = 1 + MB_GET_TUPLE_ARITY(hdr);

        /* Copy to new from-space */
        new_off = heap->hp;
        memcpy(&heap->from[new_off], old_ptr, size * sizeof(mb_term_t));
        heap->hp += size;

        /* Leave forwarding pointer */
        old_ptr[0] = MB_MOVED_MARKER;
        old_ptr[1] = (mb_term_t)new_off;

        return MB_MAKE_BOXED(new_off);
    }

    if (MB_IS_CONS(term)) {
        old_off = MB_GET_CONS(term);
        old_ptr = &old_space[old_off];

        /* Already forwarded? */
        if (MB_IS_MOVED(old_ptr[0])) {
            return MB_MAKE_CONS(old_ptr[1]);
        }

        size = 2; /* head + tail */
        new_off = heap->hp;
        memcpy(&heap->from[new_off], old_ptr, size * sizeof(mb_term_t));
        heap->hp += size;

        /* Leave forwarding pointer */
        old_ptr[0] = MB_MOVED_MARKER;
        old_ptr[1] = (mb_term_t)new_off;

        return MB_MAKE_CONS(new_off);
    }

    return term;
}

void mb_heap_gc(mb_heap_t *heap, mb_term_t **roots, size_t n_roots) {
    mb_term_t *old_space;
    size_t scan;
    size_t i;

    /* Swap spaces: to becomes the new from. */
    old_space = heap->from;
    heap->from = heap->to;
    heap->to = old_space;
    heap->hp = 0;

    /* Phase 1: copy root terms. */
    for (i = 0; i < n_roots; i++) {
        *roots[i] = mb_gc_copy_term(heap, old_space, *roots[i]);
    }

    /* Phase 2: Cheney scan — BFS over copied objects. */
    scan = 0;
    while (scan < heap->hp) {
        mb_term_t w = heap->from[scan];

        if (MB_IS_TUPLE_HDR(w)) {
            /* Scan tuple elements */
            size_t arity = MB_GET_TUPLE_ARITY(w);
            size_t j;
            scan++; /* skip header */
            for (j = 0; j < arity; j++) {
                heap->from[scan] = mb_gc_copy_term(heap, old_space, heap->from[scan]);
                scan++;
            }
        } else {
            /* Cons cell or other: scan each word */
            heap->from[scan] = mb_gc_copy_term(heap, old_space, heap->from[scan]);
            scan++;
        }
    }

    /* Clear old space for debugging visibility. */
    memset(old_space, 0, heap->capacity * sizeof(mb_term_t));

    heap->gc_count++;
}
