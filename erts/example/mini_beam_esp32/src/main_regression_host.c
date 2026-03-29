#include <stdio.h>

#include "mb_vm.h"
#include "mb_scheduler.h"
#include "mb_term.h"

#define I32LE(v) \
    (uint8_t)((v) & 0xff), \
    (uint8_t)(((v) >> 8) & 0xff), \
    (uint8_t)(((v) >> 16) & 0xff), \
    (uint8_t)(((v) >> 24) & 0xff)

static int failures = 0;

static void check_int(const char *name, int expected, int actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL %s: expected=%d actual=%d\n", name, expected, actual);
        failures++;
    }
}

/* ---- original vm-compat tests ---- */

static void test_invalid_command_rejected(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};

    mb_vm_init(&vm, NULL, 0);
    cmd.type = 999;
    check_int("invalid_command", MB_INVALID_COMMAND, mb_vm_mailbox_push(&vm, cmd));
}

static void test_bad_argument_rejected(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};

    mb_vm_init(&vm, NULL, 0);
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 100;
    cmd.b = 1;
    check_int("bad_arg", MB_BAD_ARGUMENT, mb_vm_mailbox_push(&vm, cmd));
}

static void test_mailbox_full(void) {
    mb_vm_t vm;
    mb_command_t cmd = {0};
    int i;

    mb_vm_init(&vm, NULL, 0);
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 2;
    cmd.b = 1;

    for (i = 0; i < MB_MAILBOX_CAPACITY; i++) {
        check_int("mailbox_push_ok", MB_OK, mb_vm_mailbox_push(&vm, cmd));
    }
    check_int("mailbox_full", MB_MAILBOX_FULL, mb_vm_mailbox_push(&vm, cmd));
}

static void test_invalid_opcode(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {0x7E};
    mb_vm_init(&vm, program, sizeof(program));
    check_int("invalid_opcode", MB_BAD_OPCODE, mb_vm_run(&vm, 8));
}

static void test_bad_register_decode(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {
        MB_OP_CONST_I32, 77, I32LE(1)
    };
    mb_vm_init(&vm, program, sizeof(program));
    check_int("bad_reg", MB_BAD_REG, mb_vm_run(&vm, 8));
}

static void test_recv_empty_is_nonfatal(void) {
    mb_vm_t vm;
    static const uint8_t program[] = {
        MB_OP_RECV_CMD, 0, 1, 2, 3, 4,
        MB_OP_HALT
    };

    mb_vm_init(&vm, program, sizeof(program));
    check_int("recv_empty_run", MB_OK, mb_vm_run(&vm, 8));
    check_int("recv_empty_type", MB_CMD_NONE, MB_GET_SMALLINT(vm.regs[0]));
    check_int("recv_empty_status", MB_MAILBOX_EMPTY, MB_GET_SMALLINT(vm.regs[1]));
}

/* ---- scheduler tests ---- */

static void test_sched_spawn_and_halt(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    int rc;
    static const uint8_t prog[] = { MB_OP_HALT };

    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));
    check_int("spawn_pid", 1, pid);
    check_int("sched_count", 1, sched.count);

    rc = mb_sched_tick(&sched);
    check_int("tick_halt", MB_OK, rc);
    check_int("proc_halted", MB_PROC_HALTED, mb_sched_proc(&sched, pid)->state);

    /* No more runnable processes */
    rc = mb_sched_tick(&sched);
    check_int("tick_idle", MB_SCHED_IDLE, rc);
}

static void test_sched_full_table(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    int i;
    static const uint8_t prog[] = { MB_OP_HALT };

    mb_sched_init(&sched);
    for (i = 0; i < MB_MAX_PROCESSES; i++) {
        pid = mb_sched_spawn(&sched, prog, sizeof(prog));
        check_int("spawn_fill", i + 1, pid);
    }
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));
    check_int("spawn_full", MB_PID_NONE, pid);
}

