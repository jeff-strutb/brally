/* ultra64.h: the part of libultra the game calls, as the platform layer
 * implements it natively (platform/os).
 *
 * Every core TU is compiled with this header forced in, so a TU's own
 * declaration of a libultra function that disagrees with it is an error:
 * the decomp declared these loosely (IDO's ILP32 made int, long and pointers
 * one type), and natively each one has to be right.
 *
 * The OS objects the game allocates (threads, message queues, timers, I/O
 * messages, the Controller Pak's handle) live in game memory at their
 * original addresses, so they keep libultra's own layout: an address in
 * them is a 4-byte original address (TgrAddr) and a message is a 32-bit
 * value (an integer, or an original address), as on the N64.  The platform
 * reads and writes them where libultra did. */
#ifndef TGR_ULTRA64_H
#define TGR_ULTRA64_H
#include <stdint.h>

typedef uint32_t TgrAddr;           /* an original address (tgr_addr.h) */

typedef int64_t  OSTime;
typedef int32_t  OSPri;
typedef int32_t  OSId;
typedef uint32_t OSEvent;
typedef uint32_t OSMesg;            /* an integer or an original address */

typedef struct OSThread_s {         /* 0x1B0 bytes; the platform keeps its own state */
    TgrAddr next;
    OSPri priority;
    char context[0x1B0 - 8];
} OSThread;

typedef struct OSMesgQueue_s {      /* 0x18 bytes */
    TgrAddr mtqueue;
    TgrAddr fullqueue;
    int32_t validCount;
    int32_t first;
    int32_t msgCount;
    TgrAddr msg;                    /* OSMesg[msgCount] */
} OSMesgQueue;

typedef struct {
    uint16_t type;
    uint8_t  pri;
    uint8_t  status;
    TgrAddr  retQueue;              /* OSMesgQueue */
} OSIoMesgHdr;

typedef struct {                    /* 0x18 bytes */
    OSIoMesgHdr hdr;
    TgrAddr  dramAddr;
    uint32_t devAddr;
    uint32_t size;
    TgrAddr  piHandle;
} OSIoMesg;

typedef struct OSTimer_s {          /* 0x20 bytes */
    TgrAddr next, prev;
    OSTime interval;
    OSTime value;
    TgrAddr mq;
    OSMesg msg;
} OSTimer;

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
} OSTask_t;

typedef union {
    OSTask_t t;
    long long force_structure_alignment;
} OSTask;

typedef struct {
    uint16_t button;
    int8_t   stick_x;
    int8_t   stick_y;
    uint8_t  errnum;
} OSContPad;

typedef struct {
    uint16_t type;
    uint8_t  status;
    uint8_t  errnum;
} OSContStatus;

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
} OSPfs;

typedef struct {                    /* 0x20 bytes */
    uint32_t file_size;
    uint32_t game_code;
    uint16_t company_code;
    char     ext_name[4];
    char     game_name[16];
    char     pad1e[2];
} OSPfsState;

typedef struct { uint8_t type; uint8_t pad[3]; uint32_t regs[19]; } OSViMode;   /* 0x50 bytes */

#define OS_MESG_NOBLOCK     0
#define OS_MESG_BLOCK       1
#define OS_READ             0
#define OS_WRITE            1
#define OS_EVENT_SP         4
#define OS_EVENT_SI         5
#define OS_EVENT_AI         6
#define OS_EVENT_VI         7
#define OS_EVENT_PI         8
#define OS_EVENT_DP         9
#define OS_EVENT_FAULT      12
#define M_GFXTASK           1
#define M_AUDTASK           2

#define PFS_ERR_NOPACK      1
#define PFS_ERR_INVALID     5
#define PFS_DATA_FULL       8
#define PFS_ERR_ID_FATAL    10

#ifdef __cplusplus
extern "C" {
#endif
/* the game's view of the machine */
extern int32_t  osTvType;
extern uint32_t osMemSize;
extern uint64_t osClockRate;
extern uint32_t osRomBase;
extern int32_t  osResetType;

void     osInitialize(void);
void     osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri);
void     osStartThread(OSThread *t);
void     osSetThreadPri(OSThread *t, OSPri pri);
void     osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int32_t count);
int32_t  osSendMesg(OSMesgQueue *mq, OSMesg msg, int32_t flag);
int32_t  osJamMesg(OSMesgQueue *mq, OSMesg msg, int32_t flag);
int32_t  osRecvMesg(OSMesgQueue *mq, OSMesg *msg, int32_t flag);
void     osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg msg);
uint32_t osGetCount(void);
int32_t  osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg);

void     osCreateViManager(OSPri pri);
void     osViSetMode(OSViMode *mode);
void     osViSetEvent(OSMesgQueue *mq, OSMesg msg, uint32_t retraceCount);
void     osViSetSpecialFeatures(uint32_t func);
void     osViBlack(uint8_t active);
void     osViSwapBuffer(void *fb);
void    *osViGetCurrentFramebuffer(void);

void     osCreatePiManager(OSPri pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, int32_t cmdMsgCnt);
int32_t  osPiStartDma(OSIoMesg *mb, int32_t pri, int32_t direction, uint32_t devAddr, void *vAddr,
                      uint32_t nbytes, OSMesgQueue *mq);
int32_t  osPiReadIo(uint32_t devAddr, uint32_t *data);

void     osInvalDCache(void *vaddr, int32_t nbytes);
void     osWritebackDCacheAll(void);

int32_t  osContInit(OSMesgQueue *mq, uint8_t *bitpattern, OSContStatus *status);
int32_t  osContStartReadData(OSMesgQueue *mq);
void     osContGetReadData(OSContPad *pad);

int32_t  osPfsIsPlug(OSMesgQueue *mq, uint8_t *pattern);
int32_t  osPfsInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel);
int32_t  osPfsInitPak(OSMesgQueue *mq, OSPfs *pfs, int32_t channel);
int32_t  osPfsRepairId(OSPfs *pfs);
int32_t  osPfsChecker(OSPfs *pfs);
int32_t  osPfsFindFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                       int32_t *file_no);
int32_t  osPfsAllocateFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext,
                           int32_t length, int32_t *file_no);
int32_t  osPfsDeleteFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext);
int32_t  osPfsReadWriteFile(OSPfs *pfs, int32_t file_no, uint8_t flag, int32_t offset, int32_t size,
                            uint8_t *data);
int32_t  osPfsFreeBlocks(OSPfs *pfs, int32_t *bytes);
int32_t  osPfsNumFiles(OSPfs *pfs, int32_t *max_files, int32_t *files_used);
int32_t  osPfsFileState(OSPfs *pfs, int32_t file_no, OSPfsState *state);
int32_t  osMotorInit(OSMesgQueue *mq, OSPfs *pfs, int32_t channel);
int32_t  osMotorStart(OSPfs *pfs);
int32_t  osMotorStop(OSPfs *pfs);

void     osSpTaskLoad(OSTask *task);
void     osSpTaskStartGo(OSTask *task);

int32_t  osAiSetFrequency(uint32_t frequency);
int32_t  osAiSetNextBuffer(void *buf, uint32_t size);
uint32_t osAiGetStatus(void);
uint32_t osAiGetLength(void);

void     osSyncPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
TgrAddr  __osGetCurrFaultedThread(void);

/* the libm member the game links (gu's sinf/cosf are core C) */
int16_t  sins(uint16_t angle);
int16_t  coss(uint16_t angle);
#ifdef __cplusplus
}
#endif
#endif
