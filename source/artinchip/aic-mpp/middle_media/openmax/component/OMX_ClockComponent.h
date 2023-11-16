/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_ClockComponent
*/

#ifndef _OMX_Clock_COMPONENT_H_
#define _OMX_Clock_COMPONENT_H_
#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <inttypes.h>

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "OMX_Component.h"

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"

OMX_ERRORTYPE OMX_ClockComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_ClockComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

#define CLOCK_PORT_NUM_MAX 2

typedef struct CLOCK_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t stateLock;
	OMX_CALLBACKTYPE *pCallbacks;
	OMX_PTR pAppData;
	OMX_HANDLETYPE hSelf;

	OMX_PORT_PARAM_TYPE sPortParam;
	OMX_PARAM_PORTDEFINITIONTYPE sOutPortDef[2];
	OMX_PARAM_BUFFERSUPPLIERTYPE sOutBufSupplier[2];
	OMX_PORT_TUNNELEDINFO sOutPortTunneledInfo[2];

	pthread_t threadId;
	pthread_t decodeThreadId;
	struct aic_message_queue       sMsgQue;

	OMX_TIME_CONFIG_CLOCKSTATETYPE sClockState;
	OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE sActiveRefClock;
	OMX_TICKS sPortStartTime[CLOCK_PORT_NUM_MAX];

	OMX_TICKS sRefClockTimeBase;//unit us
	OMX_TICKS sWallTimeBase;
	OMX_TICKS sPauseTimePoint;
	OMX_TICKS sPauseTimeDurtion;

}CLOCK_DATA_TYPE;

#endif


