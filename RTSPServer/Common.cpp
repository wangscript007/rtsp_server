#include "Common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>

char g_pszServerIP[16] = {0};
int g_nServerPort = 0;
char* strDup(char const* str) 
{
	if (str == NULL) return NULL;
	size_t len = strlen(str) + 1;
	char* copy = new char[len];

	if (copy != NULL) {
		memcpy(copy, str, len);
	}
	return copy;
}

char* strDupSize(char const* str, size_t& resultBufSize) 
{
	if (str == NULL) {
		resultBufSize = 0;
		return NULL;
	}

	resultBufSize = strlen(str) + 1;
	char* copy = new char[resultBufSize];

	return copy;
}

char* strDupSize(char const* str) 
{
	size_t dummy;
	return strDupSize(str, dummy);
}

void decodeURL(char* url) {
	// Replace (in place) any %<hex><hex> sequences with the appropriate 8-bit character.
	char* cursor = url;
	while (*cursor) {
		if ((cursor[0] == '%') &&
			cursor[1] && isxdigit(cursor[1]) &&
			cursor[2] && isxdigit(cursor[2])) {
				// We saw a % followed by 2 hex digits, so we copy the literal hex value into the URL, then advance the cursor past it:
				char hex[3];
				hex[0] = cursor[1];
				hex[1] = cursor[2];
				hex[2] = '\0';
				*url++ = (char)strtol(hex, NULL, 16);
				cursor += 3;
		} else {
			// Common case: This is a normal character or a bogus % expression, so just copy it
			*url++ = *cursor++;
		}
	}

	*url = '\0';
}

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
	unsigned& contentLength)
{

	// This parser is currently rather dumb; it should be made smarter #####

	// "Be liberal in what you accept": Skip over any whitespace at the start of the request:
	unsigned i;
	for (i = 0; i < reqStrSize; ++i) {
		char c = reqStr[i];
		if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0')) break;
	}
	if (i == reqStrSize) return false; // The request consisted of nothing but whitespace!

	// Then read everything up to the next space (or tab) as the command name:
	bool parseSucceeded = false;
	unsigned i1 = 0;
	for (; i1 < resultCmdNameMaxSize-1 && i < reqStrSize; ++i,++i1) {
		char c = reqStr[i];
		if (c == ' ' || c == '\t') {
			parseSucceeded = true;
			break;
		}

		resultCmdName[i1] = c;
	}
	resultCmdName[i1] = '\0';
	if (!parseSucceeded) return false;

	// Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
	unsigned j = i+1;
	while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j; // skip over any additional white space
	for (; (int)j < (int)(reqStrSize-8); ++j) {
		if ((reqStr[j] == 'r' || reqStr[j] == 'R')
			&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
			&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
			&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
			&& reqStr[j+4] == ':' && reqStr[j+5] == '/') {
				j += 6;
				if (reqStr[j] == '/') {
					// This is a "rtsp://" URL; skip over the host:port part that follows:
					++j;
					while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
				} else {
					// This is a "rtsp:/" URL; back up to the "/":
					--j;
				}
				i = j;
				break;
		}
	}

	// Look for the URL suffix (before the following "RTSP/"):
	parseSucceeded = false;
	for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k) {
		if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
			reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') {
				while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
				unsigned k1 = k;
				while (k1 > i && reqStr[k1] != '/') --k1;

				// ASSERT: At this point
				//   i: first space or slash after "host" or "host:port"
				//   k: last non-space before "RTSP/"
				//   k1: last slash in the range [i,k]

				// The URL suffix comes from [k1+1,k]
				// Copy "resultURLSuffix":
				unsigned n = 0, k2 = k1+1;
				if (k2 <= k) {
					if (k - k1 + 1 > resultURLSuffixMaxSize) return false; // there's no room
					while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
				}
				resultURLSuffix[n] = '\0';

				// The URL 'pre-suffix' comes from [i+1,k1-1]
				// Copy "resultURLPreSuffix":
				n = 0; k2 = i+1;
				if (k2+1 <= k1) {
					if (k1 - i > resultURLPreSuffixMaxSize) return false; // there's no room
					while (k2 <= k1-1) resultURLPreSuffix[n++] = reqStr[k2++];
				}
				resultURLPreSuffix[n] = '\0';
				decodeURL(resultURLPreSuffix);

				i = k + 7; // to go past " RTSP/"
				parseSucceeded = true;
				break;
		}
	}
	if (!parseSucceeded) return false;

	// Look for "CSeq:" (mandatory, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'CSeq':
	parseSucceeded = false;
	for (j = i; (int)j < (int)(reqStrSize-5); ++j) {
		if (_strncasecmp("CSeq:", &reqStr[j], 5) == 0) {
			j += 5;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned n;
			for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n') {
					parseSucceeded = true;
					break;
				}

				resultCSeq[n] = c;
			}
			resultCSeq[n] = '\0';
			break;
		}
	}
	if (!parseSucceeded) return false;

	// Look for "Session:" (optional, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'Session':
	resultSessionIdStr[0] = '\0'; // default value (empty string)
	for (j = i; (int)j < (int)(reqStrSize-8); ++j) {
		if (_strncasecmp("Session:", &reqStr[j], 8) == 0) {
			j += 8;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned n;
			for (n = 0; n < resultSessionIdStrMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n') {
					break;
				}

				resultSessionIdStr[n] = c;
			}
			resultSessionIdStr[n] = '\0';
			break;
		}
	}

	// Also: Look for "Content-Length:" (optional, case insensitive)
	contentLength = 0; // default value
	for (j = i; (int)j < (int)(reqStrSize-15); ++j) {
		if (_strncasecmp("Content-Length:", &(reqStr[j]), 15) == 0) {
			j += 15;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned num;
			if (sscanf(&reqStr[j], "%u", &num) == 1) {
				contentLength = num;
			}
		}
	}
	return true;
}

