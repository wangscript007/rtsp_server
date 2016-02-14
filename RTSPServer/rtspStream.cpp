#include "rtspStream.h"

rtspStream::rtspStream(int ID,int width,int height,int fps,int bitrate,rtspStreamSendData& lpCallback)
	:m_nStreamID(ID)
	,m_nStreamPort(12000+ID)
	,estBitrate(500)
	,fSDPLines(NULL)
	,fTrackId(NULL)
	,m_nSSRC(ID)
	,fSeqNo(0)
	,fRTPPayloadType(96)
	,m_hSendThread(NULL)
	,m_nTimeStamp(0)
	,m_nWidth(width)
	,m_nHeight(height)
{
	lpCallback = SendData;
	PYUVBuf = new unsigned char[width*height+(width * height)];
	pLoadBuf = new unsigned char[width*height+(width * height)]; 
	initH264Encoder(width,height,fps,bitrate);
	initStream();
	m_pRtpHeader = new RTPHeader;
	m_pSendData = new unsigned char[RTP_PACKAGE_MAX_SIZE+RTP_HEADER_SIZE +2];

	m_hSendThread = CreateThread(NULL,NULL,InitSendThread,this,NULL,NULL);
	m_hExitEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hDelIPEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	m_hSendEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&cs);
}

rtspStream::~rtspStream(void)
{
	SetEvent(m_hExitEvent);
	delete PYUVBuf;
	delete pLoadBuf;
	delete m_pRtpHeader;
	delete m_pSendData;
}

char* rtspStream::generateSDPDescription()
{
	int ipAddressStrSize = strlen(inet_ntoa(sockaddr.sin_addr));

	char* sdp = NULL; // for now
	unsigned sdpLength = 0;
	char* sdpLines = GetSdpLines();
	if (sdpLines == NULL)
		return NULL;
	sdpLength += strlen(sdpLines);

	char const* const sdpPrefixFmt =
		"v=0\r\n"
		"o=- %ld%06ld %d IN IP4 %s\r\n"
		"s=Media Presentation\r\n"
		"i=%s\r\n"
		"t=0 0\r\n"
		"a=type:broadcast\r\n"
		"a=control:*\r\n"
		"%s";

	sdpLength += strlen(sdpPrefixFmt)
		+ 20 + 6 + 20 + ipAddressStrSize
		+ strlen(sdpLines);
	sdpLength += 1000; // in case the length of the "subsession->sdpLines()" calls below change
	sdp = new char[sdpLength];
	if (sdp == NULL) return NULL;

	// Generate the SDP prefix (session-level lines):
	snprintf(sdp, sdpLength, sdpPrefixFmt,
		0, 0, // o= <session id>
		1, // o= <version> // (needs to change if params are modified)
		inet_ntoa(sockaddr.sin_addr), // o= <address>
		trackId(),
		sdpLines); // miscellaneous session SDP lines (if any)

	return sdp;
}

char* rtspStream::GetSdpLines()
{
	if (fSDPLines != NULL ) 
		return fSDPLines;

	char* rtpmapLine = "a=rtpmap: 96 H264/90000";

	char const* const sdpFmt =
		"m=video 0 RTP/AVP 96\r\n"
		"c=IN IP4 0.0.0.0\r\n"
		"b=AS:%u\r\n"
		"%s\r\n"
		"a=control:%s\r\n";
	char * tmp = inet_ntoa(sockaddr.sin_addr);
	unsigned sdpFmtSize = strlen(sdpFmt)
		+ strlen(tmp) + 3 /* max char len */
		+ 20 /* max int len */
		+ strlen(rtpmapLine)
		+ strlen(trackId());

	char* sdpLines = new char[sdpFmtSize];
	sprintf(sdpLines, sdpFmt,
		estBitrate, // b=AS:<bandwidth>
		rtpmapLine, // a=rtpmap:... (if present)
		trackId()); // a=control:<track-id>

	fSDPLines = strDup(sdpLines);
	delete[] sdpLines;

	return fSDPLines;
}

float rtspStream::duration()
{
	// it is presentation so return -1.0
	return -1.0;
}

