#include "rtsp_server_api.h"
#include "rtspProfileHeader.h"


RTSPServerHandle RTSPServer_InitRTSPServerServer(char* IPAddr, int port)
{
	rtspServer* pServer = new rtspServer(IPAddr,port);
	return (RTSPServerHandle)pServer;
}

WORD32 RTSPServer_InsertStream(RTSPServerHandle hServer, int streamID,RTSPStreamHandle hStream)
{
	rtspServer* pServer = (rtspServer*)hServer;
	rtspStream* pStream = (rtspStream*)hStream;
	pServer->insertStream(streamID,pStream);
	return 0;
}

WORD32 RTSPServer_ExitRTSPServerInstance(RTSPServerHandle h)
{
	rtspServer* pServer = (rtspServer*)h;
	delete pServer;
	pServer = NULL;
	return 0;
}

RTSPStreamHandle RTSPStream_InitRTSPStream(int streamID,int width, int height,int fps,int bitrate,rtspStreamSendData& pCallback)
{
	rtspStream* pStream = new rtspStream(streamID,width,height,fps,bitrate,pCallback);
	return (RTSPStreamHandle) pStream;
}

WORD32 RTSPStream_ExitRTSPStreamInstance(RTSPStreamHandle h)
{
	rtspStream* pStream = (rtspStream*)h;
	delete pStream;
	return 0;
}