char const* dateHeader() {
	static char buf[200];
#if !defined(_WIN32_WCE)
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
#else
	// WinCE apparently doesn't have "time()", "strftime()", or "gmtime()",
	// so generate the "Date:" header a different, WinCE-specific way.
	// (Thanks to Pierre l'Hussiez for this code)
	// RSF: But where is the "Date: " string?  This code doesn't look quite right...
	SYSTEMTIME SystemTime;
	GetSystemTime(&SystemTime);
	WCHAR dateFormat[] = L"ddd, MMM dd yyyy";
	WCHAR timeFormat[] = L"HH:mm:ss GMT\r\n";
	WCHAR inBuf[200];
	DWORD locale = LOCALE_NEUTRAL;

	int ret = GetDateFormat(locale, 0, &SystemTime,
		(LPTSTR)dateFormat, (LPTSTR)inBuf, sizeof inBuf);
	inBuf[ret - 1] = ' ';
	ret = GetTimeFormat(locale, 0, &SystemTime,
		(LPTSTR)timeFormat,
		(LPTSTR)inBuf + ret, (sizeof inBuf) - ret);
	wcstombs(buf, inBuf, wcslen(inBuf));
#endif
	return buf;
}

void parseTransportHeader(char const* buf,
	StreamingMode& streamingMode,
	char*& streamingModeString,
	char*& destinationAddressStr,
	unsigned char& destinationTTL,
	unsigned short& clientRTPPortNum, // if UDP
	unsigned short& clientRTCPPortNum, // if UDP
	unsigned char& rtpChannelId, // if TCP
	unsigned char& rtcpChannelId // if TCP
	) {
		// Initialize the result parameters to default values:
		streamingMode = RTP_UDP;
		streamingModeString = NULL;
		destinationAddressStr = NULL;
		destinationTTL = 255;
		clientRTPPortNum = 0;
		clientRTCPPortNum = 1;
		rtpChannelId = rtcpChannelId = 0xFF;

		short p1, p2;
		unsigned ttl, rtpCid, rtcpCid;

		// First, find "Transport:"
		while (1) {
			if (*buf == '\0') return; // not found
			if (*buf == '\r' && *(buf+1) == '\n' && *(buf+2) == '\r') return; // end of the headers => not found
			if (_strncasecmp(buf, "Transport:", 10) == 0) break;
			++buf;
		}

		// Then, run through each of the fields, looking for ones we handle:
		char const* fields = buf + 10;
		while (*fields == ' ') ++fields;
		char* field = strDupSize(fields);
		while (sscanf(fields, "%[^;\r\n]", field) == 1) 
		{
			if (strcmp(field, "RTP/AVP/TCP") == 0) 
			{
				streamingMode = RTP_TCP;
			} 
			else if (strcmp(field, "RAW/RAW/UDP") == 0 || strcmp(field, "MP2T/H2221/UDP") == 0) 
			{
				streamingMode = RAW_UDP;
				streamingModeString = strDup(field);
			} 
			else if (_strncasecmp(field, "destination=", 12) == 0) 
			{
				delete[] destinationAddressStr;
				destinationAddressStr = strDup(field+12);
			} 
			else if (sscanf(field, "ttl%u", &ttl) == 1) 
			{
				destinationTTL = (unsigned char)ttl;
			} 
			else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) 
			{
				clientRTPPortNum = p1;
				clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p2; // ignore the second port number if the client asked for raw UDP
			} 
			else if (sscanf(field, "client_port=%hu", &p1) == 1) 
			{
				clientRTPPortNum = p1;
				clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
			}
			else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2)
			{
				rtpChannelId = (unsigned char)rtpCid;
				rtcpChannelId = (unsigned char)rtcpCid;
			}

			fields += strlen(field);
			while (*fields == ';' || *fields == ' ' || *fields == '\t') ++fields; // skip over separating ';' chars or whitespace
			if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
		}
		delete[] field;
}

