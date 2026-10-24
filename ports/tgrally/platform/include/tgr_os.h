/* tgr_os.h: what the game asks of the machine (platform/os): its threads,
 * message queues, events, timers and clock, the picture, loads from the
 * cartridge, the controllers, Controller Pak and Rumble Pak, the drawing and
 * sound tasks and the audio out -- the calls it made to libultra on the
 * N64, under the port's own names.
 *
 * Every core TU is compiled with this header forced in, so a TU's own
 * declaration of a libultra function that disagrees with it is an error:
 * the decomp declared libultra's loosely (IDO's ILP32 made int, long and
 * pointers one type), and natively each one has to be right.
 *
 * The objects the game allocates (threads, message queues, timers, load
 * requests, the Controller Pak's handle) live in game memory at their
 * original addresses, so they keep the layout libultra gave them: an address in
 * them is a 4-byte original address (TgrAddr) and a message is a 32-bit
 * value (an integer, or an original address), as on the N64.  The platform
 * reads and writes them where libultra did. */
#ifndef TGR_OS_H
#define TGR_OS_H
#include <stdint.h>

typedef uint32_t TgrAddr;           /* an original address (tgr_addr.h) */

typedef int64_t  TgrTime;
typedef int32_t  TgrPri;
typedef int32_t  TgrId;
typedef uint32_t TgrEvent;
typedef uint32_t TgrMsg;            /* an integer or an original address */

typedef struct TgrThread_s {         /* 0x1B0 bytes; the platform keeps its own state */
    TgrAddr next;
    TgrPri priority;
    char context[0x1B0 - 8];
} TgrThread;

typedef struct TgrMsgQueue_s {      /* 0x18 bytes */
    TgrAddr mtqueue;
    TgrAddr fullqueue;
    int32_t validCount;
    int32_t first;
    int32_t msgCount;
    TgrAddr msg;                    /* OSMesg[msgCount] */
} TgrMsgQueue;

typedef struct {
    uint16_t type;
    uint8_t  pri;
    uint8_t  status;
    TgrAddr  retQueue;              /* OSMesgQueue */
} TgrIoMsgHdr;

typedef struct {                    /* 0x18 bytes */
    TgrIoMsgHdr hdr;
    TgrAddr  dramAddr;
    uint32_t devAddr;
    uint32_t size;
    TgrAddr  piHandle;
} TgrIoMsg;

typedef struct TgrTimer_s {          /* 0x20 bytes */
    TgrAddr next, prev;
    TgrTime interval;
    TgrTime value;
    TgrAddr mq;
    TgrMsg msg;
} TgrTimer;

typedef struct {                    /* 0x40 bytes; the addresses are original ones */
    uint32_t type;
    uint32_t flags;
    TgrAddr  ucode_boot;
    uint32_t ucode_boot_size;
    TgrAddr  ucode;
    uint32_t ucode_size;
    TgrAddr  ucode_data;
    uint32_t ucode_data_size;
    TgrAddr  dram_stack;
    uint32_t dram_stack_size;
    TgrAddr  output_buff;
    TgrAddr  output_buff_size;
    TgrAddr  data_ptr;
    uint32_t data_size;
    TgrAddr  yield_data_ptr;
    uint32_t yield_data_size;
} TgrTaskDesc;

typedef union {
    TgrTaskDesc t;
    long long force_structure_alignment;
} TgrTask;

typedef struct {
    uint16_t button;
    int8_t   stick_x;
    int8_t   stick_y;
    uint8_t  errnum;
} TgrPad;

typedef struct {
    uint16_t type;
    uint8_t  status;
    uint8_t  errnum;
} TgrPadStatus;

typedef struct {                    /* 0x68 bytes */
    int32_t status;
    TgrAddr queue;                  /* OSMesgQueue */
    int32_t channel;
    uint8_t id[32];
    uint8_t label[32];
    int32_t version;
    int32_t dir_size;
    int32_t inode_table;
    int32_t minode_table;
    int32_t dir_table;
    int32_t inode_start_page;
    uint8_t banks;
    uint8_t activebank;
    uint8_t pad66[2];
} TgrPak;

typedef struct {                    /* 0x20 bytes */
    uint32_t file_size;
    uint32_t game_code;
    uint16_t company_code;
    char     ext_name[4];
    char     game_name[16];
    char     pad1e[2];
} TgrPakFile;


#define TGR_NOBLOCK     0
#define TGR_BLOCK       1
#define TGR_READ             0
#define TGR_WRITE            1
#define TGR_EVENT_SP         4
#define TGR_EVENT_SI         5
#define TGR_EVENT_AI         6
#define TGR_EVENT_VI         7
#define TGR_EVENT_PI         8
#define TGR_EVENT_DP         9
#define TGR_EVENT_FAULT      12
#define TGR_TASK_GFX           1
#define TGR_TASK_AUDIO           2

