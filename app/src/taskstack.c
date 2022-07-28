#include "taskstack.h"

#include "gh.h"

int taskstack_init(taskstack_t *stack, int size) {
    INIT_LIST_HEAD(&stack->head);
    stack->stack_size = size;
    stack->depth = 0;
    return 0;
}

int taskstack_push(taskstack_t *stack, taskstack_task_t *task) {
    if (stack->depth >= stack->stack_size) return -1;
    taskstack_task_t *tmp = gh_malloc(sizeof(*task));
    memcpy(tmp, task, sizeof(*task));
    list_add_tail(&tmp->next, &stack->head);
    stack->depth++;
    return 0;
}

int taskstack_pop(taskstack_t *stack, taskstack_task_t *task) {
    if (stack->depth <= 0) return -1;
    taskstack_task_t *last = list_last_entry(&stack->head, taskstack_task_t, next);
    list_del(&last->next);
    memcpy(task, last, sizeof(*task));
    gh_free(last);
    stack->depth--;
    return 0;
}

int taskstack_pop_clean(taskstack_t *stack) {
    taskstack_task_t task;
    if (0 == taskstack_pop(stack, &task)) {
        if (task.cleanup) {
            task.cleanup(&task);
        }
        return 0;
    }
    return -1;
}

int taskstack_get_depth(taskstack_t *stack) {
    return stack->depth;
}

void taskstack_destroy(taskstack_t *stack) {
    if (!stack) return;
    //TODO
}