static void test_sched_send_ext(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    mb_command_t cmd = {0};
    int rc;

    /* Program: RECV_CMD then HALT */
    static const uint8_t prog[] = {
        MB_OP_RECV_CMD, 0, 1, 2, 3, 4,
        MB_OP_HALT
    };

    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));

    /* First tick: RECV_CMD finds empty mailbox -> WAITING */
    rc = mb_sched_tick(&sched);
    check_int("tick_waiting", MB_OK, rc);
    check_int("proc_waiting", MB_PROC_WAITING, mb_sched_proc(&sched, pid)->state);

    /* No runnable -> IDLE */
    rc = mb_sched_tick(&sched);
    check_int("tick_idle_waiting", MB_SCHED_IDLE, rc);

    /* External send wakes the process */
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 2;
    cmd.b = 1;
    rc = mb_sched_send(&sched, pid, cmd);
    check_int("ext_send_ok", MB_OK, rc);
    check_int("proc_ready", MB_PROC_READY, mb_sched_proc(&sched, pid)->state);

    /* Now it should run: RECV_CMD succeeds, then HALT */
    rc = mb_sched_tick(&sched);
    check_int("tick_recv", MB_OK, rc);
    check_int("recv_type", MB_CMD_GPIO_WRITE, MB_GET_SMALLINT(mb_sched_proc(&sched, pid)->regs[0]));
    check_int("recv_a", 2, MB_GET_SMALLINT(mb_sched_proc(&sched, pid)->regs[1]));
    check_int("proc_halted_2", MB_PROC_HALTED, mb_sched_proc(&sched, pid)->state);
}

static void test_sched_send_bad_pid(void) {
    mb_scheduler_t sched;
    mb_command_t cmd = {0};

    mb_sched_init(&sched);
    cmd.type = MB_CMD_GPIO_WRITE;
    cmd.a = 2;
    cmd.b = 1;
    check_int("send_bad_pid", MB_BAD_PID, mb_sched_send(&sched, 99, cmd));
}

static void test_self_opcode(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    int rc;
    static const uint8_t prog[] = {
        MB_OP_SELF, 0,
        MB_OP_HALT
    };

    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));

    rc = mb_sched_tick(&sched);
    check_int("self_tick", MB_OK, rc);
    check_int("self_pid", (int)MB_MAKE_PID(pid), (int)mb_sched_proc(&sched, pid)->regs[0]);
}

static void test_yield_opcode(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    int rc;
    /* YIELD then CONST_I32 r0=42 then HALT */
    static const uint8_t prog[] = {
        MB_OP_YIELD,
        MB_OP_CONST_I32, 0, I32LE(42),
        MB_OP_HALT
    };

    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));

    /* First tick: executes YIELD, reduction budget exhausted */
    rc = mb_sched_tick(&sched);
    check_int("yield_tick1", MB_OK, rc);
    check_int("yield_not_halted", MB_PROC_READY, mb_sched_proc(&sched, pid)->state);

    /* Second tick: continues with CONST_I32 + HALT */
    rc = mb_sched_tick(&sched);
    check_int("yield_tick2", MB_OK, rc);
    check_int("yield_r0", 42, MB_GET_SMALLINT(mb_sched_proc(&sched, pid)->regs[0]));
    check_int("yield_halted", MB_PROC_HALTED, mb_sched_proc(&sched, pid)->state);
}

static void test_two_process_round_robin(void) {
    mb_scheduler_t sched;
    mb_pid_t pid_a, pid_b;
    int rc, ticks;

    /* Both processes: load a constant then halt */
    static const uint8_t prog_a[] = {
        MB_OP_CONST_I32, 0, I32LE(100),
        MB_OP_HALT
    };
    static const uint8_t prog_b[] = {
        MB_OP_CONST_I32, 0, I32LE(200),
        MB_OP_HALT
    };

    mb_sched_init(&sched);
    pid_a = mb_sched_spawn(&sched, prog_a, sizeof(prog_a));
    pid_b = mb_sched_spawn(&sched, prog_b, sizeof(prog_b));

    ticks = 0;
    while (ticks < 10) {
        rc = mb_sched_tick(&sched);
        if (rc == MB_SCHED_IDLE) break;
        check_int("rr_tick", MB_OK, rc);
        ticks++;
    }

    check_int("rr_a_val", 100, MB_GET_SMALLINT(mb_sched_proc(&sched, pid_a)->regs[0]));
    check_int("rr_b_val", 200, MB_GET_SMALLINT(mb_sched_proc(&sched, pid_b)->regs[0]));
    check_int("rr_a_state", MB_PROC_HALTED, mb_sched_proc(&sched, pid_a)->state);
    check_int("rr_b_state", MB_PROC_HALTED, mb_sched_proc(&sched, pid_b)->state);
}

