/*
* Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
*
* SPDX-License-Identifier: Apache-2.0
*
*  author: <jun.ma@artinchip.com>
*  Desc: rtsp_parser
*/

#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>

#include "aic_stream.h"
#include "mov.h"
#include "mov_tags.h"
#include "mpp_mem.h"
#include "mpp_log.h"
#include "rtsp.h"

uint32_t g_rtsp_debug_level = (RTSP_DBG_ERR|RTSP_DBG_INFO|RTSP_DBG_DBG);

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
	return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
	return env << subsession.mediumName() << "/" << subsession.codecName();
}

static void shutdownStream(RTSPClient* rtspClient);

/* return true if the RTSP command succeeded */
static int8_t wait_live555_response(AICRtspClientCtx *ctx)
{
	ctx->error = 1;
	ctx->event_loop_ctrl = 0;
	ctx->result_code = 0;
	ctx->env->taskScheduler().doEventLoop( &ctx->event_loop_ctrl);
	return !ctx->error;
}

static void default_callback_after_cmd( RTSPClient* client, int result_code, char* result_string )
{
	AICRTSPClient *rtsp_client = static_cast<AICRTSPClient *> ( client );
	AICRtspClientCtx *ctx = static_cast<AICRtspClientCtx *>(rtsp_client->client_ctx);

	RTSP_DEBUG(RTSP_DBG_DBG,"result_code:%d \n",result_code);
	ctx->result_code = result_code;
	ctx->error = (ctx->result_code != 0);
	ctx->event_loop_ctrl = 1;
	delete []result_string;
}


static void continue_after_describe( RTSPClient* client, int result_code,
                                   char* result_string )
{
	AICRTSPClient*rtsp_client = static_cast<AICRTSPClient *> (client);
	AICRtspClientCtx *ctx = static_cast<AICRtspClientCtx *>(rtsp_client->client_ctx);
	ctx->result_code = result_code;

	if ( result_code == 0 ) {
		if (ctx->str_sdp) {
			free(ctx->str_sdp);
			ctx->str_sdp = NULL;
		}
		if (result_string) {
			ctx->str_sdp = strDup(result_string);
			ctx->error = false;
		}
	} else {
		ctx->error = true;
	}
	delete[] result_string;
	// Set event loop exit
	ctx->event_loop_ctrl= 1;

}

static void continue_after_options( RTSPClient* client, int result_code, char* result_string )
{
	AICRTSPClient*rtsp_client = static_cast<AICRTSPClient *> (client);
	AICRtspClientCtx *ctx = static_cast<AICRtspClientCtx *>(rtsp_client->client_ctx);

	RTSP_DEBUG(RTSP_DBG_DBG,"result_code:%d,result_string:%s",result_code,result_string);

	/*
		If OPTIONS fails, assume GET_PARAMETER is not supported but
		still continue on with the stream.  Some servers (foscam)
		return 501/not implemented for OPTIONS.
	*/

	ctx->get_param = result_code == 0 && result_string != NULL && strstr(result_string, "GET_PARAMETER") != NULL;
	rtsp_client->sendDescribeCommand(continue_after_describe);
	delete[] result_string;
}

// Implementation of "AICRTSPClient":
AICRTSPClient *AICRTSPClient::createNew(UsageEnvironment &env, char const *rtspURL, int verbosityLevel,
                                        char const *applicationName, portNumBits tunnelOverHTTPPortNum)
{
	return new AICRTSPClient(env,rtspURL,verbosityLevel,applicationName,tunnelOverHTTPPortNum);
}

AICRTSPClient::AICRTSPClient(UsageEnvironment &env, char const *rtspURL, int verbosityLevel,
                            char const *applicationName, portNumBits tunnelOverHTTPPortNum) :
    RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1)
{
}


AICRTSPClient::~AICRTSPClient(void)
{
	RTSP_DEBUG(RTSP_DBG_DBG,"~AICRTSPClient \n");
}

// Implementation of "AICStreamClientState":
AICStreamClientState::AICStreamClientState(void) :
    session(NULL),
    subsession(NULL),
    streamTimerTask(NULL),
    duration(0.0)
{

}

AICStreamClientState::~AICStreamClientState(void)
{
	if (session != NULL) {
		// We also need to delete "session", and unschedule "streamTimerTask" (if set)
		UsageEnvironment    &env = session->envir();    // alias
		env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
		Medium::close(session);
		RTSP_DEBUG(RTSP_DBG_DBG,"~AICStreamClientState \n");
	}
}

