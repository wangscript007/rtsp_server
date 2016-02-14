#include "rtspServer.h"

rtspServer::rtspServer(char* IPAddr,int port)
{
	m_pSocket = new rtspSocket(AllocClientSession, this,IPAddr,port);
	InitializeCriticalSection(&cs);

	m_hDelMsgThread = CreateThread(NULL,NULL,initDelMsgThread,this,NULL,NULL);
	m_hDelMsgEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hExitEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
}

rtspServer::~rtspServer(void)
{
	delete m_pSocket;
}

void rtspServer::AllocClientSession(int listenSocket,LPVOID lpParam)
{
	rtspServer* pServer = (rtspServer*) lpParam; 
	pServer->getNewSession(listenSocket); 
}

void rtspServer::getNewSession(int listenSocket)
{
	CRtspClientSession* pCSession = new CRtspClientSession(listenSocket,this,sendMsg,timeGetTime());
	m_pSessionList.push_back(*pCSession);
}

void rtspServer::handleRequestBytes(int newBytesRead,unsigned char* in_request, char* out_response,LPVOID lpParam,SOCKADDR_IN clientIPAddr,int sessionID)
{
	rtspServer* pCls = (rtspServer*)lpParam;
	// analysis the request
	if (newBytesRead < 0 || newBytesRead > RTSP_BUFFER_SIZE)
		return ;

	char cmdName[RTSP_PARAM_STRING_MAX];
	char urlPreSuffix[RTSP_PARAM_STRING_MAX];
	char urlSuffix[RTSP_PARAM_STRING_MAX];
	char cseq[RTSP_PARAM_STRING_MAX];
	char sessionIdStr[RTSP_PARAM_STRING_MAX];
	unsigned contentLength = 0;
	
	pCls->m_fRequestBuffer = (char*)in_request;
	pCls->m_fResponseBuffer = (char*)out_response;
	pCls->m_fCurrentCSeq = cseq;

	bool parseSucceeded = parseRTSPRequestString((char*)in_request, strlen((char*)in_request),
		cmdName, sizeof cmdName,
		urlPreSuffix, sizeof urlPreSuffix,
		urlSuffix, sizeof urlSuffix,
		cseq, sizeof cseq,
		sessionIdStr, sizeof sessionIdStr,
		contentLength);

	if (!parseSucceeded)	
		return ;
	// make up the response
	if (strcmp(cmdName,"OPTIONS") == 0)
	{
		pCls->handleCmd_OPTION();
	}

	if (strcmp(cmdName,"DESCRIBE") == 0)
	{
		pCls->handleCmd_DESCRIBE(urlPreSuffix, urlSuffix, (char const*)in_request);
	}

	if (strcmp(cmdName,"SETUP") == 0)
	{
		pCls->handleCmd_SETUP(urlPreSuffix, urlSuffix, (char const*)in_request,clientIPAddr,sessionID);
	}

	if (strcmp(cmdName,"PLAY") == 0)
	{
		pCls->handleCmd_PLAY(urlPreSuffix, urlSuffix, (char const*)in_request,atoi(sessionIdStr));
	}

	if(strcmp(cmdName,"PAUSE") == 0)
	{
		pCls->handleCmd_PAUSE(urlPreSuffix,atoi(sessionIdStr));
	}

	if(strcmp(cmdName,"TEARDOWN") == 0)
	{
		pCls->handleCmd_TEARDOWN(urlPreSuffix,atoi(sessionIdStr));
	}

	if (strcmp(cmdName,"GET_PARAMETER") == 0)
	{
		pCls->handleCmd_GET_PARAMETER(urlPreSuffix,atoi(sessionIdStr));
	}

	if (strcmp(cmdName,"SET_PARAMETER") == 0)
	{
		pCls->handleCmd_SET_PARAMETER(urlPreSuffix,atoi(sessionIdStr));
	} 
}

void rtspServer::handleCmd_OPTION()
{
	// take it as heartbeat detection
	sprintf((char*)m_fResponseBuffer, "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",m_fCurrentCSeq, dateHeader(), allowedCommandNames());
}

void rtspServer::handleCmd_DESCRIBE(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr)
{
	// find if the session requested by client is exist
	char* sessionName = (char*)urlSuffix;

	// get stream's description from stream
	rtspStream* pServer = findStream(atoi(sessionName));

	if (pServer == NULL)
	{
		handleCmd_sessionNotFound();
		return ;
	}
	char* sdpTempDescribe = pServer->generateSDPDescription();
	int tmpSize = strlen(sdpTempDescribe);

	// make up the response
	sprintf((char*)m_fResponseBuffer,
		"RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
		"%s"
		"Content-Base: %s%s/\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		m_fCurrentCSeq,
		dateHeader(),
		m_pSocket->m_URL,
		sessionName,
		tmpSize,
		sdpTempDescribe);
}

