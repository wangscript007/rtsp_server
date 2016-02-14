#ifndef _A_THE_RTSP_INTERFACE_H
#define  _A_THE_RTSP_INTERFACE_H

#if defined DLL_EXPORT
#define Interface_API __declspec(dllexport)
#else
#define Interface_API __declspec(dllexport)
#endif

#define DEFAULT_RTSP_PORT 554
#define DEFAULT_RTSP_BITRATE 1024*1024

typedef unsigned __int64	WORD64;
typedef unsigned long		WORD32;
typedef unsigned short		WORD16;
typedef unsigned char		WORD8;

typedef unsigned long		RTSPServerHandle;
typedef unsigned long		RTSPStreamHandle;

typedef void (*rtspStreamSendData)(unsigned char* data, int  size, int type,RTSPStreamHandle lpStream);//type:0-RGB; 1-YUV
#ifdef __cplusplus
extern "C"
{
#endif
	Interface_API RTSPServerHandle	RTSPServer_InitRTSPServerServer(char* IPAddr, int port);
	Interface_API WORD32					RTSPServer_InsertStream(RTSPServerHandle hServer, int StreamID,RTSPStreamHandle hStream);
	Interface_API WORD32					RTSPServer_ExitRTSPServerInstance(RTSPServerHandle h);

	Interface_API RTSPStreamHandle RTSPStream_InitRTSPStream(int streamID,int width,int height,int fps,int bitrate,rtspStreamSendData& callback);
	Interface_API WORD32					RTSPStream_ExitRTSPStreamInstance(RTSPStreamHandle h);
#ifdef __cplusplus
};
#endif

#endif