AICRtspClientCtx::AICRtspClientCtx(void)
{
	client = NULL;
	env = NULL;
	scheduler = NULL;
	http_port = 0;
	error = false;
	get_param = false;
	rtsp_tcp = false;
	rtsp_http = false;
	got_bye_mask = false;;
	play_time = 0.0;
	timeout = 60; // default is 60 seconds
	last_keepalive_time = 0;
	str_sdp = NULL;
	p_a_extra = NULL;
	a_extra_size = 0;
	p_v_extra = NULL;
	v_extra_size = 0;
	event_loop_ctrl = 0;
	event_data_ctrl = 0;
	wait_data_timeout = 1*1000*1000;
	wait_data_timeout_count = 0;
	got_first_key_frame = true;
}

AICRtspClientCtx::~AICRtspClientCtx(void)
{
	http_port = 0;
	if (str_sdp) {
		free(str_sdp);
		str_sdp = NULL;
	}
	if (p_a_extra) {
		free(p_a_extra);
		p_a_extra = NULL;
	}
	if (p_v_extra) {
		free(p_v_extra);
		p_v_extra = NULL;
	}
	if (client) {
		shutdownStream(client);
	}
	if (p_stream_info) {
		delete  p_stream_info;
		p_stream_info = NULL;
	}
	if (env) {
		env->reclaim();
		env = NULL;
	}
	if (scheduler) {
		delete scheduler;
		scheduler = NULL;
	}
	RTSP_DEBUG(RTSP_DBG_DBG,"~AICRtspClientCtx \n");
}

// Implementation of "AICMediaSink":
// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
AICMediaSink *AICMediaSink::createNew(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId, void *priv)
{
	return new AICMediaSink(env,subsession,streamId, priv);
}

AICMediaSink::AICMediaSink(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId, void *priv) :
    MediaSink(env),
    fSubsession(subsession),
    fPrivateData(priv)
{
	fStreamId = strDup(streamId);
	fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
	fReceiveBufferSize = DUMMY_SINK_RECEIVE_BUFFER_SIZE;
	index = 0;
	fid = NULL;
}

AICMediaSink::~AICMediaSink(void)
{
	delete[] fReceiveBuffer;
	delete[] fStreamId;
	RTSP_DEBUG(RTSP_DBG_DBG,"~AICMediaSink \n");
}

void AICMediaSink::afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                    struct timeval  presentationTime, unsigned durationInMicroseconds)
{
	AICMediaSink    *sink = (AICMediaSink *)clientData;
	sink->afterGettingFrame(frameSize,numTruncatedBytes,presentationTime,durationInMicroseconds);
}

void AICMediaSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                                    struct timeval  presentationTime, unsigned durationInMicroseconds)
{
	AICRtspClientCtx *ctx = NULL;
	ctx = (AICRtspClientCtx*)fPrivateData;

	// if (fStreamId != NULL) envir() << "Stream \"" << fStreamId << "\"; ";
	// envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
	// if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
	// char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
	// sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
	// envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
	// if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
	// 	envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
	// }
	// envir() << "\n";
	this->fFrameSize = frameSize;
	if (!strcmp(fSubsession.mediumName(),"video")) {
		if (!strcmp(fSubsession.codecName(),"H264")) {
			this->fFrameSize = frameSize + 4;
			if ((fReceiveBuffer[0] & 0x1f) == 5) {// IDR
				ctx->got_first_key_frame = true;
			}
			if (numTruncatedBytes > 0) {
				RTSP_DEBUG(RTSP_DBG_DBG,"fReceiveBufferSize:%u,numTruncatedBytes:%u,frameSize:%u\n",fReceiveBufferSize,numTruncatedBytes,frameSize);
				ctx->got_first_key_frame = false;
			}
		} else {
			ctx->got_first_key_frame = true;
		}
	}
	fPresentationTime = presentationTime;
	ctx->data_got = true;
	ctx->p_current_data_sink = this;
	continuePlaying();
	ctx->event_data_ctrl = 1;
}

Boolean AICMediaSink::continuePlaying(void)
{
	if (fSource == NULL){
		RTSP_DEBUG(RTSP_DBG_ERR,"fSource == NULL \n");
		return false;
	}
	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(fReceiveBuffer,fReceiveBufferSize,afterGettingFrame,this,onSourceClosure,this);
	// onSourceClosure will call subsession_after_playing of sub->sink->startPlaying(*(sub->readSource()), subsession_after_playing, sub);
	//[in sessions_setup function]
	return True;
}

int32_t AICMediaSink::getFrameInfo(int32_t *frameSize,struct timeval *presentationTime)
{
	*frameSize = fFrameSize;
	*presentationTime = fPresentationTime;
	return 0;
}

