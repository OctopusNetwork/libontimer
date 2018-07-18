#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "on_msgagent.h"
#include "on_malloc.h"
#include "on_time.h"

#include "onclfds.h"
#include "onevent.h"

#include "ontimer.h"

static int g_running = 0;
static int g_msgbuf_pid;
static void *g_event = NULL;

static void __sigint_handler(int sig)
{
    g_running = 0;
    onc_event_wakeup(g_event);
}

static int __msg_feeder(onc_msg_s_t *msg)
{
    onc_timerarg_s_t *arg = (onc_timerarg_s_t *)msg->arg;
    if (g_msgbuf_pid == msg->receiver_pid) {
        onc_msg_agent_feedmsg_myself(g_msgbuf_pid, msg);
        printf("<%lu>Feed msg %x to myself value %lu %p\n",
                onc_time_now(1),
                msg->msg, (unsigned long)arg->arg,
                arg->timer);
        onc_event_wakeup(g_event);
    } else {
        printf("Not my msg\n");
    }
    return 0;
}

static void __timer_func(onc_timerarg_s_t *arg)
{
    printf("Timer func processed\n");
    if (!onc_timer_repeat(arg->timer_handle, arg->timer)) {
        onc_timer_destroy(arg->timer_handle, arg->timer);
        onc_free(arg);
    }
}

int main(int argc, char *argv[])
{
    void *repeat_timer = NULL;
    void *timer_handle = NULL;
    onc_msg_s_t msg;
    onc_timerarg_s_t *timer_arg;
    onc_timerarg_s_t *resp_timer_arg;
    int rc = 0;
    void *lfds = NULL;

    if (onc_msg_agent_init(2, 1) < 0) {
        return -1;
    }

    timer_handle = onc_timer_init(1);
    if (NULL == timer_handle) {
        onc_msg_agent_final();
        return -1;
    }

    g_event = onc_event_create(1,
            ONC_EVENT_READ | ONC_EVENT_ERROR,
            1, 0, 0);
    if (NULL == g_event) {
        onc_msg_agent_final();
        onc_timer_final(timer_handle);
        return -1;
    }

    g_msgbuf_pid = onc_msg_agent_create_bidirect_buf(
            0x98765432, 20, 20, __msg_feeder);
    if (g_msgbuf_pid < 0) {
        onc_event_destroy(g_event);
        onc_msg_agent_final();
        onc_timer_final(timer_handle);
        return -1;
    }

    lfds = onc_lfds_new();
    onc_msg_agent_start();

    g_running = 1;
    signal(SIGINT, __sigint_handler);

    timer_arg = onc_malloc(sizeof(onc_timerarg_s_t));
    timer_arg->arg = (void *)2000;
    timer_arg->timer_func = __timer_func;
    repeat_timer = onc_timer_create(timer_handle, g_msgbuf_pid,
            1, 1000, timer_arg);
    do {
        timer_arg = onc_malloc(sizeof(onc_timerarg_s_t));
        timer_arg->arg = (void *)1000;
        timer_arg->timer_func = __timer_func;
        onc_timer_create(timer_handle, g_msgbuf_pid,
               0, 1000, timer_arg);

        rc = onc_event_wait(g_event, lfds, 1000);
        if (rc < 0) {
        } else if (0 < rc) {
            while (0 == onc_msg_agent_recvmsg(g_msgbuf_pid, &msg)) {
                resp_timer_arg = (onc_timerarg_s_t *)msg.arg;
                switch (msg.msg) {
                    case ONC_MSG_TIMER_EXPIRE:
                        resp_timer_arg->timer_func(resp_timer_arg);
                        break;
                    default:
                        printf("Unknown message\n");
                        break;
                }
            }
            printf("Message process done---\n");
        } else {
            printf("Timeout---\n");
        }
        printf("<%lu>Create timer---\n", onc_time_now(1));
    } while (1 == g_running);

    onc_timer_destroy(timer_handle, repeat_timer);

    onc_msg_agent_stop();
    onc_lfds_del(lfds);
    onc_msg_agent_destroy_bidirect_buf(g_msgbuf_pid);
    onc_event_destroy(g_event);
    onc_timer_final(timer_handle);
    onc_msg_agent_final();

    return 0;
}
