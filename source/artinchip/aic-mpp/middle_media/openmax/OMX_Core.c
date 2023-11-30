/*
* Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_DemuxerComponent
*/

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "OMX_DemuxerComponent.h"
#include "OMX_VdecComponent.h"
#include "OMX_VideoRenderComponent.h"
#include "OMX_AdecComponent.h"
#include "OMX_AudioRenderComponent.h"
#include "OMX_ClockComponent.h"
#include "OMX_MuxerComponent.h"
#include "OMX_VencComponent.h"

OMX_COMPONENTREGISTERTYPE OMX_ComponentRegistered[] = {
	{OMX_COMPONENT_DEMUXER_NAME,OMX_DemuxerComponentInit},
	{OMX_COMPONENT_VDEC_NAME,OMX_VdecComponentInit},
	{OMX_COMPONENT_VIDEO_RENDER_NAME,OMX_VideoRenderComponentInit},
	{OMX_COMPONENT_ADEC_NAME,OMX_AdecComponentInit},
	{OMX_COMPONENT_AUDIO_RENDER_NAME,OMX_AudioRenderComponentInit},
	{OMX_COMPONENT_CLOCK_NAME,OMX_ClockComponentInit},
	{OMX_COMPONENT_MUXER_NAME,OMX_MuxerComponentInit},
	{OMX_COMPONENT_VENC_NAME,OMX_VencComponentInit}
};

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_BOOL bFind = OMX_FALSE;
	OMX_S32 i,index;
	OMX_S32 nCompNum = sizeof(OMX_ComponentRegistered) / sizeof(OMX_COMPONENTREGISTERTYPE);

	for (i = 0; i < nCompNum; i++) {
		if (!strcmp(cComponentName, OMX_ComponentRegistered[i].pName)) {
			bFind = OMX_TRUE;
			index = i;
			break;
		}
	}

	if (bFind == OMX_TRUE) {
		*pHandle = (OMX_HANDLETYPE)malloc(sizeof(OMX_COMPONENTTYPE));
		eError = OMX_ComponentRegistered[index].pInitialize(*pHandle);
		if (eError == OMX_ErrorNone) {
			((OMX_COMPONENTTYPE *)(*pHandle))->SetCallbacks(*pHandle,pCallBacks,pAppData);
			logd("get handle ok,component name:%s\n",OMX_ComponentRegistered[index].pName);
		} else {
			loge("find compoent but init fail:0x%x!!\n",eError);
			free(*pHandle);
		}
	} else {
		eError = OMX_ErrorComponentNotFound;
	}

	return eError;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN  OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	eError = ((OMX_COMPONENTTYPE*)hComponent)->ComponentDeInit(hComponent);
	free(hComponent);
	return eError;
}

/*OpenMAX_IL_1_1_2_Specification.pdf 3.2.3.6 OMX_SetupTunnel*/

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN  OMX_HANDLETYPE hOutput,
    OMX_IN  OMX_U32 nPortOutput,
    OMX_IN  OMX_HANDLETYPE hInput,
    OMX_IN  OMX_U32 nPortInput)
{
    OMX_TUNNELSETUPTYPE tunnel_setup;
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE * comp_in;
    OMX_COMPONENTTYPE * comp_out;

    comp_in = (OMX_COMPONENTTYPE *)hInput;
    comp_out = (OMX_COMPONENTTYPE *)hOutput;

    if (NULL != hOutput &&  NULL != hInput) {// setup
        tunnel_setup.eSupplier = OMX_BufferSupplyMax;
		//step1 output
		ret = comp_out->ComponentTunnelRequest(hOutput, nPortOutput,
												hInput, nPortInput,
													&tunnel_setup);
		//step2 input
		if (OMX_ErrorNone == ret) {
			ret = comp_in->ComponentTunnelRequest(hInput, nPortInput,
								hOutput, nPortOutput,&tunnel_setup);
			if (OMX_ErrorNone != ret) {
				logd("unable to setup tunnel on input component.\n");
				comp_out->ComponentTunnelRequest(hOutput, nPortOutput,NULL, 0, NULL);
			}
		} else {
			logd("unable to setup tunnel on output component.\n");
		}
	} else if (NULL == hOutput && NULL != hInput) {//cancel input
		comp_in->ComponentTunnelRequest(hInput, nPortInput,NULL, 0, NULL);
	} else if (NULL == hInput && NULL != hOutput) {//cancel output
		comp_out->ComponentTunnelRequest(hOutput, nPortOutput,NULL, 0, NULL);
	} else {
		return OMX_ErrorBadParameter;
	}
    return ret;
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole (
	OMX_IN      OMX_STRING role,
	OMX_INOUT   OMX_U32 *pNumComps,
	OMX_INOUT   OMX_U8  **compNames)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent (
	OMX_IN      OMX_STRING compName,
	OMX_INOUT   OMX_U32 *pNumRoles,
	OMX_OUT     OMX_U8 **roles)
{
		OMX_ERRORTYPE eError = OMX_ErrorNone;
		return eError;

}

