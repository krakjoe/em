/*
  +----------------------------------------------------------------------+
  | ort                                                                  |
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

#include <sys/types.h>
#include <stdlib.h>

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_RMID    0
#define IPC_SET     1
#define IPC_STAT    2
#define SHM_R       0400
#define SHM_W       0200

int shmget(int key, size_t size, int shmflg) {
    return 1;
}

void* shmat(int shmid, const void *shmaddr, int shmflg) { 
    static void* shm = NULL;
    if (!shm)
        shm = malloc(64 * 1024 * 1024);
    return shm; 
}

int shmdt(const void *shmaddr) {
    return 0; 
}

int shmctl(int shmid, int cmd, void *buf) {
    return 0; 
}