int32_t AICMediaSink::getFrameData(char *buf,int32_t size)
{
	int32_t len;
	char start_code[4] = {0x00,0x00,0x00,0x01};
	len = size;

	if (fFrameSize < size) {
		len = fFrameSize;
	}

	if(!strcmp(fSubsession.codecName(),"H264")) {
		memcpy(buf,start_code,sizeof(start_code));
		len -= 4;
		memcpy(buf+sizeof(start_code),fReceiveBuffer,len);
	} else {
		memcpy(buf,fReceiveBuffer,len);
	}

	return len;
}

//void shutdownStream(RTSPClient* rtspClient, int exitCode) {
void shutdownStream(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	AICStreamClientState& scs = ((AICRTSPClient*)rtspClient)->scs; // alias
	AICRtspClientCtx      *ctx = (AICRtspClientCtx *)((AICRTSPClient*)rtspClient)->client_ctx;


	// First, check whether any subsessions have still to be closed:
	if (scs.session != NULL) {
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.session);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				RTSP_DEBUG(RTSP_DBG_ERR," close  %s stream \n",subsession->mediumName());
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				if (subsession->rtcpInstance() != NULL) {
					// in case the server sends a RTCP "BYE" while handling "TEARDOWN"
					subsession->rtcpInstance()->setByeHandler(NULL, NULL);
				}
				someSubsessionsWereActive = True;
			}
		}
		if (someSubsessionsWereActive) {
			// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
			// Don't bother handling the response to the "TEARDOWN".
			rtspClient->sendTeardownCommand(*scs.session,default_callback_after_cmd,NULL);
		}
	}
	RTSP_DEBUG(RTSP_DBG_DBG,"close rtspClient\n");
	//env << *rtspClient << "Closing the stream.\n";
	Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

	// if (--rtspClientCount == 0) {
	// // The final stream has ended, so exit the application now.
	// // (Of course, if you're embedding this code into your own application, you might want to comment this out,
	// // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
	// exit(exitCode);
	// }
}

static void subsession_after_playing(void *clientData)
{
	MediaSubsession *subsession = (MediaSubsession *)clientData;
	//RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);
	AICRTSPClient* rtspClient = (AICRTSPClient*)(subsession->miscPtr);
	AICRtspClientCtx      *ctx = (AICRtspClientCtx*)rtspClient->client_ctx;

	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
	subsession->sink = NULL;
	if (strcmp(subsession->mediumName(),"video")) {
		ctx->got_bye_mask &= ~MPP_MEDIA_TYPE_VIDEO;
	} else if (strcmp(subsession->mediumName(),"audio")) {
		ctx->got_bye_mask &= ~MPP_MEDIA_TYPE_AUDIO;
	}
	RTSP_DEBUG(RTSP_DBG_DBG,"close stream %s\n",subsession->mediumName());
  // Next, check whether *all* subsessions' streams have now been closed:
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while ((subsession = iter.next()) != NULL) {
		if (subsession->sink != NULL) {
			RTSP_DEBUG(RTSP_DBG_DBG,"%s subsession is still active \n",subsession->mediumName());
			return; // this subsession is still active
		}
	}
	// All subsessions' streams have now been closed, so shutdown the client:
	ctx->error = 1;
	ctx->event_data_ctrl = 1;
}

static void subsession_bye_handler(void *clientData)
{
	MediaSubsession     *subsession = (MediaSubsession *)clientData;
	AICRTSPClient       *rtspClient = (AICRTSPClient *)subsession->miscPtr;
	AICRtspClientCtx      *ctx = (AICRtspClientCtx*)rtspClient->client_ctx;

	RTSP_DEBUG(RTSP_DBG_DBG,"%s stream received RTCP BYE \n",subsession->mediumName());
	//subsession_after_playing(subsession);
	if (ctx) {
		if (strcmp(subsession->mediumName(),"video")) {
			ctx->got_bye_mask &= ~MPP_MEDIA_TYPE_VIDEO;
		} else if (strcmp(subsession->mediumName(),"audio")) {
			ctx->got_bye_mask &= ~MPP_MEDIA_TYPE_AUDIO;
		}
		if (!ctx->got_bye_mask) {
			ctx->error = 1;
			ctx->event_data_ctrl = 1;
			RTSP_DEBUG(RTSP_DBG_DBG,"All streams received RTCP BYE\n");
		}
	}

}

