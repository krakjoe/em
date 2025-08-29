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

#include <emscripten.h>
#include <emscripten/fiber.h>
#include <bits/alltypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define EM_UCONTEXT_ARENA_ALIGN 8
#define EM_UCONTEXT_ARENA_SIZE  64 * 32 * 1024
#define EM_UCONTEXT_STACK_SIZE (32 * 1024)
#define EM_UCONTEXT_MAGIC       0x12345678

#define EM_UCONTEXT_OK     0
#define EM_UCONTEXT_ERROR -1

typedef struct __ucontext {
    /* don't touch, used by zend */
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
        void*  address;
        size_t size;
    } em_stack;
    uint32_t magic;
} ucontext_t;

typedef struct _em_arena_t em_arena_t;

extern em_arena_t* em_arena_create(size_t size, size_t alignment);
extern void* em_arena_alloc(em_arena_t* arena, size_t size);
extern void em_arena_reset(em_arena_t* arena);
extern void em_arena_destroy(em_arena_t* arena);

static em_arena_t* __em_ucontext_arena__;

void em_ucontext_startup(void) {
    __em_ucontext_arena__ =
        em_arena_create(
            EM_UCONTEXT_ARENA_SIZE,
            EM_UCONTEXT_ARENA_ALIGN);
}

void em_ucontext_activate(void) {
    em_arena_reset(
        __em_ucontext_arena__);
}

void em_ucontext_deactivate(void) {
    em_arena_reset(
        __em_ucontext_arena__);
}

void em_ucontext_shutdown(void) {
    em_arena_destroy(
        __em_ucontext_arena__);
}

static void em_ucontext_enter(void* function) {
    void (*entry)(void)  =
        (void(*)(void)) function;
    entry();
}

int getcontext(ucontext_t *ucp) {
    ucp->em_stack.size =
        EM_UCONTEXT_STACK_SIZE;
    ucp->em_stack.address =
        em_arena_alloc(
            __em_ucontext_arena__,
            ucp->em_stack.size);

    if (!ucp->em_stack.address) {
        errno =
            ENOMEM;
        return EM_UCONTEXT_ERROR;
    }

    emscripten_fiber_init_from_current_context(
        &ucp->fiber, ucp->em_stack.address, ucp->em_stack.size);
    ucp->magic = EM_UCONTEXT_MAGIC;

    return EM_UCONTEXT_OK;
}

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...) {
    ucp->func = func;
    emscripten_fiber_init(
        &ucp->fiber,
        em_ucontext_enter, func,
        ucp->uc_stack.ss_sp, ucp->uc_stack.ss_size,
        ucp->em_stack.address, ucp->em_stack.size);
}

int swapcontext(ucontext_t *oucp, ucontext_t *ucp) {
    if (oucp->magic != EM_UCONTEXT_MAGIC) {
        memset(oucp, 0, sizeof(ucontext_t));
        oucp->magic = EM_UCONTEXT_MAGIC;
        oucp->em_stack.size = EM_UCONTEXT_STACK_SIZE;
        oucp->em_stack.address = em_arena_alloc(
            __em_ucontext_arena__,
            oucp->em_stack.size);
        if (!oucp->em_stack.address) {
            errno =
                ENOMEM;
            return EM_UCONTEXT_ERROR;
        }
    }

    emscripten_fiber_init_from_current_context(
        &oucp->fiber, oucp->em_stack.address, oucp->em_stack.size);

    emscripten_fiber_swap(
        &oucp->fiber, &ucp->fiber);
    return EM_UCONTEXT_OK;
}