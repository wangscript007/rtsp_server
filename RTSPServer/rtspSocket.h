#pragma once
#include "Common.h"
#define RTSP_LISTENING_PORT 554

typedef void (*rtsp_callback_func)(int listensocket,LPVOID lpParam);

class rtspSocket
{
	// Using TCP protocol  insures the connection being safe 
public:
	rtspSocket(rtsp_callback_func,LPVOID,char* IpAddr,int port = RTSP_LISTENING_PORT);
	virtual ~rtspSocket(void);
	char m_URL[100];
private:
	int m_server_listen_socket;
	LPVOID m_lpRtspServer;
	rtsp_callback_func m_pCallBack;
	HANDLE m_hlistenThread;
	HANDLE m_hlistenEvent;
	HANDLE m_hExitEvent;
protected:
	bool	rtsp_create_socket(char* IPAddr,int port);
	void	rtsp_close_socket();
	static DWORD  _stdcall initThread(LPVOID lpParam)
	{
		rtspSocket* pc = (rtspSocket*) lpParam;
		return pc->ListenThread();
	}
	DWORD ListenThread();
};