/* ---- term tagging tests ---- */

static void test_smallint_roundtrip(void) {
    mb_term_t t;
    int values[] = {0, 1, -1, 42, -42, 127, -128, 100000, -100000,
                    MB_SMALLINT_MAX, MB_SMALLINT_MIN};
    int i;
    for (i = 0; i < (int)(sizeof(values)/sizeof(values[0])); i++) {
        t = MB_MAKE_SMALLINT(values[i]);
        check_int("smallint_is", 1, MB_IS_SMALLINT(t));
        check_int("smallint_not_atom", 0, MB_IS_ATOM(t));
        check_int("smallint_not_pid", 0, MB_IS_PID(t));
        check_int("smallint_not_boxed", 0, MB_IS_BOXED(t));
        check_int("smallint_not_cons", 0, MB_IS_CONS(t));
        check_int("smallint_roundtrip", values[i], MB_GET_SMALLINT(t));
    }
}

static void test_atom_roundtrip(void) {
    mb_term_t t;
    check_int("nil_is_atom", 1, MB_IS_ATOM(MB_NIL));
    check_int("nil_idx", 0, (int)MB_GET_ATOM(MB_NIL));
    check_int("true_is_atom", 1, MB_IS_ATOM(MB_TRUE));
    check_int("true_idx", 1, (int)MB_GET_ATOM(MB_TRUE));
    check_int("false_is_atom", 1, MB_IS_ATOM(MB_FALSE));
    check_int("false_idx", 2, (int)MB_GET_ATOM(MB_FALSE));

    t = MB_MAKE_ATOM(999);
    check_int("atom_roundtrip", 999, (int)MB_GET_ATOM(t));
    check_int("atom_not_int", 0, MB_IS_SMALLINT(t));
}

static void test_pid_roundtrip(void) {
    mb_term_t t = MB_MAKE_PID(5);
    check_int("pid_is", 1, MB_IS_PID(t));
    check_int("pid_val", 5, (int)MB_GET_PID(t));
    check_int("pid_not_int", 0, MB_IS_SMALLINT(t));
    check_int("pid_not_atom", 0, MB_IS_ATOM(t));
}

static void test_boxed_cons_roundtrip(void) {
    mb_term_t b = MB_MAKE_BOXED(10);
    mb_term_t c = MB_MAKE_CONS(20);
    check_int("boxed_is", 1, MB_IS_BOXED(b));
    check_int("boxed_off", 10, (int)MB_GET_BOXED(b));
    check_int("cons_is", 1, MB_IS_CONS(c));
    check_int("cons_off", 20, (int)MB_GET_CONS(c));
    check_int("boxed_not_cons", 0, MB_IS_CONS(b));
    check_int("cons_not_boxed", 0, MB_IS_BOXED(c));
}

static void test_tuple_header(void) {
    mb_term_t h = MB_MAKE_TUPLE_HDR(3);
    check_int("tuple_hdr", 1, MB_IS_TUPLE_HDR(h));
    check_int("tuple_arity", 3, (int)MB_GET_TUPLE_ARITY(h));
}

/* ---- heap and GC tests ---- */

static void test_heap_alloc_basic(void) {
    mb_heap_t heap;
    mb_term_t *p1, *p2;

    mb_heap_init(&heap);
    check_int("heap_hp_init", 0, (int)heap.hp);

    p1 = mb_heap_alloc(&heap, 3);
    check_int("alloc1_ok", 1, p1 != NULL);
    check_int("heap_hp_3", 3, (int)heap.hp);

    p2 = mb_heap_alloc(&heap, 2);
    check_int("alloc2_ok", 1, p2 != NULL);
    check_int("heap_hp_5", 5, (int)heap.hp);

    /* Exhaust heap */
    p1 = mb_heap_alloc(&heap, MB_HEAP_WORDS);
    check_int("alloc_full", 1, p1 == NULL);
}