static const char base64Char[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* base64Encode(char const* origSigned, unsigned origLength) {
	unsigned char const* orig = (unsigned char const*)origSigned; // in case any input bytes have the MSB set
	if (orig == NULL) return NULL;

	unsigned const numOrig24BitValues = origLength/3;
	bool havePadding = origLength > numOrig24BitValues*3;
	bool havePadding2 = origLength == numOrig24BitValues*3 + 2;
	unsigned const numResultBytes = 4*(numOrig24BitValues + havePadding);
	char* result = new char[numResultBytes+1]; // allow for trailing '\0'

	// Map each full group of 3 input bytes into 4 output base-64 characters:
	unsigned i;
	for (i = 0; i < numOrig24BitValues; ++i) {
		result[4*i+0] = base64Char[(orig[3*i]>>2)&0x3F];
		result[4*i+1] = base64Char[(((orig[3*i]&0x3)<<4) | (orig[3*i+1]>>4))&0x3F];
		result[4*i+2] = base64Char[((orig[3*i+1]<<2) | (orig[3*i+2]>>6))&0x3F];
		result[4*i+3] = base64Char[orig[3*i+2]&0x3F];
	}

	// Now, take padding into account.  (Note: i == numOrig24BitValues)
	if (havePadding) {
		result[4*i+0] = base64Char[(orig[3*i]>>2)&0x3F];
		if (havePadding2) {
			result[4*i+1] = base64Char[(((orig[3*i]&0x3)<<4) | (orig[3*i+1]>>4))&0x3F];
			result[4*i+2] = base64Char[(orig[3*i+1]<<2)&0x3F];
		} else {
			result[4*i+1] = base64Char[((orig[3*i]&0x3)<<4)&0x3F];
			result[4*i+2] = '=';
		}
		result[4*i+3] = '=';
	}
	result[numResultBytes] = '\0';
	return result;
}

bool  RGB2YUV(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len)  
{  
	int i, j;  
	unsigned char*bufY, *bufU, *bufV, *bufRGB,*bufYuv;  
	memset(yuvBuf,0,(unsigned int )*len);  
	bufY = yuvBuf;  
	bufV = yuvBuf + nWidth * nHeight;  
	bufU = bufV + (nWidth * nHeight* 1/4);  
	*len = 0;   
	unsigned char y, u, v, r, g, b,testu,testv;  
	unsigned int ylen = nWidth * nHeight;  
	unsigned int ulen = (nWidth * nHeight)/4;  
	unsigned int vlen = (nWidth * nHeight)/4; 

	for (j = 0; j<nHeight;j++)  
	{  
		bufRGB = RgbBuf + nWidth * (nHeight - 1 - j) * 3 ;  
		for (i = 0;i<nWidth;i++)  
		{  
			int pos = nWidth * i + j;  
			r = *(bufRGB++);  
			g = *(bufRGB++);  
			b = *(bufRGB++);  
			y = (unsigned char)( ( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16  ;            
			u = (unsigned char)( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128 ;            
			v = (unsigned char)( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128 ;  
			*(bufY++) = max( 0, min(y, 255 ));  
			if (j%2==0&&i%2 ==0)  
			{  
				if (u>255)  
				{  
					u=255;  
				}  
				if (u<0)  
				{  
					u = 0;  
				}  
				*(bufU++) =u;  
				//存u分量  
			}  
			else  
			{  
				//存v分量  
				if (i%2==0)  
				{  
					if (v>255)  
					{  
						v = 255;  
					}  
					if (v<0)  
					{  
						v = 0;  
					}  
					*(bufV++) =v;  
				}  
			}  
		}  
	}  
	*len = nWidth * nHeight+(nWidth * nHeight)/2 ;  
	return true;  
}   

bool YUV2RGB(LPBYTE yuvBuf,UINT nWidth,UINT nHeight,LPBYTE pRgbBuf,unsigned long *len)  
{  
#define PIXELSIZE nWidth * nHeight  
	const int Table_fv1[256]={ -180, -179, -177, -176, -174, -173, -172, -170, -169, -167, -166, -165, -163, -162, -160, -159, -158, -156, -155, -153, -152, -151, -149, -148, -146, -145, -144, -142, -141, -139, -138, -137, -135, -134, -132, -131, -130, -128, -127, -125, -124, -123, -121, -120, -118, -117, -115, -114, -113, -111, -110, -108, -107, -106, -104, -103, -101, -100, -99, -97, -96, -94, -93, -92, -90, -89, -87, -86, -85, -83, -82, -80, -79, -78, -76, -75, -73, -72, -71, -69, -68, -66, -65, -64, -62, -61, -59, -58, -57, -55, -54, -52, -51, -50, -48, -47, -45, -44, -43, -41, -40, -38, -37, -36, -34, -33, -31, -30, -29, -27, -26, -24, -23, -22, -20, -19, -17, -16, -15, -13, -12, -10, -9, -8, -6, -5, -3, -2, 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14, 15, 16, 18, 19, 21, 22, 23, 25, 26, 28, 29, 30, 32, 33, 35, 36, 37, 39, 40, 42, 43, 44, 46, 47, 49, 50, 51, 53, 54, 56, 57, 58, 60, 61, 63, 64, 65, 67, 68, 70, 71, 72, 74, 75, 77, 78, 79, 81, 82, 84, 85, 86, 88, 89, 91, 92, 93, 95, 96, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 114, 116, 117, 119, 120, 122, 123, 124, 126, 127, 129, 130, 131, 133, 134, 136, 137, 138, 140, 141, 143, 144, 145, 147, 148, 150, 151, 152, 154, 155, 157, 158, 159, 161, 162, 164, 165, 166, 168, 169, 171, 172, 173, 175, 176, 178 };  
	const int Table_fv2[256]={ -92, -91, -91, -90, -89, -88, -88, -87, -86, -86, -85, -84, -83, -83, -82, -81, -81, -80, -79, -78, -78, -77, -76, -76, -75, -74, -73, -73, -72, -71, -71, -70, -69, -68, -68, -67, -66, -66, -65, -64, -63, -63, -62, -61, -61, -60, -59, -58, -58, -57, -56, -56, -55, -54, -53, -53, -52, -51, -51, -50, -49, -48, -48, -47, -46, -46, -45, -44, -43, -43, -42, -41, -41, -40, -39, -38, -38, -37, -36, -36, -35, -34, -33, -33, -32, -31, -31, -30, -29, -28, -28, -27, -26, -26, -25, -24, -23, -23, -22, -21, -21, -20, -19, -18, -18, -17, -16, -16, -15, -14, -13, -13, -12, -11, -11, -10, -9, -8, -8, -7, -6, -6, -5, -4, -3, -3, -2, -1, 0, 0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 7, 8, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 17, 18, 19, 20, 20, 21, 22, 22, 23, 24, 25, 25, 26, 27, 27, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 37, 38, 39, 40, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 47, 48, 49, 50, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 57, 58, 59, 60, 60, 61, 62, 62, 63, 64, 65, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 72, 73, 74, 75, 75, 76, 77, 77, 78, 79, 80, 80, 81, 82, 82, 83, 84, 85, 85, 86, 87, 87, 88, 89, 90, 90 };  
	const int Table_fu1[256]={ -44, -44, -44, -43, -43, -43, -42, -42, -42, -41, -41, -41, -40, -40, -40, -39, -39, -39, -38, -38, -38, -37, -37, -37, -36, -36, -36, -35, -35, -35, -34, -34, -33, -33, -33, -32, -32, -32, -31, -31, -31, -30, -30, -30, -29, -29, -29, -28, -28, -28, -27, -27, -27, -26, -26, -26, -25, -25, -25, -24, -24, -24, -23, -23, -22, -22, -22, -21, -21, -21, -20, -20, -20, -19, -19, -19, -18, -18, -18, -17, -17, -17, -16, -16, -16, -15, -15, -15, -14, -14, -14, -13, -13, -13, -12, -12, -11, -11, -11, -10, -10, -10, -9, -9, -9, -8, -8, -8, -7, -7, -7, -6, -6, -6, -5, -5, -5, -4, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43 };  
	const int Table_fu2[256]={ -227, -226, -224, -222, -220, -219, -217, -215, -213, -212, -210, -208, -206, -204, -203, -201, -199, -197, -196, -194, -192, -190, -188, -187, -185, -183, -181, -180, -178, -176, -174, -173, -171, -169, -167, -165, -164, -162, -160, -158, -157, -155, -153, -151, -149, -148, -146, -144, -142, -141, -139, -137, -135, -134, -132, -130, -128, -126, -125, -123, -121, -119, -118, -116, -114, -112, -110, -109, -107, -105, -103, -102, -100, -98, -96, -94, -93, -91, -89, -87, -86, -84, -82, -80, -79, -77, -75, -73, -71, -70, -68, -66, -64, -63, -61, -59, -57, -55, -54, -52, -50, -48, -47, -45, -43, -41, -40, -38, -36, -34, -32, -31, -29, -27, -25, -24, -22, -20, -18, -16, -15, -13, -11, -9, -8, -6, -4, -2, 0, 1, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 21, 23, 24, 26, 28, 30, 31, 33, 35, 37, 39, 40, 42, 44, 46, 47, 49, 51, 53, 54, 56, 58, 60, 62, 63, 65, 67, 69, 70, 72, 74, 76, 78, 79, 81, 83, 85, 86, 88, 90, 92, 93, 95, 97, 99, 101, 102, 104, 106, 108, 109, 111, 113, 115, 117, 118, 120, 122, 124, 125, 127, 129, 131, 133, 134, 136, 138, 140, 141, 143, 145, 147, 148, 150, 152, 154, 156, 157, 159, 161, 163, 164, 166, 168, 170, 172, 173, 175, 177, 179, 180, 182, 184, 186, 187, 189, 191, 193, 195, 196, 198, 200, 202, 203, 205, 207, 209, 211, 212, 214, 216, 218, 219, 221, 223, 225 };  
	*len = 3* nWidth * nHeight;  
	if(!yuvBuf || !pRgbBuf)  
		return false;  
	const long nYLen = long(PIXELSIZE);  
	const int nHfWidth = (nWidth>>1);  
	if(nYLen<1 || nHfWidth<1)  
		return false;  
	// Y data  
	unsigned char* yData = yuvBuf;  
	// v data  
	unsigned char* vData = &yData[nYLen];  
	// u data  
	unsigned char* uData = &vData[nYLen>>2];  
	if(!uData || !vData)  
		return false;  
	int rgb[3];  
	int i, j, m, n, x, y, pu, pv, py, rdif, invgdif, bdif;  
	m = -nWidth;  
	n = -nHfWidth;  
	bool addhalf = true;  
	for(y=0; y<nHeight;y++) {  
		m += nWidth;  
		if( addhalf ){  
			n+=nHfWidth;  
			addhalf = false;  
		} else {  
			addhalf = true;  
		}  
		for(x=0; x<nWidth;x++)  {  
			i = m + x;  
			j = n + (x>>1);  
			py = yData[i];  
			// search tables to get rdif invgdif and bidif  
			rdif = Table_fv1[vData[j]];    // fv1  
			invgdif = Table_fu1[uData[j]] + Table_fv2[vData[j]]; // fu1+fv2  
			bdif = Table_fu2[uData[j]]; // fu2       
			rgb[0] = py+rdif;    // R  
			rgb[1] = py-invgdif; // G  
			rgb[2] = py+bdif;    // B  
			j = nYLen - nWidth - m + x;  
			i = (j<<1) + j;  
			// copy this pixel to rgb data  
			for(j=0; j<3; j++)  
			{  
				if(rgb[j]>=0 && rgb[j]<=255){  
					pRgbBuf[i + j] = rgb[j];  
				}  
				else{  
					pRgbBuf[i + j] = (rgb[j] < 0)? 0 : 255;  
				}  
			}  
		}  
	}  
	return true;  
} 