static uint8_t* parse_h264_config_str( char const* configStr, unsigned int& configSize)
{
	SPropRecord* props = NULL;
	unsigned numsRecord = 0;
	unsigned char *cfg = NULL;
	unsigned cfg_len = 0;
	unsigned i = 0;

	if( configStr == NULL || *configStr == '\0' ){
		RTSP_DEBUG(RTSP_DBG_ERR,"configstr error!!! \n");
		goto fexit;
	}
	props = parseSPropParameterSets(configStr, numsRecord);
	if ((props == NULL) || (numsRecord == 0)) {
		goto fexit;
	}
	for (i=0; i<numsRecord; i++) {
		cfg_len += 4;
		cfg_len += props[i].sPropLength;
	}
	cfg = new uint8_t[cfg_len];
	if (cfg == NULL){
		RTSP_DEBUG(RTSP_DBG_ERR,"new cfg error!!! \n");
		goto fexit;
	}
	configSize = 0;
	for (i=0; i<numsRecord; i++) {
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x00;
		cfg[configSize++] = 0x01;
		memcpy(cfg+configSize, props[i].sPropBytes, props[i].sPropLength);
		configSize += props[i].sPropLength;
		RTSP_DEBUG(RTSP_DBG_DBG,"props, i = %d, sPropLength %d\n", i, props[i].sPropLength);
	}
	RTSP_DEBUG(RTSP_DBG_DBG,"numsRecord = %d, configSize = %d\n",numsRecord, configSize);

	fexit:
	if (props) {
		for (i=0; i<numsRecord; i++) {
			if (props[i].sPropBytes) {
				delete[] props[i].sPropBytes;
				props[i].sPropBytes = NULL;
			}
		}
		delete[] props;
	}
	return cfg;
}

static void get_audio_info(AICRtspClientCtx *ctx, MediaSubsession *sub)
{
	uint32_t a_codec = 0;
	uint8_t channel_num = 0;
	uint32_t sample_rate = 0;
	uint32_t sample_bits = 0;

	channel_num = sub->numChannels();
	sample_rate = sub->rtpTimestampFrequency();
	 if ( !strcmp( sub->codecName(), "MPA" ) ||
		!strcmp( sub->codecName(), "MPA-ROBUST" ) ||
		!strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) ) {
		a_codec = MPP_CODEC_AUDIO_DECODER_MP3;
		sample_rate = 0;
		RTSP_DEBUG(RTSP_DBG_DBG,"codecName:%s\n",sub->codecName());
	} else if ( !strcmp( sub->codecName(), "AC3" ) ) {
		a_codec = MPP_CODEC_AUDIO_DECODER_AAC;
		sample_rate = 0;
	}  else {
		RTSP_DEBUG(RTSP_DBG_ERR,"unsupport codec:%s\n",sub->codecName());
	}

	if (ctx->p_stream_info == NULL) {
		ctx->p_stream_info = new AICStreamInfo;
		memset(ctx->p_stream_info, 0, sizeof(AICStreamInfo));
	}
	ctx->p_stream_info->av_present_flag |= MPP_MEDIA_TYPE_AUDIO;
	ctx->got_bye_mask |= MPP_MEDIA_TYPE_AUDIO;
	if (ctx->p_stream_info != NULL) {
		ctx->p_stream_info->a_codec_id = a_codec;
		ctx->p_stream_info->channel_num = channel_num;
		ctx->p_stream_info->samp_rate = sample_rate;
		ctx->p_stream_info->samp_bits = sample_bits;
	}
	RTSP_DEBUG(RTSP_DBG_DBG,"codecName:%s,av_present_flag:%d,channel_num:%d,samp_rate:%d,samp_bits:%d\n"
			,sub->codecName()
			,ctx->p_stream_info->av_present_flag
			,ctx->p_stream_info->channel_num
			,ctx->p_stream_info->samp_rate
			,ctx->p_stream_info->samp_bits);
}

