#pragma once
#include "Common.h"
#include "rtpHeader.h"
#include <assert.h>
#include <stdint.h>
extern "C"
{
#include "x264.h"
#include "x264_config.h"
};
#include "rtsp_server_api.h"
#pragma comment(lib,"libx264.lib")

class rtspStream
{
public:
	rtspStream(int streamID,int width,int height,int fps,int bitrate,rtspStreamSendData& lpCallBack);
	~rtspStream(void);
private:
	SOCKET m_SendSocket;
	int m_nStreamPort;
	HANDLE m_hSendThread;
	HANDLE m_hExitEvent;
	HANDLE m_hDelIPEvent;
	HANDLE m_hSendEvent;
	map<unsigned,RtspSessionInfo*> m_SessionList;
	SOCKADDR_IN  sockaddr;
	char* fSDPLines;
	char* fTrackId;
	CRITICAL_SECTION cs;
protected:
	bool initStream();
	static DWORD _stdcall InitSendThread(LPVOID lpParam)
	{
		rtspStream* pC = (rtspStream*)lpParam;
		return pC->SendThread(lpParam);
	}

	DWORD SendThread(LPVOID lpParam);
	char* GetSdpLines();
	float duration();
	char* getAuxSDPLine();
public:
	int m_nStreamID;
	int m_nStreamRTCPPort;
	int m_nStreamRTPPort;
	char* generateSDPDescription();
	bool AddSessionIP(unsigned,RtspSessionInfo*);
	bool DelSessionIP(unsigned);
	bool playSession(unsigned);
	bool pauseSession(unsigned);
	char* trackId();
	unsigned short GetRtpSeqNum();
	unsigned int GetRtpTimestamp();
	// h264 parameter
private:
	unsigned int estBitrate;
	// RTP parameter
private:
	unsigned char fRTPPayloadType;
	unsigned short fSeqNo;
	RTPuint32 m_nTimeStamp;
	pRTPHeader m_pRtpHeader;
	unsigned int m_nSSRC;
	int m_nFPS; // frame per-second 
	unsigned char* m_pSendData;
public:
	void PrepareRTPHeader();
	void GetOneFrame();
	unsigned char* pLoadBuf;
	unsigned char* PYUVBuf;

	//create h264 frame
	x264_t* pX264Handle;
	x264_param_t* pX264Param;
	x264_nal_t* pNals;
	x264_picture_t* pPicIn;
	x264_picture_t* pPicOut;
	int frame_num;
	int m_nWidth;
	int m_nHeight;
	void initH264Encoder(int weith,int height,int fps,int bitRate);
public:
	void static SendData(unsigned char* data ,int size,int type,RTSPStreamHandle lpParam);//type:0-RGB;1-YUV
};

