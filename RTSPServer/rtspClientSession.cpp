#include "rtspClientSession.h"

CRtspClientSession::CRtspClientSession(int listensocket,LPVOID lpParam,SendMsg callbackFunc,int sessionID)
	:m_hExitEvent(NULL)
	,m_hRecvEvent(NULL)
	,m_hRecvThread(NULL)
	,m_lpRtspServer(lpParam)
	,m_pCallBack(callbackFunc)
	,m_nSessionID(sessionID)
{
	//create receive thread 
	int len=sizeof(SOCKADDR);
	m_RecvSocket = accept(listensocket,(SOCKADDR*)&addrClient,&len);
	m_hRecvEvent = WSACreateEvent();
	SetEvent(m_hRecvEvent);
	WSAEventSelect(m_RecvSocket,m_hRecvEvent,FD_READ);
	m_hExitEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hRecvThread = CreateThread(NULL,NULL,InitRecvThread,this,NULL,NULL);
}

CRtspClientSession::~CRtspClientSession(void)
{
	SetEvent(m_hExitEvent);
	if (m_RecvSocket != NULL)
		closesocket(m_RecvSocket);
}

DWORD CRtspClientSession::RecvThread(LPVOID lpParam)
{
	HANDLE m_hMutihandle[2] = {m_hExitEvent,m_hRecvEvent};

	char request[RTSP_BUFFER_SIZE] = {0};
	char response[RTSP_BUFFER_SIZE]  = {0};
	int bytesCount  = 0;
	while(1)
	{
		switch(WaitForMultipleObjects(2,m_hMutihandle,FALSE,INFINITE))
		{
		case WAIT_OBJECT_0:
			ResetEvent(m_hExitEvent);
			closesocket(m_RecvSocket);
			m_RecvSocket = NULL;
			return 0;
		case WAIT_OBJECT_0+1:
			WSAResetEvent(m_hRecvEvent);
			bytesCount = recv(m_RecvSocket,request,RTSP_BUFFER_SIZE,0);
			if (bytesCount <1) 	break;
			pSessionMsg pMsg = new SessionMsg();
			pMsg->addrClient = addrClient;
			pMsg->byteCount = bytesCount;
			memcpy(pMsg->request ,request,bytesCount);
			pMsg->SendSocket = m_RecvSocket;
			pMsg->sessionID = m_nSessionID;
			m_pCallBack(pMsg,m_lpRtspServer);
#ifdef DEBUG_TEST
			cout<<"get request "<<endl;
			cout<<request<<endl;
#endif
			break;
		}
	}
	return 0;
}
