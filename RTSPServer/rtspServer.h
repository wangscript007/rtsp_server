#pragma once
#include "rtspProfileHeader.h"
class rtspServer
{
public:
	rtspServer(char* IPAddr,int nServerPort);
	~rtspServer(void);
protected:
	// handle request URL
	static void  handleRequestBytes(int newBytesRead,unsigned char* in_request, char* out_response,LPVOID lpParam,SOCKADDR_IN clientIPAddr,int sessionID);
	static void AllocClientSession(int listenSocket,LPVOID lpParam);

	// handle Client's request option
	virtual void handleCmd_OPTION();
	virtual void handleCmd_DESCRIBE(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr);
	virtual void handleCmd_REGISTER();
	virtual void handleCmd_bad();
	virtual void handleCmd_notSupported();
	virtual void handleCmd_notFound();
	virtual void handleCmd_sessionNotFound();
	virtual void handleCmd_unsupportedTransport();
	// option to control session
	virtual void handleCmd_SETUP(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr ,SOCKADDR_IN clientIPAddr,int );
	virtual void handleCmd_PLAY(char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr, unsigned sessionID);
	virtual void handleCmd_PAUSE(char const* urlPreSuffix,unsigned);
	virtual void handleCmd_TEARDOWN(char const* urlPreSuffix,unsigned);
	virtual void handleCmd_GET_PARAMETER(char const* urlPreSuffix,unsigned) ;
	virtual void handleCmd_SET_PARAMETER(char const* urlPreSuffix,unsigned) ;

	// Shortcuts for setting up a RTSP response (prior to sending it):
	void setRTSPResponse(char const* responseStr);
	void setRTSPResponse(char const* responseStr, int sessionId);
	void setRTSPResponse(char const* responseStr, char const* contentStr);
	void setRTSPResponse(char const* responseStr, int sessionId, char const* contentStr);

	static DWORD _stdcall initDelMsgThread(LPVOID lpParam) { rtspServer* pC = (rtspServer*)lpParam;return pC->delMsg(lpParam); }
	DWORD delMsg(LPVOID lpParam);

private:
	char const* allowedCommandNames();
	rtspStream* findStream(int streamID);	
	HANDLE m_hDelMsgThread;
	HANDLE m_hDelMsgEvent;
	HANDLE m_hExitEvent;
	map<int,rtspStream*> m_StreamMap;
	list<CRtspClientSession> m_pSessionList;
	list<pSessionMsg> m_MsgList;
	char* m_fRequestBuffer;// request URL
	char* m_fResponseBuffer;	// response URL
	char* m_fCurrentCSeq;
	rtspSocket* m_pSocket;// for sending and receiving messages

	void getNewSession(int listenSocket);
	CRITICAL_SECTION cs;
public:
	void static sendMsg(pSessionMsg msg, LPVOID lpParam);
	void insertStream(int streamID,rtspStream* pStream);
};
