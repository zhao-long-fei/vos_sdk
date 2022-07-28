#ifndef TASKSTACK_H
#define TASKSTACK_H
#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

typedef struct {
    struct list_head head;
    int stack_size;
    int depth;
} taskstack_t;

struct taskstack_task {
    struct list_head next;
    int (*do_job)(struct taskstack_task *task);
    void (*cleanup)(struct taskstack_task *task;);
    int argc;
    char **argv;
};

typedef struct taskstack_task taskstack_task_t;

int taskstack_init(taskstack_t *stack, int size);
int taskstack_push(taskstack_t *stack, taskstack_task_t *task);
int taskstack_pop(taskstack_t *stack, taskstack_task_t *task);
int taskstack_get_depth(taskstack_t *stack);
int taskstack_pop_clean(taskstack_t *stack);
void taskstack_destroy(taskstack_t *stack);

#ifdef __cplusplus
}
#endif
#endif
