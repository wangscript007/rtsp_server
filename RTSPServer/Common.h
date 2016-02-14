#pragma once
#define SOFTWARE_VERSION "1.0"
#define RTSP_BUFFER_SIZE 2000
#define RTSP_PARAM_STRING_MAX 200
#define TIME_OUT "60"

#define _strncasecmp _strnicmp

#include <iostream>
#include <map>
#include <list>
#include <string>
using namespace std;

//#define DEBUG_TEST

#include <WinSock2.h>
#pragma comment(lib,"Ws2_32.lib")
#include <MMSystem.h>
#pragma comment(lib,"Winmm.lib")

#define TRACE printf

#define TEST_SERVREIP "192.168.1.200"
extern char g_pszServerIP[16];//the listen socket's ip address
extern int g_nServerPort;// the listen socket's port
typedef struct _tagRtspSessionInfo{
	SOCKADDR_IN clientAddr;
	int state;// 0,pause;1,play
	_tagRtspSessionInfo(SOCKADDR_IN soc,unsigned short port,int s){clientAddr = soc;state = s;clientAddr.sin_port = ntohs(port);}
}RtspSessionInfo,LPRtspSessionInfo;

//rtspClientsession 消息结构体
typedef struct _tagSessionMsg
{
	int byteCount;
	SOCKET SendSocket;
	SOCKADDR_IN addrClient;
	int sessionID;
	unsigned char request[RTSP_BUFFER_SIZE];
	_tagSessionMsg():byteCount(0),sessionID(0){memset(request,0,RTSP_BUFFER_SIZE);memset(&SendSocket,0,sizeof(SOCKET));memset(&addrClient,0,sizeof(SOCKADDR_IN));}
}SessionMsg,*pSessionMsg;

typedef enum StreamingMode {
	RTP_UDP,
	RTP_TCP,
	RAW_UDP
} StreamingMode;

char* strDupSize(char const* str) ;

char* strDupSize(char const* str, size_t& resultBufSize) ;

char* strDup(char const* str) ;

void decodeURL(char* url);

bool parseRTSPRequestString(char const* reqStr,
	unsigned reqStrSize,
	char* resultCmdName,
	unsigned resultCmdNameMaxSize,
	char* resultURLPreSuffix,
	unsigned resultURLPreSuffixMaxSize,
	char* resultURLSuffix,
	unsigned resultURLSuffixMaxSize,
	char* resultCSeq,
	unsigned resultCSeqMaxSize,
	char* resultSessionIdStr,
	unsigned resultSessionIdStrMaxSize,
	unsigned& contentLength);

bool parseHTTPRequestString(char* resultCmdName, unsigned resultCmdNameMaxSize,
	char* urlSuffix, unsigned urlSuffixMaxSize,
	char* sessionCookie, unsigned sessionCookieMaxSize,
	char* acceptStr, unsigned acceptStrMaxSize);

char const* dateHeader();

void parseTransportHeader(char const* buf,
	StreamingMode& streamingMode,
	char*& streamingModeString,
	char*& destinationAddressStr,
	unsigned char& destinationTTL,
	unsigned short& clientRTPPortNum, // if UDP
	unsigned short& clientRTCPPortNum, // if UDP
	unsigned char& rtpChannelId, // if TCP
	unsigned char& rtcpChannelId // if TCP
	);

char* base64Encode(char const* origSigned, unsigned origLength);

bool  RGB2YUV(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len);

void rgb2yuv_convert(unsigned char *YUV, unsigned char *RGB_RAW, 
	unsigned int width, unsigned int height);