static void get_video_info(AICRtspClientCtx *ctx, MediaSubsession *sub)
{
	uint32_t v_codec = 0;

	if (!strcmp( sub->codecName(), "H264" )) {
		unsigned int i_extra = 0;
		uint8_t      *p_extra = NULL;
		v_codec = MPP_CODEC_VIDEO_DECODER_H264;//RTSP_CODEC_H264;
		RTSP_DEBUG(RTSP_DBG_DBG,"codecName:%s\n",sub->codecName());
		// parse sps pps
		p_extra=parse_h264_config_str( sub->fmtp_spropparametersets(), i_extra);

		if (p_extra != NULL) {
			ctx->v_extra_size = i_extra;
			if (ctx->p_v_extra)
			{
				free(ctx->p_v_extra);
				ctx->p_v_extra = NULL;
			}

			ctx->p_v_extra = (unsigned char*)malloc(i_extra);
			memcpy(ctx->p_v_extra, p_extra, i_extra);
			delete[] p_extra;
		}
		ctx->got_first_key_frame = false;
	} else if( !strcmp( sub->codecName(), "JPEG" ) ) {
		v_codec = MPP_CODEC_VIDEO_DECODER_MJPEG;
		ctx->got_first_key_frame = true;
		RTSP_DEBUG(RTSP_DBG_DBG,"codecName:%s\n",sub->codecName());
	} else {
		RTSP_DEBUG(RTSP_DBG_ERR,"unsupport codec:%s\n",sub->codecName());
	}

	if (ctx->p_stream_info == NULL) {
		ctx->p_stream_info = new AICStreamInfo;
		memset(ctx->p_stream_info, 0, sizeof(AICStreamInfo));
	}
	ctx->got_bye_mask |= MPP_MEDIA_TYPE_VIDEO;
	ctx->p_stream_info->av_present_flag |= MPP_MEDIA_TYPE_VIDEO;
	ctx->p_stream_info->v_codec_id = v_codec;
	ctx->p_stream_info->width = sub->videoWidth();
	ctx->p_stream_info->height = sub->videoHeight();
	ctx->p_stream_info->frm_rate = sub->videoFPS();
	RTSP_DEBUG(RTSP_DBG_DBG,"codecName:%s,av_present_flag:%d,width:%d,height:%d,frm_rate:%d\n"
				,sub->codecName()
				,ctx->p_stream_info->av_present_flag
				,ctx->p_stream_info->width
				,ctx->p_stream_info->height
				,ctx->p_stream_info->frm_rate);
}

static void get_media_info(AICRtspClientCtx *ctx)
{
	AICStreamClientState *scs = NULL;
	MediaSubsession *sub = NULL;
	// double dura = 0.0;
	if ((ctx == NULL)||(ctx->client == NULL)) {
		return;
	}

	scs = &ctx->client->scs;
	if (scs == NULL) {
		return;
	}

	MediaSubsessionIterator iter(*scs->session);
	ctx->timeout = ctx->client->sessionTimeoutParameter();
	RTSP_DEBUG(RTSP_DBG_DBG,"timeout:%u\n",ctx->timeout);

	while ((sub = iter.next()) != NULL) {
		if (!strcmp( sub->mediumName(), "video")) {
			get_video_info(ctx, sub);
		} else if (!strcmp( sub->mediumName(), "audio")) {
			get_audio_info(ctx, sub);
		}
	}
	if (ctx->p_stream_info) {
		ctx->p_stream_info->duration = 0;//0xFFFFFFFF; // live play
	}
}

static int sessions_setup(AICRtspClientCtx *ctx)
{
	int ret = -1;
	unsigned int ret_code = 0;
	Boolean bret = False;
	AICStreamClientState *scs = NULL;
	MediaSubsession *sub = NULL;

	if ((ctx == NULL) || (ctx->client == NULL)) {
		return ret;
	}
	scs = &ctx->client->scs;
	MediaSubsessionIterator iter(*scs->session);
	while ((sub = iter.next()) != NULL) {
		RTSP_DEBUG(RTSP_DBG_DBG,"setup session: %s/%s \n",sub->mediumName(), sub->codecName());
		if (!strcmp(sub->codecName(), "X-ASF-PF")) {
			bret = sub->initiate(0);
		} else {
			bret = sub->initiate();
		}
		if (!bret) {
			RTSP_DEBUG(RTSP_DBG_ERR,"Failed to initiate the sub session.%s \n",ctx->env->getResultMsg());
			continue;
		} else {
			if (sub->rtcpIsMuxed()) {
				RTSP_DEBUG(RTSP_DBG_DBG,"client port %d \n",sub->clientPortNum());
			} else {
				RTSP_DEBUG(RTSP_DBG_DBG,"client ports %d-%d \n",sub->clientPortNum() ,sub->clientPortNum() + 1);
			}
	try_again:
			ret_code = ctx->client->sendSetupCommand(*sub,default_callback_after_cmd,false,ctx->rtsp_tcp);
			if((ret_code == 0) || (false == wait_live555_response(ctx))) {
				// Maybe we should try TCP
				RTSP_DEBUG(RTSP_DBG_ERR,"Setup Sub session %s/%s failed, ret code %d \n", sub->mediumName(), sub->codecName(), ctx->result_code);
				if (ctx->result_code == 461) { // Unsupported Transport
					ctx->rtsp_tcp = !(ctx->rtsp_tcp);
					goto try_again;
				}
				RTSP_DEBUG(RTSP_DBG_ERR,"Setup Sub session %s/%s failed, ret code %d \n", sub->mediumName(), sub->codecName(), ctx->result_code);
				continue;
			} else {
				RTSP_DEBUG(RTSP_DBG_DBG,"Setup Sub session %s/%s OK\n", sub->mediumName(), sub->codecName());
				// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
				// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
				// after we've sent a RTSP "PLAY" command.)
				sub->sink = AICMediaSink::createNew(*(ctx->env), *sub, ctx->client->url(), (void*)ctx);
					// perhaps use your own custom "MediaSink" subclass instead
				if (sub->sink == NULL) {
					RTSP_DEBUG(RTSP_DBG_ERR,"Failed to create a data sink for the session \n");
					continue;
				}
				if (!strcmp(sub->mediumName(),"video")) {
					AICMediaSink *sink = static_cast<AICMediaSink *> (sub->sink);
					sink->setMediaType(MPP_MEDIA_TYPE_VIDEO);
				} else if (!strcmp(sub->mediumName(),"audio")) {
					AICMediaSink *sink = static_cast<AICMediaSink *> (sub->sink);
					sink->setMediaType(MPP_MEDIA_TYPE_AUDIO);
				}
				sub->miscPtr = ctx->client; // a hack to let subsession handle functions get the "RTSPClient" from the subsession
				sub->sink->startPlaying(*(sub->readSource()), subsession_after_playing, sub);
				// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
				if (sub->rtcpInstance() != NULL) {
					sub->rtcpInstance()->setByeHandler(subsession_bye_handler, sub);
				}
			}
		}
	}
	//live555_print("%s, leave @ tick %d\n", __FUNCTION__, osal_get_tick());
	get_media_info(ctx);
	ret = 0;
	return ret;
}

