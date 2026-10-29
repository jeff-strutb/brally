/* checkrun.h: the check mode's scheduler (checkrun.c), which thread.c's
 * calls hand over to when TGR_CHECK=1. */
#ifndef TGR_CHECKRUN_H
#define TGR_CHECKRUN_H
#include <stdint.h>
#include "tgr_os.h"

uint64_t chk_count(void);
uint32_t chk_frame(void);
void     chk_lock(void);
void     chk_unlock(void);
void     chk_event(int ev);
void     chk_post_event(int event);
void     chk_post_mesg(TgrMsgQueue *mq, TgrMsg msg);
void     chk_thread_new(TgrThread *t, TgrId id, void (*entry)(void *), void *arg, TgrPri pri);
void     chk_thread_start(TgrThread *t);
void     chk_thread_pri(TgrThread *t, TgrPri pri);
int32_t  chk_mq_send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag);
int32_t  chk_mq_jam(TgrMsgQueue *mq, TgrMsg msg, int32_t flag);
int32_t  chk_mq_recv(TgrMsgQueue *mq, TgrMsg *msg, int32_t flag);
void     chk_event_mq(TgrEvent e, TgrMsgQueue *mq, TgrMsg msg);
void     chk_retrace_mq(TgrMsgQueue *mq, TgrMsg msg, uint32_t retraceCount);
void     chk_timer(TgrTime countdown, TgrTime interval, TgrMsgQueue *mq, TgrMsg msg);
void     chk_start(void (*boot)(void));
int      chk_finished(void);
void     chk_wait(void);
#endif