char* rtspStream::trackId() 
{
	if (m_nStreamID == 0) return NULL; 

	if (fTrackId == NULL) {
		char buf[100];
		sprintf(buf, "%d", m_nStreamID);
		fTrackId = strDup(buf);
	}
	return fTrackId;
}

bool rtspStream::initStream()
{
	// using UDP protocol
	m_SendSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(m_nStreamPort);
	sockaddr.sin_addr.s_addr  = inet_addr(TEST_SERVREIP);
	m_nStreamRTCPPort = m_nStreamPort+1;
	m_nStreamRTPPort = m_nStreamPort;
	bind(m_SendSocket,(struct sockaddr *)&sockaddr,sizeof(sockaddr));
	return true;
}

bool rtspStream::AddSessionIP(unsigned addr,RtspSessionInfo* info)
{
	EnterCriticalSection(&cs);
	m_SessionList[addr] = info;
	LeaveCriticalSection(&cs);
	return true;
}

bool rtspStream::DelSessionIP(unsigned addr)
{
	auto i = m_SessionList.find(addr);
	if (i != m_SessionList.end())
	{
		i->second->state = -1;
		SetEvent(m_hDelIPEvent);
	}
	return true;
}

bool rtspStream::playSession(unsigned addr)
{
	auto i = m_SessionList.find(addr);
	if (i != m_SessionList.end())
	{
		i->second->state = 1;
	}
	return true;
}

bool rtspStream::pauseSession(unsigned addr)
{
	auto i = m_SessionList.find(addr);
	if (i != m_SessionList.end())
	{
		i->second->state = 0;
	}
	return true;
}

unsigned short rtspStream::GetRtpSeqNum()
{
	return fSeqNo;
}

unsigned int rtspStream::GetRtpTimestamp()
{
	return 0;//m_nTimeStamp;
}

void rtspStream::PrepareRTPHeader()
{
	unsigned rtpHdr = 0x80000000; // RTP version 2; marker ('M') bit not set (by default; it can be set later)
	rtpHdr |= (fRTPPayloadType<<16);
	rtpHdr |= fSeqNo++; // sequence number
	m_pRtpHeader->rtpHdr = htonl(rtpHdr);
	m_pRtpHeader->timestamp = htonl(m_nTimeStamp);
	m_pRtpHeader->ssrc = m_nSSRC;
}

