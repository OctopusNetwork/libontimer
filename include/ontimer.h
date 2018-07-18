#ifndef __ONC_TIMER____H__
#define __ONC_TIMER____H__

#define ONC_MSG_TIMER_BASE          0x80000000
#define ONC_MSG_TIMER_EXPIRE        0x80000001

typedef struct onc_timerarg_s {
    void    (*timer_func)(struct onc_timerarg_s *arg);
    void    *arg;
    void    *timer_handle;
    void    *timer;
} onc_timerarg_s_t;

#ifdef __cplusplus
extern "C" {
#endif

void   *onc_timer_init(int internal_sched);
void   *onc_timer_create(void *timer_handle,
            int listener, int repeat, int timeout,
            onc_timerarg_s_t *timer_arg);
void    onc_timer_destroy(void *timer_handle, void *timer);
int     onc_timer_repeat(void *timer_handle, void *timer);
void    onc_timer_final(void *timer_handle);

#ifdef __cplusplus
}
#endif

#endif