#ifdef SUPPORT_RTSP_SWITCH_RTP_DATA_TO_TCP
static int rtsp_switch_to_tcp(AICRtspClientCtx *ctx)
{
	int ret = -1;
	int ret_code = 0;
	AICStreamClientState *scs = NULL;
	char *rtspUrl = NULL;
	MediaSubsession *sub = NULL;

	ret_code = ctx->client->sendTeardownCommand(*ctx->client->scs.session, default_callback_after_cmd, NULL);
	if((ret_code = 0) || (false == wait_live555_response(ctx))) {
		RTSP_DEBUG(RTSP_DBG_ERR,"RTSP OVER TCP teardown failed \n");
		return ret;
	}
	rtspUrl = strDup(ctx->client->url());
	MediaSubsessionIterator iter(*(ctx->client->scs.session));
	while ((sub = iter.next()) != NULL) {
		Medium::close(sub->sink);
		sub->sink = NULL;
	}
	Medium::close(ctx->client->scs.session);
	ctx->client->scs.session = NULL;
	ctx->client->scs.subsession = NULL;
	ctx->client->resetClient();
	ctx->client->setRtspBaseURL(rtspUrl);
	free(rtspUrl);
	rtspUrl = NULL;

	ret_code = ctx->client->sendOptionsCommand(continue_after_options, NULL);
	// here will return after DESCRIBE, we should got SDP after this call
	if ((ret_code == 0) || (false == wait_live555_response(ctx)) || (ctx->str_sdp == NULL)) {
		RTSP_DEBUG(RTSP_DBG_ERR,"OPTION/DESCRIBE failed ret_code = %d, sdp %s\n", ret_code, ctx->str_sdp);
		goto exit;
	}
	// Create media session from SDP
	scs = &ctx->client->scs;
	scs->session = MediaSession::createNew(*(ctx->env), ctx->str_sdp);
	if (scs->session == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR,"Cteate Media session fail\n");
		goto exit;
	}
	else if (!scs->session->hasSubsessions()) {
		RTSP_DEBUG(RTSP_DBG_ERR,"This session has no media subsessions (i.e., no \"m=\" lines)\n");
		goto exit;
	}
	ctx->rtsp_tcp = true;
	ret = rtsp_play(ctx);
	return ret;

exit:
	delete ctx;
	ctx = NULL;
	return ret;
}
#endif

static void * heart_beat(void *para)
{
	struct timespec time = {0};
	int32_t time_diff;
	int32_t index = 0;
	AICRtspClientCtx *ctx = (AICRtspClientCtx *)para;

	if (ctx == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR,"para error \n");
		return NULL;
	}
	if (ctx->timeout == 0) {
		RTSP_DEBUG(RTSP_DBG_DBG,"there is no need to do \n");
		return NULL;
	}
	if (ctx->rtsp_tcp || !ctx->get_param) {
		RTSP_DEBUG(RTSP_DBG_DBG,"there is no need to do \n");
		return NULL;
	}
    while(1) {
        clock_gettime(CLOCK_REALTIME, &time);
        time.tv_sec += ctx->timeout * 3 / 4;
        if (sem_timedwait(&ctx->sem, &time)) {
			RTSP_DEBUG(RTSP_DBG_DBG,"sendGetParameterCommand %d\n",index++);
			ctx->client->sendGetParameterCommand(*(ctx->client->scs.session), NULL, NULL, NULL);
		} else {
			RTSP_DEBUG(RTSP_DBG_DBG,"heart_beat exit \n");
			break;
		}
	}
	return NULL;
}

