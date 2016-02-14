#pragma once
#include "rtspSocket.h"
typedef void (*rtsp_clientmsg_callback)(int newBytesRead,unsigned char* in_request, char* out_response,LPVOID lpParam,SOCKADDR_IN clientIPAddr,int sessionID);
typedef void (*SendMsg)(pSessionMsg msg,LPVOID lpParam);
class CRtspClientSession
{
public:
	CRtspClientSession(int listensocket,LPVOID ,SendMsg,int sessionID);
	~CRtspClientSession(void);
	
	SOCKADDR_IN addrClient;
	int m_nSessionID;
private:
	int streamID;
	HANDLE m_hRecvThread;
	HANDLE m_hRecvEvent;

	SOCKET m_RecvSocket;
protected:

protected:
	static  DWORD _stdcall InitRecvThread(LPVOID lpParam)
	{
		CRtspClientSession* pCls = (CRtspClientSession*) lpParam;
		return pCls->RecvThread(lpParam);
	}
	DWORD RecvThread(LPVOID lpParam);
	//rtsp_clientmsg_callback m_pCallBack;
	SendMsg m_pCallBack;
	LPVOID m_lpRtspServer;
public:
		HANDLE m_hExitEvent;
};
