/*
  +----------------------------------------------------------------------+
  | em                                                                   |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2025                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#define _GNU_SOURCE

#include <emscripten.h>
#include <emscripten/fiber.h>
#include <bits/alltypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#define EM_FIBER_ASYNC_STACK (64 * 1024)

typedef struct __ucontext {
    unsigned long uc_flags;
	struct __ucontext *uc_link;
    struct {
        void*  ss_sp;
        int    ss_flags;
        size_t ss_size;
    } uc_stack;

    emscripten_fiber_t fiber;
    void (*func)(void);
    struct {
        void* address;
        size_t size;
    } em_stack;
    uint32_t magic;
} ucontext_t;

static void em_fiber_enter(void* function) {
    void (*entry)(void)  =
        (void(*)(void)) function;
    entry();
}

int getcontext(ucontext_t *ucp) {
    ucp->em_stack.size =
        EM_FIBER_ASYNC_STACK;
    /* TODO(krakjoe) this leaks, fix it ... */
    ucp->em_stack.address =
        malloc(ucp->em_stack.size);
    emscripten_fiber_init_from_current_context(
        &ucp->fiber, ucp->em_stack.address, ucp->em_stack.size);
    ucp->magic = 0x12345678;

    return 0;
}

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...) {
    ucp->func = func;
    emscripten_fiber_init(
        &ucp->fiber,
        em_fiber_enter, func,
        ucp->uc_stack.ss_sp, ucp->uc_stack.ss_size,
        ucp->em_stack.address, ucp->em_stack.size);
}

int swapcontext(ucontext_t *oucp, ucontext_t *ucp) {
    if (oucp->magic != 0x12345678) {
        memset(oucp, 0, sizeof(ucontext_t));
        oucp->magic = 0x12345678;
        oucp->em_stack.size = EM_FIBER_ASYNC_STACK;
        oucp->em_stack.address = malloc(oucp->em_stack.size);
    }

    emscripten_fiber_init_from_current_context(
        &oucp->fiber, oucp->em_stack.address, oucp->em_stack.size);

    emscripten_fiber_swap(
        &oucp->fiber,
        &((ucontext_t*)ucp)->fiber);
    return 0;
}