#include <stdlib.h>

#include "on_thread.h"
#include "on_malloc.h"
#include "on_time.h"

#include "onlfds.h"
#include "onevent.h"
#include "rbtree_api.h"
#include "on_msgagent.h"

#include "ontimer.h"

#define ONC_TIMER_MSG_COUNT     20000
#define ONC_DEFAULT_WAIT        500

typedef struct {
    void           *event;
    void           *sched_thread;
    int             msgbuf_pid;
    int             running;

    void           *rb_mutex;
    struct rb_root  root;
} onc_timer_handle_s_t;

typedef struct {
    int             listener;
    uint32_t        interval;
    uint64_t        expire;
    unsigned char   repeat;
    struct rb_node  rb;
    unsigned char   rb_erased;
    /* For easy convertion, timer never use it */
    void           *timer_arg;
} onc_timer_s_t;

static rbtree_cmpresult_e_t __compare(
        struct rb_node *n1, struct rb_node *n2)
{
    onc_timer_s_t *timer1 = rb_entry(n1, onc_timer_s_t, rb);
    onc_timer_s_t *timer2 = rb_entry(n2, onc_timer_s_t, rb);

    if (timer1->expire < timer2->expire) {
        return N_CMP_LOWER;
    } else {
        return N_CMP_LARGER;
    }

    return N_CMP_LOWER;
}

static uint32_t __process_timer(onc_timer_handle_s_t *timer_handle)
{
    uint64_t now = 0;
    struct rb_node *rb = NULL;
    struct rb_root *root = &timer_handle->root;
    uint32_t next_wait = ONC_DEFAULT_WAIT;

    now = onc_time_now(1);

    onc_mutex_lock(timer_handle->rb_mutex);
    for (rb = rb_first(root); rb; rb = rb_next(rb)) {
        onc_timer_s_t *timer = rb_entry(rb, onc_timer_s_t, rb);
        if (timer->expire <= now) {
            /* Send timer tick message to dispatcher */
            if (onc_msg_agent_sendmsg_toother(
                        timer_handle->msgbuf_pid,
                        timer->listener, ONC_MSG_TIMER_EXPIRE,
                        timer->timer_arg, N_MSG_PRIORITY_HIGH,
                        1, 0) < 0) {
                /* Cannot write any more */
                next_wait = 1;
                break;
            }
        } else {
            next_wait = (uint32_t)(timer->expire - now);
            break;
        }

        rbtree_erase(root, rb);
        if (0 == timer->repeat) {
            /* Do nothing: Let user to free it */
            timer->rb_erased = 1;
        } else {
            timer->expire += timer->interval;
            rbtree_insert(root, &timer->rb, __compare);
        }
    }
    onc_mutex_unlock(timer_handle->rb_mutex);

    return next_wait;
}

static void *__schedule(void *arg)
{
    onc_timer_handle_s_t *timer_handle =
        (onc_timer_handle_s_t *)arg;
    uint32_t wait = ONC_DEFAULT_WAIT;
    int rc = 0;
    void *lfds = onc_lfds_new();

    if (NULL == lfds) {
        return NULL;
    }

    do {
        rc = onc_event_wait(timer_handle->event, lfds, wait);
        if (rc < 0) {

        /* For wakeup case and timeout case, process timer */
        } else {
            wait = __process_timer(timer_handle);

        }
    } while (1 == timer_handle->running);

    onc_lfds_del(lfds);

    return NULL;
}

static int __msg_feeder(onc_msg_s_t *msg)
{
    return -1;
}

void *onc_timer_init(int internal_sched)
{
    onc_timer_handle_s_t *timer_handle =
        onc_malloc(sizeof(onc_timer_handle_s_t));

    if (NULL == timer_handle) {
        return NULL;
    }

    timer_handle->event = onc_event_create(1,
            ONC_EVENT_READ | ONC_EVENT_ERROR, 1, 0, 0);
    if (NULL == timer_handle->event) {
        goto L_ERROR_EVENT_CREATE;
    }

    timer_handle->rb_mutex = onc_mutex_init();
    if (NULL == timer_handle->rb_mutex) {
        goto L_ERROR_RBMUTEX_INIT;
    }

    timer_handle->msgbuf_pid =
        onc_msg_agent_create_bidirect_buf(
                (unsigned long)timer_handle->sched_thread,
                ONC_TIMER_MSG_COUNT,
                ONC_TIMER_MSG_COUNT,
                __msg_feeder);
    if (timer_handle->msgbuf_pid < 0) {
        goto L_ERROR_MSGBUF_CREATE;
    }

    timer_handle->root = RB_ROOT;

    timer_handle->running = 1;
    timer_handle->sched_thread =
        onc_thread_create(__schedule, timer_handle);
    if (NULL == timer_handle->sched_thread) {
        goto L_ERROR_SCHEDTHREAD_CREATE;
    }

    return (void *)timer_handle;

L_ERROR_SCHEDTHREAD_CREATE:
    onc_msg_agent_destroy_bidirect_buf(timer_handle->msgbuf_pid);
L_ERROR_MSGBUF_CREATE:
    onc_mutex_destroy(timer_handle->rb_mutex);
L_ERROR_RBMUTEX_INIT:
    onc_event_destroy(timer_handle->event);
L_ERROR_EVENT_CREATE:
    onc_free(timer_handle);
    return NULL;
}

int onc_timer_repeat(void *timer_handle, void *timer)
{
    onc_timer_s_t *onc_timer = (onc_timer_s_t *)timer;
    return (1 == onc_timer->repeat);
}

void *onc_timer_create(void *timer_handle,
        int listener, int repeat, int timeout,
        onc_timerarg_s_t *timer_arg)
{
    onc_timer_handle_s_t *handle =
        (onc_timer_handle_s_t *)timer_handle;
    onc_timer_s_t *timer = NULL;

    timer = onc_malloc(sizeof(onc_timer_s_t));
    if (NULL == timer) {
        return NULL;
    }

    timer_arg->timer = timer;
    timer_arg->timer_handle = handle;

    timer->listener = listener;
    timer->interval = timeout;
    timer->repeat = repeat;
    timer->expire = onc_time_now(1) + timeout;
    timer->timer_arg = timer_arg;
    timer->rb_erased = 0;

    onc_mutex_lock(handle->rb_mutex);
    rbtree_insert(&handle->root, &timer->rb, __compare);
    onc_mutex_unlock(handle->rb_mutex);

    onc_event_wakeup(handle->event);
    return timer;
}

void onc_timer_destroy(void *timer_handle, void *timer)
{
    onc_timer_handle_s_t *handle =
        (onc_timer_handle_s_t *)timer_handle;
    onc_timer_s_t *onc_timer = (onc_timer_s_t *)timer;

    if (0 == onc_timer->rb_erased) {
        onc_mutex_lock(handle->rb_mutex);
        rbtree_erase(&handle->root, &onc_timer->rb);
        onc_mutex_unlock(handle->rb_mutex);
    }

    onc_free(timer);
    onc_event_wakeup(handle->event);
}

void onc_timer_final(void *timer_handle)
{
    onc_timer_handle_s_t *handle =
        (onc_timer_handle_s_t *)timer_handle;

    handle->running = 0;
    onc_event_wakeup(handle->event);
    onc_thread_join(handle->sched_thread);

    onc_msg_agent_destroy_bidirect_buf(handle->msgbuf_pid);
    onc_mutex_destroy(handle->rb_mutex);
    onc_event_destroy(handle->event);
    onc_free(timer_handle);
}
