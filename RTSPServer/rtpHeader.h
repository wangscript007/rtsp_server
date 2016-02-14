typedef unsigned char RTPuint8;
typedef unsigned short RTPuint16;
typedef unsigned long RTPuint32;
typedef unsigned long long RTPuint64;

#define RTP_CLOCK_DURATION 90000
#define RTP_PACKAGE_MAX_SIZE 14000
#define RTP_HEADER_SIZE 12

typedef struct tagRTPHeader
{
	//unsigned char cc:4;
	//unsigned char extension:1;
	//unsigned char padding:1;
	//unsigned char version:2;
	//unsigned char payloadtype:7;
	//unsigned char marker:1;
	//RTPuint16 seqnum;
	RTPuint32 rtpHdr;
	RTPuint32 timestamp;
	RTPuint32 ssrc;
}RTPHeader,*pRTPHeader;

typedef struct {  
	//byte 0  
	unsigned char TYPE:5;  
	unsigned char NRI:2;  
	unsigned char F:1;      

} NALU_HEADER; /**//* 1 BYTES */ 

typedef struct {  
	//byte 0  
	unsigned char TYPE:5;  
	unsigned char NRI:2;   
	unsigned char F:1;      


} FU_INDICATOR; /**//* 1 BYTES */  

typedef struct {  
	//byte 0  
	unsigned char TYPE:5;  
	unsigned char R:1;  
	unsigned char E:1;  
	unsigned char S:1;      
} FU_HEADER; /**//* 1 BYTES */  