void rtspStream::GetOneFrame()
{
	// when get one frame
	if (m_SessionList.size() < 1)	return;
	char tmp[100] = {0};
	//the number of frame needs added 1
	frame_num ==0 ? pPicIn->i_pts =frame_num++ : pPicIn->i_pts =frame_num++ - 1;

	//encode 
	int iNal = 0;
	int dataSize = 0;
	unsigned char* pData;
	if( x264_encoder_encode(pX264Handle,&pNals,&iNal,pPicIn,pPicOut) >0)
	{

		for (int j = 0; j < iNal; ++j)
		{
			dataSize = pNals[j].i_payload;
			pData = pNals[j].p_payload;
			//h264 NALU header's bit offset:	if key frame the bitoffset is 3,otherwise,4
			int bitoffset = pNals[j].i_type != 5 ? 4 : 3;
			dataSize -=bitoffset;
			pData+=bitoffset;
			
			sprintf(tmp,"the nalu data size is %d\tthe nalu type is %d\n",dataSize,pNals[j].i_type);
			OutputDebugString(tmp);
			// get RTP Header for one NALU
			if (dataSize <RTP_PACKAGE_MAX_SIZE)
			{
				PrepareRTPHeader();
				int size =  dataSize;

				memset(m_pSendData,0,RTP_PACKAGE_MAX_SIZE+RTP_HEADER_SIZE);
				memcpy(m_pSendData,m_pRtpHeader,sizeof(RTPHeader));
				memcpy(m_pSendData+RTP_HEADER_SIZE,(const char*)pData, size);
				int sendsize = 0;
				//send data
				for (auto i = m_SessionList.begin(); i != m_SessionList.end(); i++)
				{
					if (i->second->state == 1)
					{
						sendsize = \
							sendto(m_SendSocket,(const char*)m_pSendData,size+RTP_HEADER_SIZE,0, (SOCKADDR *) &i->second->clientAddr,sizeof(i->second->clientAddr));	 

						sprintf(tmp,"send size is %d\n",sendsize);
						OutputDebugString(tmp);
					}
				}
			}
			else
			{
				unsigned char header1 = pData[0];
				unsigned char header2 = NULL;
				pData++;
				int k = 0, last = 0; 
				k = dataSize / (RTP_PACKAGE_MAX_SIZE) ;
				last = dataSize % RTP_PACKAGE_MAX_SIZE;
				int t = 0;//����ָʾ��ǰ���͵��ǵڼ�����ƬRTP�� 
				int sendSize = 0;
				unsigned char fu_ind;
				unsigned char fu_hdr;
				while(t <= k)
				{
					PrepareRTPHeader(); 
					if(!t)//����һ����Ҫ��Ƭ��NALU�ĵ�һ����Ƭ����FU HEADER��Sλ,t = 0ʱ������߼���  
					{  
						fu_ind = (header1 & 0xE0) | 28;
						fu_hdr = 0x80 | (header1& 0x1F);
						header1 = fu_ind;
						header2 = fu_hdr;

						sendSize = RTP_PACKAGE_MAX_SIZE+RTP_HEADER_SIZE +2;
					}  
					//�Ȳ��ǵ�һ����Ƭ��Ҳ�������һ����Ƭ�Ĵ���  
					else if(t < k && 0 != t)  
					{  
						fu_ind = header1; // FU indicator
						fu_hdr = header2&~0x80  ; // FU header (no S bit)
						sendSize = RTP_PACKAGE_MAX_SIZE+RTP_HEADER_SIZE + 2;
					} 
					//����һ����Ҫ��Ƭ��NALU�ķǵ�һ����Ƭ������FU HEADER��Sλ������÷�Ƭ�Ǹ�NALU�����һ����Ƭ����FU HEADER��Eλ  
					else if(k == t )//���͵������һ����Ƭ��ע�����һ����Ƭ�ĳ��ȿ��ܳ���1400�ֽڣ��� l> 1386ʱ����  
					{
						fu_ind = header1; // FU indicator
						fu_hdr = header2&~0x80 ; // FU header (no S bit)
						fu_hdr |= 0x40;
						sendSize = last   +RTP_HEADER_SIZE + 2;
					}  
					
					memset(m_pSendData,0,RTP_PACKAGE_MAX_SIZE+RTP_HEADER_SIZE + 2);
					memcpy(m_pSendData,m_pRtpHeader,sizeof(RTPHeader));
					memcpy(m_pSendData+RTP_HEADER_SIZE,&fu_ind,1);
					memcpy(m_pSendData+RTP_HEADER_SIZE+1,&fu_hdr,1);
					memcpy(m_pSendData+RTP_HEADER_SIZE+2,(const char*)pData,  sendSize - 14);
					int sendsize1 = 0;
					//send data
					for (auto i = m_SessionList.begin(); i != m_SessionList.end(); i++)
					{
						if (i->second->state == 1)
						{
							sendsize1 = \
								sendto(m_SendSocket,(const char*)m_pSendData,sendSize,0, (SOCKADDR *) &i->second->clientAddr,sizeof(i->second->clientAddr));	 

							sprintf(tmp,"send size is %d\n",sendsize1);
							OutputDebugString(tmp);
						}
					}
					pData+=sendSize-14;
					t++;  
				}
			}
			//timestamp adds one duration
			m_nTimeStamp += RTP_CLOCK_DURATION/m_nFPS;
		}
	}

}