static void data_delay_check( void *p_private )
{
	AICRtspClientCtx *ctx = (AICRtspClientCtx*)p_private;

	RTSP_DEBUG(RTSP_DBG_ERR,"ctx->data_got:%d",ctx->data_got);
	ctx->wait_data_timeout_count++;
	if (ctx->data_got == false) {
		ctx->event_data_ctrl = 1;
		ctx->error = true;
	}
}

extern "C" void* rtsp_open(char *rtsp_url)
{
	unsigned int ret_code = 0;
	AICRtspClientCtx *ctx = NULL;
	TaskScheduler       *scheduler = NULL;
	UsageEnvironment    *env = NULL;
	AICStreamClientState *scs = NULL;

	//  we should do some url check here,URL may contain usr/pwd,now,not support.
	ctx = new AICRtspClientCtx();
	if (ctx == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR,"new AICRtspClientCtx failed \n");
		return NULL;
	}
	scheduler = BasicTaskScheduler::createNew();
	env = BasicUsageEnvironment::createNew(*scheduler);
	ctx->env = env;
	ctx->scheduler = scheduler;
	ctx->client = AICRTSPClient::createNew(*(ctx->env), rtsp_url, RTSP_CLIENT_VERBOSITY_LEVEL, "Live555", ctx->http_port);
	if (ctx->client == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR,"create RTSP Client failed \n");
		delete ctx;
		ctx = NULL;
		return NULL;
	}
	ctx->client->client_ctx = (void*) ctx;
	RTSP_DEBUG(RTSP_DBG_ERR," rtsp_url %s \n",rtsp_url);
	ret_code = ctx->client->sendOptionsCommand(continue_after_options, NULL);
	// here will return after DESCRIBE, we should got SDP after this call
	if ((ret_code == 0) || (0 == wait_live555_response(ctx)) || (ctx->str_sdp == NULL)) {
		RTSP_DEBUG(RTSP_DBG_ERR,"OPTION/DESCRIBE failed ret_code %d\n",ret_code);
		delete ctx;
		ctx = NULL;
		return NULL;
	}

	scs = &ctx->client->scs;
	// Create a media session object from this SDP description
	scs->session = MediaSession::createNew(*(ctx->env), ctx->str_sdp);
	if (scs->session == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR,"Failed to create a MediaSession object from the SDP description:%s \n",ctx->env->getResultMsg());
		delete ctx;
		ctx = NULL;
	}
	else if (!scs->session->hasSubsessions()){
		RTSP_DEBUG(RTSP_DBG_ERR,"This session has no media subsessions (i.e., no \"m=\" lines)\n");
		delete ctx;
		ctx = NULL;
	}

	return (void*)ctx;
}

extern "C" int rtsp_play(void* client_ctx)
{
	int ret = -1;
	AICRtspClientCtx *ctx = NULL;
	AICRTSPClient *rtsp_client = NULL;
	MediaSession *session = NULL;
	unsigned int ret_code = 0;

	if (client_ctx == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR," param error \n");
		goto exit;
	}
	ctx = (AICRtspClientCtx*)client_ctx;

	ret = sessions_setup(ctx);
	if (ret < 0) {
		RTSP_DEBUG(RTSP_DBG_ERR," setup session fail\n");
		goto exit;
	}
	rtsp_client = ctx->client;
	session = rtsp_client ->scs.session;
	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	if (session->absStartTime() != NULL) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		ret_code = rtsp_client->sendPlayCommand( *session, default_callback_after_cmd, session->absStartTime(), session->absEndTime());
	} else {
		rtsp_client->scs.duration = session->playEndTime() - session->playStartTime();
		ret_code = rtsp_client->sendPlayCommand( *session, default_callback_after_cmd, session->playStartTime(), -1, 1 );
	}
	if((ret_code == 0) || (false == wait_live555_response(ctx))) {
		RTSP_DEBUG(RTSP_DBG_ERR," RTSP PLAY failed\n");
		goto exit;
	}
	sem_init(&ctx->sem, 0, 0);
	if (pthread_create(&ctx->heart_beat_handle, NULL, heart_beat, ctx) != 0) {
		RTSP_DEBUG(RTSP_DBG_ERR," pthread_create failed\n");
		sem_destroy(&ctx->sem);
	}
	return 0;
exit:
	return -1;
}

