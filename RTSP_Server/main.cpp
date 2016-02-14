#include <iostream>
using namespace std;
#include <stdio.h>
#include "rtsp_server_api.h"
#pragma comment(lib,"libRtspServer.lib")
#define FRAME_SIZE 100*1000
int main()
{
	char *pBuff = new char[FRAME_SIZE];
	FILE* pFile = NULL;
	FILE* pWfile = fopen("test.264","wb");
	int total = 0;
	for(int i = 0; i < 101; i++)
	{
		memset(pBuff,0,FRAME_SIZE);
		char fileName[100] = {0};
		sprintf(fileName,"E:\\X264BlockData\\AAFF_%d.dat",i);
		pFile = fopen(fileName,"rb");
		int readSize = fread(pBuff,1,FRAME_SIZE,pFile);
		total+=readSize;
		cout<<"alread write "<<readSize<<" bytes"<<endl;
		fwrite(pBuff,1,readSize,pWfile);
		fclose(pFile);
	}
	fclose(pWfile);

	cout<<"Totaly write "<<total<<" bytes"<<endl;
	char c = getchar();
	return 0;
}