void rtspServer::handleCmd_REGISTER()
{

}

void rtspServer::handleCmd_SETUP(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr,SOCKADDR_IN clientIPAddr,int sessionID )
{
	char const* streamName = urlPreSuffix; // in the normal case
	char const* trackId = urlSuffix; // in the normal case
	char* concatenatedStreamName = NULL; // in the normal case
	// find if stream is existed
	rtspStream* pStream = findStream(atoi(trackId));
	if (pStream == NULL) 
	{
		handleCmd_notFound();
		return;
	}
	// make up response message
	StreamingMode streamingMode;
	char* streamingModeString = NULL; // set when RAW_UDP streaming is specified
	char* clientsDestinationAddressStr;
	unsigned char clientsDestinationTTL;
	unsigned short clientRTPPortNum, clientRTCPPortNum;
	unsigned char rtpChannelId, rtcpChannelId;

	parseTransportHeader(fullRequestStr, streamingMode, streamingModeString,
		clientsDestinationAddressStr, clientsDestinationTTL,
		clientRTPPortNum, clientRTCPPortNum,
		rtpChannelId, rtcpChannelId);

	RtspSessionInfo* rsi = new RtspSessionInfo(clientIPAddr,clientRTPPortNum,0);

	pStream->AddSessionIP(sessionID, rsi);

	sprintf((char*)m_fResponseBuffer,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Transport: RTP/AVP;unicast;destination=%s;source=%s;client_port=%d-%d;server_port=%d-%d\r\n"
		"Session: %08d \r\n\r\n",
		m_fCurrentCSeq,
		dateHeader(),      
		inet_ntoa(clientIPAddr.sin_addr),g_pszServerIP, clientRTPPortNum, clientRTCPPortNum , pStream->m_nStreamRTPPort, pStream->m_nStreamRTCPPort,
 		sessionID);
}

void rtspServer::handleCmd_PLAY(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr,unsigned sessionID)
{
	rtspStream* pStream = findStream(atoi(urlPreSuffix));
	if (pStream == NULL)
	{
		handleCmd_notFound();
		return ;
	}
	// make up response
	char const* rtpInfoFmt =
		"%s" // "RTP-Info:", plus any preceding rtpInfo items
		"url=%s%s/%s"
		";seq=%d"
		";rtptime=%u";

	unsigned rtpInfoFmtSize = strlen(rtpInfoFmt);
	char* rtpInfo = strDup("RTP-Info: ");
	char* prevRTPInfo = rtpInfo;
	unsigned rtpInfoSize = rtpInfoFmtSize
		+ strlen(prevRTPInfo)
		+ 1
		+ strlen(m_pSocket->m_URL) + strlen(urlSuffix)
		+ 5 /*max unsigned short len*/
		+ 10 /*max unsigned (32-bit) len*/
		+ 2 /*allows for trailing \r\n at final end of string*/;
		rtpInfo = new char[rtpInfoSize];
	sprintf(rtpInfo, rtpInfoFmt,
		 prevRTPInfo,
		m_pSocket->m_URL, pStream->trackId(),pStream->trackId(),
		pStream->GetRtpSeqNum(),
		pStream->GetRtpTimestamp()
		);

	char* rangeHeader = "npt=0.000-\r\n";
	sprintf((char*)m_fResponseBuffer,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"%s"
		"Session: %08u\r\n"
		"%s\r\n\r\n",
		m_fCurrentCSeq,
		dateHeader(),
		rangeHeader,
		sessionID,
		rtpInfo);

	pStream->playSession(sessionID);
	delete rtpInfo;
}

void rtspServer::handleCmd_PAUSE(char const* urlPreSuffix,unsigned sessionID)
{
	rtspStream* pStream = findStream(1111);
	if (pStream == NULL)
	{
		handleCmd_notFound();
		return ;
	}
	pStream->pauseSession(sessionID);
	setRTSPResponse("200 OK",pStream->m_nStreamID);
}

void rtspServer::handleCmd_TEARDOWN(char const* urlPreSuffix,unsigned sessionID)
{
	rtspStream* pStream = findStream(atoi(urlPreSuffix));
	if (pStream == NULL)
	{
		handleCmd_notFound();
		return ;
	}
	pStream->DelSessionIP(sessionID);

	for (auto i = m_pSessionList.begin(); i != m_pSessionList.end() ; i++)
	{
		if (i->m_nSessionID == sessionID)
		{
			SetEvent(i->m_hExitEvent);
			Sleep(100);
			m_pSessionList.erase(i);
			return;
		}
	}

	setRTSPResponse("200 OK",sessionID);
}