static void test_heap_make_tuple(void) {
    mb_heap_t heap;
    mb_term_t elems[3];
    mb_term_t tup;
    mb_term_t *ptr;

    mb_heap_init(&heap);

    elems[0] = MB_MAKE_SMALLINT(10);
    elems[1] = MB_MAKE_SMALLINT(20);
    elems[2] = MB_MAKE_SMALLINT(30);

    tup = mb_heap_make_tuple(&heap, elems, 3);
    check_int("tup_is_boxed", 1, MB_IS_BOXED(tup));

    ptr = &heap.from[MB_GET_BOXED(tup)];
    check_int("tup_hdr", 1, MB_IS_TUPLE_HDR(ptr[0]));
    check_int("tup_arity", 3, (int)MB_GET_TUPLE_ARITY(ptr[0]));
    check_int("tup_e0", 10, MB_GET_SMALLINT(ptr[1]));
    check_int("tup_e1", 20, MB_GET_SMALLINT(ptr[2]));
    check_int("tup_e2", 30, MB_GET_SMALLINT(ptr[3]));
    check_int("heap_hp_4", 4, (int)heap.hp);
}

static void test_heap_cons(void) {
    mb_heap_t heap;
    mb_term_t cell;
    mb_term_t *ptr;

    mb_heap_init(&heap);

    cell = mb_heap_cons(&heap, MB_MAKE_SMALLINT(1), MB_NIL);
    check_int("cons_is_cons", 1, MB_IS_CONS(cell));

    ptr = &heap.from[MB_GET_CONS(cell)];
    check_int("cons_head", 1, MB_GET_SMALLINT(ptr[0]));
    check_int("cons_tail_nil", 1, ptr[1] == MB_NIL);
}

static void test_gc_survives_live_data(void) {
    mb_heap_t heap;
    mb_term_t root;
    mb_term_t *roots[1];
    mb_term_t elems[2];
    mb_term_t *ptr;

    mb_heap_init(&heap);

    elems[0] = MB_MAKE_SMALLINT(42);
    elems[1] = MB_MAKE_SMALLINT(99);
    root = mb_heap_make_tuple(&heap, elems, 2);
    check_int("gc_pre_hp", 3, (int)heap.hp);

    roots[0] = &root;
    mb_heap_gc(&heap, roots, 1);

    check_int("gc_count_1", 1, (int)heap.gc_count);
    check_int("gc_post_hp", 3, (int)heap.hp); /* same size — live data copied */
    check_int("gc_root_boxed", 1, MB_IS_BOXED(root));

    ptr = &heap.from[MB_GET_BOXED(root)];
    check_int("gc_tup_hdr", 1, MB_IS_TUPLE_HDR(ptr[0]));
    check_int("gc_tup_arity", 2, (int)MB_GET_TUPLE_ARITY(ptr[0]));
    check_int("gc_tup_e0", 42, MB_GET_SMALLINT(ptr[1]));
    check_int("gc_tup_e1", 99, MB_GET_SMALLINT(ptr[2]));
}

static void test_gc_reclaims_dead_data(void) {
    mb_heap_t heap;
    mb_term_t root;
    mb_term_t *roots[1];
    mb_term_t elems[2];

    mb_heap_init(&heap);

    /* Allocate some dead data first */
    elems[0] = MB_MAKE_SMALLINT(1);
    elems[1] = MB_MAKE_SMALLINT(2);
    (void)mb_heap_make_tuple(&heap, elems, 2);  /* dead — no root points here */

    elems[0] = MB_MAKE_SMALLINT(3);
    elems[1] = MB_MAKE_SMALLINT(4);
    (void)mb_heap_make_tuple(&heap, elems, 2);  /* also dead */

    check_int("gc_pre_hp_6", 6, (int)heap.hp); /* 2 tuples x 3 words */

    /* Only root is a small integer (no heap reference) */
    root = MB_MAKE_SMALLINT(77);
    roots[0] = &root;
    mb_heap_gc(&heap, roots, 1);

    /* All heap data was dead, should be reclaimed */
    check_int("gc_reclaimed_hp", 0, (int)heap.hp);
    check_int("gc_root_unchanged", 77, MB_GET_SMALLINT(root));
}