void rtspStream::initH264Encoder(int width,int height,int fps,int bitRate)
{
	frame_num = 0; 
	pX264Handle   = NULL;
	pX264Param = new x264_param_t;
	assert(pX264Param);
	m_nFPS = 25;
	//* ���ò���
	//* ʹ��Ĭ�ϲ�������������Ϊ�ҵ���ʵʱ���紫�䣬������ʹ����zerolatency��ѡ�ʹ�����ѡ��֮��Ͳ�����delayed_frames�������ʹ�õĲ��������Ļ�������Ҫ�ڱ������֮��õ�����ı���֡
	x264_param_default_preset(pX264Param, "veryfast", "zerolatency");
	//* cpuFlags
	pX264Param->i_threads  = X264_SYNC_LOOKAHEAD_AUTO;//* ȡ�ջ���������ʹ�ò������ı�֤.
	//* ��Ƶѡ��
	pX264Param->i_width   = width; //* Ҫ�����ͼ����.
	pX264Param->i_height  = height; //* Ҫ�����ͼ��߶�
	pX264Param->i_frame_total = 0; //* ������֡��.��֪����0.
	pX264Param->i_keyint_max = 10; 
	//* ������
	pX264Param->i_bframe  = 5;
	pX264Param->b_open_gop  = 0;
	pX264Param->i_bframe_pyramid = 0;
	pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
	//* Log����������Ҫ��ӡ������Ϣʱֱ��ע�͵�����
	// pX264Param->i_log_level  = X264_LOG_DEBUG;
	//* ���ʿ��Ʋ���
	pX264Param->rc.i_bitrate = bitRate;//* ����(������,��λKbps)
	//* muxing parameters
	pX264Param->i_fps_den  = 1; //* ֡�ʷ�ĸ
	pX264Param->i_fps_num  = fps;//* ֡�ʷ���
	pX264Param->i_timebase_den = pX264Param->i_fps_num;
	pX264Param->i_timebase_num = pX264Param->i_fps_den;
	//* ����Profile.ʹ��Baseline profile
	x264_param_apply_profile(pX264Param, x264_profile_names[0]);

	pNals = NULL;
	pPicIn = new x264_picture_t;
	pPicOut = new x264_picture_t;
	x264_picture_init(pPicOut);
	x264_picture_alloc(pPicIn, X264_CSP_I420, pX264Param->i_width, pX264Param->i_height);
	pPicIn->img.i_csp = X264_CSP_I420;
	pPicIn->img.i_plane = 3;
	//* �򿪱��������,ͨ��x264_encoder_parameters�õ����ø�X264
	//* �Ĳ���.ͨ��x264_encoder_reconfig����X264�Ĳ���
	pX264Handle = x264_encoder_open(pX264Param);
	assert(pX264Handle);

	pPicIn->img.plane[0] = PYUVBuf;
	pPicIn->img.plane[1] = PYUVBuf + width *height;
	pPicIn->img.plane[2] = PYUVBuf + width * height * 5 / 4;
	pPicIn->img.plane[3] = 0;
}

DWORD rtspStream::SendThread(LPVOID lpParam)
{
	HANDLE pHandle[3] = {m_hExitEvent,m_hDelIPEvent,m_hSendEvent};
	while (1)
	{
		switch(WaitForMultipleObjects(3,pHandle,FALSE,1000/m_nFPS))
		{
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_OBJECT_0+1:
			ResetEvent(m_hDelIPEvent);
			for (auto i = m_SessionList.begin();i != m_SessionList.end();)
			{
				if (i->second->state == -1 ) m_SessionList.erase(i++);				
				else i++;
			}
			break;
		case WAIT_OBJECT_0+2:
			ResetEvent(m_hSendEvent);
			EnterCriticalSection(&cs);
			memcpy(PYUVBuf,pLoadBuf,m_nWidth*m_nHeight * 2);
			LeaveCriticalSection(&cs);
			GetOneFrame();
			break;
		default:
			GetOneFrame();
			break;
		}
	}
	return 0;
}

void rtspStream::SendData(unsigned char* data ,int size,int type,RTSPStreamHandle lpParam)//type:0-RGB;1-YUV
{
	rtspStream* pStream = (rtspStream*)lpParam;
	unsigned long ul =pStream->m_nWidth * pStream->m_nHeight *1.5;
	switch(type)
	{
	case 0:
		//RGB data needs to be translated into YUV data
		RGB2YUV(data,pStream->m_nWidth,pStream->m_nHeight,pStream->pLoadBuf,&ul);
		break;
	case 1:
		// YUV data
		memcpy(pStream->pLoadBuf,data,size);
		break;
	}
	SetEvent(pStream->m_hSendEvent);
	//pStream->GetOneFrame();
}