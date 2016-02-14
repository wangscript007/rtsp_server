#include "rtspSocket.h"

rtspSocket::~rtspSocket(void)
{
	rtsp_close_socket();
	WSACleanup( );
}

rtspSocket::rtspSocket(rtsp_callback_func pfunc,LPVOID lpParam,char* IpAddr,int port)
	:m_server_listen_socket(0)
	,m_pCallBack(pfunc)
	,m_lpRtspServer(lpParam)
	,m_hlistenThread(NULL)
	,m_hlistenEvent(NULL)
{
	rtsp_create_socket(IpAddr,port);
	g_nServerPort = port;
}

bool rtspSocket::rtsp_create_socket(char* IPAddr,int port)
{
	// create socket for listening client's request 
	WSADATA wsaData;
	if ( WSAStartup( MAKEWORD( 1, 1 ), &wsaData ) != 0 ) 
		return false;

	if ( LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 )
	{
		WSACleanup( );
		return false;
	}
	m_server_listen_socket = socket(AF_INET,SOCK_STREAM,0);

	SOCKADDR_IN  sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr.s_addr  = inet_addr(IPAddr);

	bind(m_server_listen_socket,(struct sockaddr *)&sockaddr,sizeof(sockaddr));
	listen(m_server_listen_socket,SOMAXCONN);

	m_hlistenEvent = WSACreateEvent();
	WSAEventSelect(m_server_listen_socket,  m_hlistenEvent,FD_ACCEPT);
	m_hExitEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hlistenThread = CreateThread(NULL,0,initThread,this,NULL,NULL);

	// get url
	char fVal[16];
	int addrNBO = htonl(sockaddr.sin_addr.s_addr);
	sprintf(fVal, "%u.%u.%u.%u", (addrNBO>>24)&0xFF, (addrNBO>>16)&0xFF, (addrNBO>>8)&0xFF, addrNBO&0xFF);
	memcpy(g_pszServerIP,fVal,16);

	if (port == RTSP_LISTENING_PORT /* the default port number */) 
		sprintf(m_URL, "rtsp://%s/", fVal);
	else 
		sprintf(m_URL, "rtsp://%s:%hu/",fVal, port);
	
	return true;
}

void rtspSocket::rtsp_close_socket()
{
	if (m_server_listen_socket == 0) return ;
	SetEvent(m_hlistenEvent);
	Sleep(100);
	closesocket(m_server_listen_socket);
}

DWORD rtspSocket::ListenThread()
{
	HANDLE hMultipleHandles[2] = {m_hExitEvent,m_hlistenEvent};
	while (1)
	{
		switch(WaitForMultipleObjects(2,hMultipleHandles,false,INFINITE))
		{
		case WAIT_OBJECT_0:
			ResetEvent(m_hExitEvent);
			return 0;
		default:
 			WSAResetEvent(m_hlistenEvent);
			//ProcClientSession();
			m_pCallBack(m_server_listen_socket,m_lpRtspServer);
			break;
		}
	}
	return 0;
}