void rtspServer::handleCmd_GET_PARAMETER(char const* urlPreSuffix,unsigned sessionID) 
{
	rtspStream* pStream = findStream(atoi(urlPreSuffix));
	setRTSPResponse("200 OK",sessionID);
}

void rtspServer::handleCmd_SET_PARAMETER(char const* urlPreSuffix,unsigned sessionID) 
{
	rtspStream* pStream = findStream(atoi(urlPreSuffix));
	setRTSPResponse("200 OK",sessionID);
}

void rtspServer::handleCmd_bad()
{
	sprintf((char*)m_fResponseBuffer,	"RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",dateHeader(), allowedCommandNames());
}

void rtspServer::handleCmd_notSupported()
{
	sprintf((char*)m_fResponseBuffer,	"RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
		m_fCurrentCSeq, dateHeader(), allowedCommandNames());
}

void rtspServer::handleCmd_notFound()
{
	setRTSPResponse("404 Stream Not Found");
}

void rtspServer::handleCmd_sessionNotFound()
{
	setRTSPResponse("454 Session Not Found");
}

void rtspServer::handleCmd_unsupportedTransport()
{
	setRTSPResponse("461 Unsupported Transport");
}

char const* rtspServer::allowedCommandNames() {
	return "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER";
}

rtspStream* rtspServer::findStream(int streamID)
{
	auto i = m_StreamMap.find(streamID);

	if (i != m_StreamMap.end())
		return i->second;

	return NULL;
}

void rtspServer::setRTSPResponse(char const* responseStr) {
		sprintf((char*)m_fResponseBuffer,
			"RTSP/1.0 %s\r\n"
			"CSeq: %s\r\n"
			"%s\r\n",
			responseStr,
			m_fCurrentCSeq,
			dateHeader());
}

void rtspServer::setRTSPResponse(char const* responseStr, int sessionId) {
		sprintf((char*)m_fResponseBuffer,
			"RTSP/1.0 %s\r\n"
			"CSeq: %s\r\n"
			"%s"
			"Session: %d\r\n\r\n",
			responseStr,
			m_fCurrentCSeq,
			dateHeader(),
			sessionId);
}

void rtspServer::setRTSPResponse(char const* responseStr, char const* contentStr) {
		if (contentStr == NULL) contentStr = "";
		unsigned const contentLen = strlen(contentStr);

		sprintf((char*)m_fResponseBuffer, 
			"RTSP/1.0 %s\r\n"
			"CSeq: %s\r\n"
			"%s"
			"Content-Length: %d\r\n\r\n"
			"%s",
			responseStr,
			m_fCurrentCSeq,
			dateHeader(),
			contentLen,
			contentStr);
}

void rtspServer::setRTSPResponse(char const* responseStr, int sessionId, char const* contentStr) {
		if (contentStr == NULL) contentStr = "";
		unsigned const contentLen = strlen(contentStr);

		sprintf((char*)m_fResponseBuffer,
			"RTSP/1.0 %s\r\n"
			"CSeq: %s\r\n"
			"%s"
			"Session: %d\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",
			responseStr,
			m_fCurrentCSeq,
			dateHeader(),
			sessionId,
			contentLen,
			contentStr);
}

DWORD rtspServer::delMsg(LPVOID lpParam)
{
	HANDLE m_hEvent[2] = {m_hExitEvent,m_hDelMsgEvent};
	char response[RTSP_BUFFER_SIZE] = {0};
	while(1)
	{
		switch(WaitForMultipleObjects(2,m_hEvent,NULL,INFINITE))
		{
		case WAIT_OBJECT_0:
			ResetEvent(m_hExitEvent);
			return 0;
		case WAIT_OBJECT_0 +1:
			ResetEvent(m_hDelMsgEvent);
			pSessionMsg pMsg = m_MsgList.front();
			handleRequestBytes(pMsg->byteCount,pMsg->request,response,this,pMsg->addrClient,pMsg->sessionID);
			if (response !=NULL)
				send(pMsg->SendSocket,(const char*)response,strlen(response),0);
			cout<<"send message "<<endl;
			cout<<response<<endl;
			m_MsgList.pop_front();
		}
	}
}

void rtspServer::sendMsg(pSessionMsg msg,LPVOID lpParam)
{
	rtspServer* pC = (rtspServer*)lpParam;
	EnterCriticalSection(&pC->cs);
	pC->m_MsgList.push_back(msg);
	SetEvent(pC->m_hDelMsgEvent);
	LeaveCriticalSection(&pC->cs);
}

void rtspServer::insertStream(int streamID,rtspStream* pStream)
{
	m_StreamMap.insert(pair<int,rtspStream*>(streamID,pStream));
}