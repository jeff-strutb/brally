/* n64-cflags: -O1 */
/* devmgr.c -- libultra's device manager thread (io/devmgr.c).
 */
#include "tgr/ultra.h"

/* -- declarations -- */
#define DEVICE_TYPE_64DD 2
#define LEO_CMD_TYPE_0 0
#define LEO_CMD_TYPE_1 1
#define LEO_SECTOR_MODE 3
#define LEO_TRACK_MODE 2
#define LEO_BM_CTL 0x05000510
#define LEO_STATUS 0x05000508
#define LEO_BM_CTL_RESET 0x10000000
#define LEO_BM_CTL_CLR_MECHANIC_INTR 0x01000000
#define LEO_STATUS_MECHANIC_INTERRUPT 0x02000000
#define LEO_ERROR_GOOD 0
#define LEO_ERROR_4 4
#define LEO_ERROR_29 29
#define OS_IM_PI 0x00100401
#define SR_IBIT4 0x00000800
#define PI_CLR_INTR 0x02
#define OS_MESG_TYPE_LOOPBACK 10
#define OS_MESG_TYPE_EDMAREAD 15
#define OS_MESG_TYPE_EDMAWRITE 16
void __osResetGlobalIntMask(OSHWIntr mask);
void __osSetGlobalIntMask(OSHWIntr mask);
s32 __osEPiRawWriteIo(OSPiHandle *pihandle, u32 devAddr, u32 data);
s32 __osEPiRawReadIo(OSPiHandle *pihandle, u32 devAddr, u32 *data);
void osYieldThread(void);
/* -- end declarations -- */

/* WHAT IT DOES: The PI (and VI-style) device manager thread: take each
 * request from the command queue; a 64DD block transfer is started through
 * the drive's buffer manager and its completion (error 29 recovered as 4)
 * reported on the request's queue; a cartridge DMA, read or write, plain
 * or through a PI handle, runs under the access lock and is reported when
 * its PI interrupt arrives; a loopback is sent straight back. */
/* @implements 0x8026C990 tgr __osDevMgrMain */
void __osDevMgrMain(void *args)
{
	OSIoMesg *mb;
	OSMesg em;
	OSMesg dummy;
	s32 ret;
	OSDevMgr *dm;
	s32 messageSend;

	messageSend = 0;
	mb = NULL;
	ret = 0;
	dm = (OSDevMgr *)args;
	while (TRUE) {
		osRecvMesg(dm->cmdQueue, (OSMesg *)&mb, OS_MESG_BLOCK);
		if (mb->piHandle != NULL && mb->piHandle->type == DEVICE_TYPE_64DD &&
		    (mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0 ||
		     mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_1)) {
			__OSBlockInfo *blockInfo;
			__OSTranxInfo *info;

			info = &mb->piHandle->transferInfo;
			blockInfo = &info->block[info->blockNum];
			info->sectorNum = -1;
			if (info->transferMode != LEO_SECTOR_MODE) {
				blockInfo->dramAddr = (void *)((u32)blockInfo->dramAddr - blockInfo->sectorSize);
			}
			if (info->transferMode == LEO_TRACK_MODE && mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0) {
				messageSend = 1;
			} else {
				messageSend = 0;
			}
			osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
			__osResetGlobalIntMask(OS_IM_PI);
			__osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, (info->bmCtlShadow | 0x80000000));
			while (TRUE) {
				osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
				info = &mb->piHandle->transferInfo;
				blockInfo = &info->block[info->blockNum];
				if (blockInfo->errStatus == LEO_ERROR_29) {
					u32 stat;

					__osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow | LEO_BM_CTL_RESET);
					__osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow);
					__osEPiRawReadIo(mb->piHandle, LEO_STATUS, &stat);
					if (stat & LEO_STATUS_MECHANIC_INTERRUPT) {
						__osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow | LEO_BM_CTL_CLR_MECHANIC_INTR);
					}
					blockInfo->errStatus = LEO_ERROR_4;
					IO_WRITE(PI_STATUS_REG, PI_CLR_INTR);
					__osSetGlobalIntMask(OS_IM_PI | SR_IBIT4);
				}
				osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);

				if (messageSend == 1 && mb->piHandle->transferInfo.block[0].errStatus == LEO_ERROR_GOOD) {
					messageSend = 0;
					continue;
				}
				break;
			}
			osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
			if (mb->piHandle->transferInfo.blockNum == 1) {
				osYieldThread();
			}
		} else {
			switch (mb->hdr.type) {
			case OS_MESG_TYPE_DMAREAD:
				osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
				ret = dm->dma(OS_READ, mb->devAddr, mb->dramAddr, mb->size);
				break;
			case OS_MESG_TYPE_DMAWRITE:
				osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
				ret = dm->dma(OS_WRITE, mb->devAddr, mb->dramAddr, mb->size);
				break;
			case OS_MESG_TYPE_EDMAREAD:
				osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
				ret = dm->edma(mb->piHandle, OS_READ, mb->devAddr, mb->dramAddr, mb->size);
				break;
			case OS_MESG_TYPE_EDMAWRITE:
				osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
				ret = dm->edma(mb->piHandle, OS_WRITE, mb->devAddr, mb->dramAddr, mb->size);
				break;
			case OS_MESG_TYPE_LOOPBACK:
				osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
				ret = -1;
				break;
			default:
				ret = -1;
				break;
			}
			if (ret == 0) {
				osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
				osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
				osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
			}
		}
	}
}