static void test_gc_nested_structures(void) {
    mb_heap_t heap;
    mb_term_t inner_elems[2];
    mb_term_t outer_elems[2];
    mb_term_t inner, outer;
    mb_term_t *roots[1];
    mb_term_t *ptr;
    mb_term_t *inner_ptr;

    mb_heap_init(&heap);

    /* Build: {100, {200, 300}} */
    inner_elems[0] = MB_MAKE_SMALLINT(200);
    inner_elems[1] = MB_MAKE_SMALLINT(300);
    inner = mb_heap_make_tuple(&heap, inner_elems, 2);

    outer_elems[0] = MB_MAKE_SMALLINT(100);
    outer_elems[1] = inner;
    outer = mb_heap_make_tuple(&heap, outer_elems, 2);

    check_int("nested_pre_hp", 6, (int)heap.hp);

    /* Also allocate dead data to prove GC only copies live */
    (void)mb_heap_cons(&heap, MB_MAKE_SMALLINT(999), MB_NIL); /* dead */
    check_int("nested_pre_hp_dead", 8, (int)heap.hp);

    roots[0] = &outer;
    mb_heap_gc(&heap, roots, 1);

    /* Should have copied 6 words (two tuples), dead cons reclaimed */
    check_int("nested_post_hp", 6, (int)heap.hp);

    /* Verify structure integrity */
    check_int("nested_outer_boxed", 1, MB_IS_BOXED(outer));
    ptr = &heap.from[MB_GET_BOXED(outer)];
    check_int("nested_outer_e0", 100, MB_GET_SMALLINT(ptr[1]));

    check_int("nested_inner_boxed", 1, MB_IS_BOXED(ptr[2]));
    inner_ptr = &heap.from[MB_GET_BOXED(ptr[2])];
    check_int("nested_inner_e0", 200, MB_GET_SMALLINT(inner_ptr[1]));
    check_int("nested_inner_e1", 300, MB_GET_SMALLINT(inner_ptr[2]));
}

/* ---- heap opcode tests (run via bytecode) ---- */

static void test_opcode_make_tuple_and_elem(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    mb_process_t *p;
    int rc;

    /*
     * r0 = 10, r1 = 20, r2 = 30
     * MAKE_TUPLE r3, arity=3, r0, r1, r2   -> r3 = {10, 20, 30}
     * TUPLE_ELEM r4, r3, 0                 -> r4 = 10
     * TUPLE_ELEM r5, r3, 1                 -> r5 = 20
     * TUPLE_ELEM r6, r3, 2                 -> r6 = 30
     * HALT
     */
    static const uint8_t prog[] = {
        MB_OP_CONST_I32, 0, I32LE(10),
        MB_OP_CONST_I32, 1, I32LE(20),
        MB_OP_CONST_I32, 2, I32LE(30),
        MB_OP_MAKE_TUPLE, 3, 3, 0, 1, 2,
        MB_OP_TUPLE_ELEM, 4, 3, 0,
        MB_OP_TUPLE_ELEM, 5, 3, 1,
        MB_OP_TUPLE_ELEM, 6, 3, 2,
        MB_OP_HALT
    };

    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, prog, sizeof(prog));

    while ((rc = mb_sched_tick(&sched)) == MB_OK) {}
    check_int("tuple_idle", MB_SCHED_IDLE, rc);

    p = mb_sched_proc(&sched, pid);
    check_int("tuple_halted", MB_PROC_HALTED, p->state);
    check_int("tuple_r3_boxed", 1, MB_IS_BOXED(p->regs[3]));
    check_int("tuple_elem0", 10, MB_GET_SMALLINT(p->regs[4]));
    check_int("tuple_elem1", 20, MB_GET_SMALLINT(p->regs[5]));
    check_int("tuple_elem2", 30, MB_GET_SMALLINT(p->regs[6]));
}