extern "C" int rtsp_peek(void* client_ctx,struct aic_parser_packet *pkt)
{
	int ret = -1;
	AICRtspClientCtx *ctx = NULL;
	ctx = (AICRtspClientCtx*)client_ctx;
	int32_t frameSize;
	struct timeval presentationTime;
	TaskToken task;

	ctx->error = 0;
	ctx->event_data_ctrl = 0;
	ctx->data_got = false;
	task = ctx->env->taskScheduler().scheduleDelayedTask(ctx->wait_data_timeout, data_delay_check, ctx);
	ctx->env->taskScheduler().doEventLoop( &ctx->event_data_ctrl);
	ctx->env->taskScheduler().unscheduleDelayedTask(task);
	/*
	during doEventLoop, 3 cause may happen
	 cause 1 , one of subsession->sink are closed
	    it will call onSourceClosure->subsession_after_playing
	    if all subsession->sink are closed,doEventLoop will return.
	 casue 2 , rcv bye
	    it will call subsession_bye_handler,doEventLoop will return
		if all subsession rcv bye,doEventLoop will return.
	 cause 3 , wait_data_timeout
	    it will call data_delay_check,it will set ctx->error =1, ,doEventLoop will return
	*/
	if (ctx->error) {
		if (ctx->wait_data_timeout_count > 10) {// timeout more than 10 times
			RTSP_DEBUG(RTSP_DBG_DBG,"not rcv data in 10s %d \n",ctx->wait_data_timeout_count);
			ctx->wait_data_timeout_count = 0;
			return -1;
		}
		if (!ctx->got_bye_mask) {
			pkt->size = 0;
			pkt->flag = PACKET_EOS;
			return PARSER_EOS;
		}
		return -1;
	}
	if (!ctx->data_got) {
		RTSP_DEBUG(RTSP_DBG_DBG,"rec msg but not data %d \n",ctx->error);
		return -1;
	}
	ctx->wait_data_timeout_count = 0;
	if (ctx->p_stream_info->av_present_flag & MPP_MEDIA_TYPE_VIDEO) {
		if (!ctx->got_first_key_frame) {
			return -1;
		}
	}
	AICMediaSink *sink = static_cast<AICMediaSink *> (ctx->p_current_data_sink);
	sink->getFrameInfo(&frameSize,&presentationTime);
	pkt->size = frameSize;
	pkt->pts = presentationTime.tv_sec*1000*1000 + presentationTime.tv_usec;
	pkt->type = (enum aic_stream_type)sink->getMediaType();
	//RTSP_DEBUG(RTSP_DBG_DBG,"type:%d,size:%d,pts:%ld \n",pkt->type,pkt->size,pkt->pts);
	return 0;
}

extern "C" int rtsp_read(void* client_ctx,struct aic_parser_packet *pkt)
{
	AICRtspClientCtx *ctx = NULL;
	ctx = (AICRtspClientCtx*)client_ctx;
	int32_t frameSize;
	struct timeval presentationTime;

	AICMediaSink *sink = static_cast<AICMediaSink *> (ctx->p_current_data_sink);
	sink->getFrameInfo(&frameSize,&presentationTime);
	sink->getFrameData((char *)pkt->data,pkt->size);
	// RTSP_DEBUG(RTSP_DBG_DBG,"getMediaType:%d,,presentationTime:%ld,frameSize:%d,type:%d,size:%d,pts:%ld \n"
	// 					,sink->getMediaType()
	// 					,presentationTime.tv_sec*1000*1000+presentationTime.tv_usec
	// 					,frameSize
	// 					,pkt->type,pkt->size,pkt->pts);

	return 0;
}

extern "C" int rtsp_pause(void *client_ctx) {
	AICRtspClientCtx *ctx = NULL;
	AICRTSPClient *rtsp_client = NULL;
	MediaSession *session = NULL;

	ctx = (AICRtspClientCtx*)client_ctx;
	rtsp_client = ctx->client;
	session = rtsp_client ->scs.session;
	rtsp_client->sendPauseCommand(*session,default_callback_after_cmd,NULL);
	return 0;
}

extern "C" int rtsp_close(void *client_ctx)
{
	int ret = 0;
	AICRtspClientCtx *ctx = NULL;
	MediaSubsession *sub = NULL;

	if (client_ctx == NULL) {
		RTSP_DEBUG(RTSP_DBG_ERR," param error\n");
		return ret;
	}
	ctx = (AICRtspClientCtx*)client_ctx;
	sem_post(&ctx->sem);
	pthread_join(ctx->heart_beat_handle, NULL);
	sem_destroy(&ctx->sem);
	delete ctx;
	return ret;
}