#define TGR_PAK_NONE      1
#define TGR_PAK_INVALID     5
#define TGR_PAK_FULL       8
#define TGR_PAK_ID_FATAL    10

#ifdef __cplusplus
extern "C" {
#endif
/* the game's view of the machine */
extern int32_t  tgr_tv_type;
extern uint32_t tgr_mem_size;
extern uint64_t tgr_clock_rate;
extern uint32_t tgr_rom_base;
extern int32_t  tgr_reset_type;

void     tgr_os_init(void);
void     tgr_thread_new(TgrThread *t, TgrId id, void (*entry)(void *), void *arg, void *sp, TgrPri pri);
void     tgr_thread_start(TgrThread *t);
void     tgr_thread_pri(TgrThread *t, TgrPri pri);
void     tgr_thread_idle(void);              /* the idle thread rests: nothing to spin for */
void     tgr_mq_init(TgrMsgQueue *mq, TgrMsg *msg, int32_t count);
int32_t  tgr_mq_send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag);
int32_t  tgr_mq_jam(TgrMsgQueue *mq, TgrMsg msg, int32_t flag);
int32_t  tgr_mq_recv(TgrMsgQueue *mq, TgrMsg *msg, int32_t flag);
void     tgr_event_mq(TgrEvent e, TgrMsgQueue *mq, TgrMsg msg);
uint32_t tgr_clock(void);
int32_t  tgr_timer(TgrTimer *t, TgrTime countdown, TgrTime interval, TgrMsgQueue *mq, TgrMsg msg);

void     tgr_video_reset(void);               /* a change of resolution: anti-aliased, divot, gamma, no dither filter */
void     tgr_retrace_mq(TgrMsgQueue *mq, TgrMsg msg, uint32_t retraceCount);
void     tgr_video_dither_filter(int on);
void     tgr_video_divot(int on);
void     tgr_video_gamma(int on);
void     tgr_video_show(void *fb);
void    *tgr_video_shown(void);

int32_t  tgr_rom_load(TgrIoMsg *mb, int32_t pri, int32_t direction, uint32_t devAddr, void *vAddr,
                      uint32_t nbytes, TgrMsgQueue *mq);
int32_t  tgr_rom_word(uint32_t devAddr, uint32_t *data);


int32_t  tgr_pad_init(TgrMsgQueue *mq, uint8_t *bitpattern, TgrPadStatus *status);
int32_t  tgr_pad_read(TgrMsgQueue *mq);
void     tgr_pad_get(TgrPad *pad);

int32_t  tgr_pak_present(TgrMsgQueue *mq, uint8_t *pattern);
int32_t  tgr_pak_open(TgrMsgQueue *mq, TgrPak *pfs, int32_t channel);
int32_t  tgr_pak_open_new(TgrMsgQueue *mq, TgrPak *pfs, int32_t channel);
int32_t  tgr_pak_repair(TgrPak *pfs);
int32_t  tgr_pak_check(TgrPak *pfs);
int32_t  tgr_pak_find(TgrPak *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                       int32_t *file_no);
int32_t  tgr_pak_create(TgrPak *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                           int32_t length, int32_t *file_no);
int32_t  tgr_pak_delete(TgrPak *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext);
int32_t  tgr_pak_io(TgrPak *pfs, int32_t file_no, uint8_t flag, int32_t offset, int32_t size,
                            uint8_t *data);
int32_t  tgr_pak_free(TgrPak *pfs, int32_t *bytes);
int32_t  tgr_pak_count(TgrPak *pfs, int32_t *max_files, int32_t *files_used);
int32_t  tgr_pak_stat(TgrPak *pfs, int32_t file_no, TgrPakFile *state);
int32_t  tgr_rumble_open(TgrMsgQueue *mq, TgrPak *pfs, int32_t channel);
int32_t  tgr_rumble_on(TgrPak *pfs);
int32_t  tgr_rumble_off(TgrPak *pfs);

void     tgr_task_load(TgrTask *task);
void     tgr_task_run(TgrTask *task);

/* the sound out (platform/audio/out.c): 16-bit stereo, big-endian, at the
 * rate the game sets; the game mixes into a ring of its own and hands over
 * each stretch it mixed */
int32_t  tgr_audio_rate(uint32_t frequency);  /* the rate the game mixes at (the rate given) */
void     tgr_audio_start(void);               /* the sound starts now */
uint32_t tgr_audio_want(void);                /* bytes to mix now: what keeps the output fed, a multiple of 8 */
void     tgr_audio_submit(const void *ring, uint32_t size, uint32_t pos, uint32_t n);  /* n bytes from pos, round the ring */

void     tgr_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
TgrAddr  tgr_faulted_thread(void);

/* the libm member the game links (gu's sinf/cosf are core C) */
int16_t  sins(uint16_t angle);
int16_t  coss(uint16_t angle);
#ifdef __cplusplus
}
#endif
#endif