static void test_opcode_cons_head_tail(void) {
    mb_scheduler_t sched;
    mb_pid_t pid;
    mb_process_t *p;
    int rc;

    /*
     * r0 = 1, r1 = nil
     * CONS r2, r0, r1    -> r2 = [1 | nil]
     * r3 = 2
     * CONS r4, r3, r2    -> r4 = [2, 1]
     * HEAD r5, r4         -> r5 = 2
     * TAIL r6, r4         -> r6 = [1 | nil]
     * HEAD r7, r6         -> r7 = 1
     * TAIL r8, r6         -> r8 = nil
     * HALT
     */
    /* Since we can't encode MB_NIL as CONST_I32, use the process API directly */
    mb_sched_init(&sched);
    pid = mb_sched_spawn(&sched, NULL, 0);
    p = mb_sched_proc(&sched, pid);

    /* Manually set up registers and run bytecode */
    {
        static const uint8_t prog2[] = {
            MB_OP_CONS, 2, 0, 1,     /* r2 = cons(r0, r1) = [1|nil] */
            MB_OP_CONS, 4, 3, 2,     /* r4 = cons(r3, r2) = [2, 1] */
            MB_OP_HEAD, 5, 4,         /* r5 = hd(r4) = 2 */
            MB_OP_TAIL, 6, 4,         /* r6 = tl(r4) = [1|nil] */
            MB_OP_HEAD, 7, 6,         /* r7 = hd(r6) = 1 */
            MB_OP_TAIL, 8, 6,         /* r8 = tl(r6) = nil */
            MB_OP_HALT
        };
        p->program = prog2;
        p->program_size = sizeof(prog2);
        p->pc = 0;
        p->state = MB_PROC_READY;
        p->regs[0] = MB_MAKE_SMALLINT(1);
        p->regs[1] = MB_NIL;
        p->regs[3] = MB_MAKE_SMALLINT(2);
    }

    while ((rc = mb_sched_tick(&sched)) == MB_OK) {}
    check_int("cons_idle", MB_SCHED_IDLE, rc);

    check_int("cons_halted", MB_PROC_HALTED, p->state);
    check_int("cons_r2_cons", 1, MB_IS_CONS(p->regs[2]));
    check_int("cons_r4_cons", 1, MB_IS_CONS(p->regs[4]));
    check_int("cons_head", 2, MB_GET_SMALLINT(p->regs[5]));
    check_int("cons_tail_cons", 1, MB_IS_CONS(p->regs[6]));
    check_int("cons_head2", 1, MB_GET_SMALLINT(p->regs[7]));
    check_int("cons_tail_nil", 1, p->regs[8] == MB_NIL);
}

int main(void) {
    /* Original vm-compat tests */
    test_invalid_command_rejected();
    test_bad_argument_rejected();
    test_mailbox_full();
    test_invalid_opcode();
    test_bad_register_decode();
    test_recv_empty_is_nonfatal();

    /* Scheduler tests */
    test_sched_spawn_and_halt();
    test_sched_full_table();
    test_sched_send_ext();
    test_sched_send_bad_pid();
    test_self_opcode();
    test_yield_opcode();
    test_two_process_round_robin();

    /* Term tagging tests */
    test_smallint_roundtrip();
    test_atom_roundtrip();
    test_pid_roundtrip();
    test_boxed_cons_roundtrip();
    test_tuple_header();

    /* Heap and GC tests */
    test_heap_alloc_basic();
    test_heap_make_tuple();
    test_heap_cons();
    test_gc_survives_live_data();
    test_gc_reclaims_dead_data();
    test_gc_nested_structures();

    /* Heap opcode tests (via VM bytecode) */
    test_opcode_make_tuple_and_elem();
    test_opcode_cons_head_tail();

    if (failures != 0) {
        fprintf(stderr, "regression failures=%d\n", failures);
        return 1;
    }

    printf("mini_beam_host_regression: all tests passed\n");
    return 0;
}
