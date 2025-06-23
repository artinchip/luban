/*
* Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
*
* SPDX-License-Identifier: Apache-2.0
*
*  author: <jun.ma@artinchip.com>
*  Desc: rtsp_parser
*/

#ifndef __AIC_RTSP_H__
#define __AIC_RTSP_H__

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include <semaphore.h>

// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 256*1024

typedef struct AICStreamInfo
{
	uint32_t av_present_flag;
	uint32_t v_codec_id;
	uint32_t height;
	uint32_t width;
	uint32_t frm_rate;
	uint32_t a_codec_id;
	uint32_t samp_rate;
	uint32_t  channel_num;
	uint32_t  samp_bits;
	uint32_t duration;
}AICStreamInfo;

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class AICStreamClientState {
public:
	AICStreamClientState();
	virtual ~AICStreamClientState();

public:
	MediaSession* session;
	MediaSubsession* subsession;
	TaskToken streamTimerTask;
	double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "AICStreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "AICStreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "AICStreamClientState" field to the subclass:

class AICRTSPClient: public RTSPClient {
public:
  static AICRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
				  int verbosityLevel = 0,
				  char const* applicationName = NULL,
				  portNumBits tunnelOverHTTPPortNum = 0);

public:
	AICRTSPClient(UsageEnvironment& env, char const* rtspURL,
	int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
	void resetClient() {reset();}
	void setRtspBaseURL(char const* url) {setBaseURL(url);}
	virtual ~AICRTSPClient();

public:
	AICStreamClientState scs;
	void *client_ctx; // use keep AICRtspClientCtx pointer
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class AICMediaSink : public MediaSink
{
public:
	static AICMediaSink *createNew (UsageEnvironment &env, MediaSubsession &subsession,    // identifies the kind of data that's being received
									char const *streamId = NULL, void *priv = NULL);  // identifies the stream itself (optional)
	int32_t getFrameInfo(int32_t *fFrameSize,struct timeval *fPresentationTime);
	int32_t getFrameData(char * buf,int32_t size);

	void setMediaType(int32_t type) {mediaType = type;}
	int32_t getMediaType() {return mediaType;}

private:
	AICMediaSink(UsageEnvironment &env, MediaSubsession &subsession, char const *streamId, void *priv);

	// called only by "createNew()"
	virtual     ~AICMediaSink(void);
	static void afterGettingFrame (void *clientData, unsigned frameSize, unsigned numTruncatedBytes,
							struct timeval  presentationTime, unsigned durationInMicroseconds);
	void        afterGettingFrame (unsigned frameSize, unsigned numTruncatedBytes, struct timeval  presentationTime,
								unsigned durationInMicroseconds);
private:
    // redefined virtual functions:
    virtual Boolean continuePlaying(void);

public:
	u_int8_t        *fReceiveBuffer;
	uint32_t         fReceiveBufferSize;
	int32_t 		fFrameSize;
	struct timeval  fPresentationTime;
	MediaSubsession &fSubsession;
	char            *fStreamId;
	void            *fPrivateData;
	int32_t          mediaType;
	int index;
	FILE* fid;
};

typedef enum
{
	RTSP_PLAY_NONE = 0,
	RTSP_PLAY_OPEN,
	RTSP_PLAY_SETUP,
	RTSP_PLAY_DATA_LOOP,
	RTSP_PLAY_EXIT,
	RTSP_PLAY_MAX,
} RTSP_PLAY_STATE;

class AICRtspClientCtx
{
public:
	AICRtspClientCtx();
	~AICRtspClientCtx();
public:
	AICRTSPClient *client;
	UsageEnvironment *env;
	TaskScheduler    *scheduler;
	char event_loop_ctrl;
	char event_data_ctrl;
	RTSP_PLAY_STATE play_state;
	double play_time;
	int32_t http_port;
	int32_t result_code;
	uint32_t timeout;
	uint64_t last_keepalive_time;
	uint32_t wait_data_timeout;
	uint32_t wait_data_timeout_count;
	int8_t  error;
	int8_t  data_got;
	int8_t get_param;
	int8_t rtsp_tcp;
	int8_t rtsp_http;
	int8_t got_bye_mask;
	int8_t got_first_key_frame;
	unsigned char *p_a_extra;
	int32_t a_extra_size;
	unsigned char *p_v_extra;
	int32_t v_extra_size;
	char *str_sdp;
	AICStreamInfo *p_stream_info;
	void * p_current_data_sink;
	pthread_t   heart_beat_handle;
	sem_t       sem;

};

struct aic_rtsp_parser {
	struct aic_parser base;
	char uri[256];
	void* rtsp_ctx;
};

#define _RTSP_DEBUG_

enum rtsp_debug_level
{
	RTSP_DBG_ERR  = (1<<0),
	RTSP_DBG_INFO = (1<<1),
	RTSP_DBG_DBG = (1<<2)
};

extern uint32_t g_rtsp_debug_level;

#ifdef _RTSP_DEBUG_
	#define RTSP_DEBUG(level,fmt,args...) do {\
												if (level & g_rtsp_debug_level) {\
												printf("\033[40;32m" " [%s:%d] " fmt "\033[0m \n" ,__FUNCTION__,__LINE__,##args);\
												}\
											} while(0)

#else
	#define RTSP_DEBUG(...)
#endif

extern "C" void* rtsp_open(char *rtsp_url);
extern "C" int rtsp_play(void* client_ctx);
extern "C" int rtsp_peek(void* client_ctx,struct aic_parser_packet *pkt);
extern "C" int rtsp_read(void* client_ctx,struct aic_parser_packet *pkt);
extern "C" int rtsp_pause(void *client_ctx);
extern "C" int rtsp_close(void *client_ctx);
#endif
