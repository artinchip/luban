/*
    add by jun.ma 2023.02.08
*/

#ifndef OMX_CoreExt1_h
#define OMX_CoreExt1_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>



#define OMX_COMPONENT_DEMUXER_NAME              "OMX.AIC.DEMUXER.ALL"
#define OMX_COMPONENT_DEMUXER_ROLE              "DEMUXER"

#define OMX_COMPONENT_VDEC_NAME              "OMX.AIC.VDEC.ALL"
#define OMX_COMPONENT_VIDEO_RENDER_NAME              "OMX.AIC.VIDEORENDER.ALL"

#define OMX_COMPONENT_ADEC_NAME              "OMX.AIC.ADEC.ALL"
#define OMX_COMPONENT_AUDIO_RENDER_NAME              "OMX.AIC.AUDIORENDER.ALL"

#define OMX_COMPONENT_CLOCK_NAME              "OMX.AIC.CLOCK.ALL"


#define DEMUX_PORT_AUDIO_INDEX		0
#define DEMUX_PORT_VIDEO_INDEX		1
#define DEMUX_PORT_CLOCK_INDEX		2

#define VDEC_PORT_IN_INDEX		0
#define VDEC_PORT_OUT_INDEX		1

#define VIDEO_RENDER_PORT_IN_VIDEO_INDEX		0
#define VIDEO_RENDER_PORT_IN_CLOCK_INDEX		1

#define ADEC_PORT_IN_INDEX		0
#define ADEC_PORT_OUT_INDEX		1

#define AUDIO_RENDER_PORT_IN_AUDIO_INDEX		0
#define AUDIO_RENDER_PORT_IN_CLOCK_INDEX		1

#define CLOCK_PORT_OUT_VIDEO 0   // OMX_CLOCKPORT0 0x00000001
#define CLOCK_PORT_OUT_AUDIO 1   // OMX_CLOCKPORT1 0x00000002


typedef struct OMX_PORT_TUNNELEDINFO
{
	OMX_U32 nTunneledFlag;
	OMX_U32 nPortIndex;  /** port index. */
	OMX_HANDLETYPE pSelfComp;
	OMX_U32 nTunnelPortIndex; /** tunneled port index. */
	OMX_HANDLETYPE pTunneledComp; /** tunneled component handle. */
}OMX_PORT_TUNNELEDINFO;



typedef enum OMX_EXTNCOMMANDTYPE
{
	OMX_CommandStop = 0x7F000001,
	OMX_CommandNops = 0x7F000002
}OMX_EXTNCOMMANDTYPE;



typedef enum OMX_EXTEVENTTYPE
{
	//dmuxer 0x7F000000 - 0x7F000FFF
	OMX_EventDemuxerStartVideoPts = 0x7F000000,
	OMX_EventDemuxerStartAudioPts = 0x7F000001,

	//VideoRender 0x7F001000 - 0x7F001FFF
	OMX_EventVideoRenderPts = 0x7F001000,
	OMX_EventVideoRenderFirstFrame = 0x7F001001,

	//AudioRender 0x7F002000 - 0x7F002FFF
	OMX_EventAudioRenderPts = 0x7F002000,
	OMX_EventAudioRenderFirstFrame = 0x7F002001

}OMX_EXTEVENTTYPE;




#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_CoreExt_h */
/* File EOF */

