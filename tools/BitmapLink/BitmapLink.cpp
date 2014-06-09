// BitmapLink.cpp : Defines the entry point for the console application.
//

/**
 *  BitmapLink - ��������� ��� ���������� ������������������
 *               bmp ������ � ���� ��� ������������� � ��������
 *  version 1.1  ������ ������ ����. ������� ����� � �������. ��������� ������.
 *  version 2.0  ��������� ������� tga.
**/


#include <windows.h>
#include <stdio.h>
#include <math.h>


int frameW = 0, frameH = 0;


struct tga_header
{
	char   IdLeight;	//   ����� ��������� ���������� ����� ������� 
	char   ColorMap;	//   ������������� ������� �������� ����� - ������� 
	char   DataType;	//   ��� ������ - ������������ ��� ��� 
	char   ColorMapInfo[5];	//   ���������� � �������� ����� - ����� ���������� ��� 5 ���� 
	short  x_origin;	//   ������ ����������� �� ��� X 
	short  y_origin;	//   ������ ����������� �� ��� Y 
	short  width;		//   ������ ����������� 
	short  height;		//   ������ ����������� 
	char   BitPerPel;	//   ���-�� ��� �� ������� - ����� ������ 24 ��� 32 
	char   Description;	//   �������� - ����������� 
} header;

bool ReadBitmap(char* cFileName, RGBQUAD **pBits)
{
	HANDLE	hBMP_File = CreateFile(	cFileName, 
									GENERIC_READ, 
									FILE_SHARE_READ, 
									NULL, 
									OPEN_EXISTING, 
									FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
									NULL);
	
	DWORD nBytesRead = 0;

	ReadFile(hBMP_File, &header, sizeof(header), &nBytesRead, NULL );
	
	frameW = header.width;
	frameH = header.height;
	
	if ( header.BitPerPel != 32 ) 
	{
		CloseHandle(hBMP_File);
		printf("only 32 bit tga are supported\n");
		return false;
	}
	
	*pBits = new RGBQUAD[ header.width * header.height ];	
	
	SetFilePointer(hBMP_File, header.IdLeight, NULL, FILE_CURRENT);
	ReadFile(hBMP_File, *pBits, header.width * header.height * sizeof(RGBQUAD), &nBytesRead, NULL );
	
	CloseHandle(hBMP_File);

	return true;
}

BOOL SelectFile (char *pFileName) 
{
	OPENFILENAME of = { sizeof(OPENFILENAME) };

	of.lpstrFilter = "Targa Image Files (*.tga)\0*.tga\0\0";
	of.lpstrDefExt = "tga";
	of.lpstrFile   = pFileName;
	of.nMaxFile    = 16384;
	of.lpstrTitle  = "������� ��������";
	of.Flags       = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_ALLOWMULTISELECT;
	
	return GetOpenFileName (&of);	
}


int main(int argc, char* argv[])
{
	//���������� ������ �������� ������
	char pFileName[16384] = {0};
	if ( !SelectFile(pFileName) ) 
	{
		printf("canceled\n");
		return 0;
	}
	printf("%s\n", pFileName);

	//������� ���������� ������ � ������
	int nFrames = 0;
	for(int n = 0; pFileName[n]; n++)
	{
		if (pFileName[n] == ' ') 
			nFrames++;
	}

	if (0 == nFrames)
	{
		printf("there are no files selected\n");
		return -1;
	}


	char *pNextFile = strstr(pFileName, " ");

	//���������� ����������
	char *dir = pFileName;
	*pNextFile = '\0';
	int res = SetCurrentDirectory(dir);
	CharToOem(dir, dir);
	printf("Current directory is '%s'\n", dir);


	// ����������� ���-�� ������ �� X � �� Y
	int n_frames_x = int( sqrt((double) nFrames) ) - 1;
	int n_frames_y;
	do {
		n_frames_y = nFrames / (++n_frames_x);
	} while (n_frames_x * n_frames_y != nFrames);


	//����� ���������� �����������
	RGBQUAD *OutBits = NULL;

	//������ ������
	for (int nCurFrame = 0; pNextFile; nCurFrame++)
	{
		///////////////////////////////////////////////////////
		//�������� ��������� ��� �����
		char *pCurFile = pNextFile + 1;
		pNextFile = strstr(pCurFile, " ");
		if (pNextFile) *pNextFile = '\0';

		//����� �����
		RGBQUAD *pFrameBits = NULL;

		//������ � ����� ��������
		if (!ReadBitmap(pCurFile, &pFrameBits))
		{
			fprintf(stderr, "error reading '%s'\n", pCurFile);
			break;
		}
	
		if (NULL == OutBits)
			OutBits = new RGBQUAD[frameH*frameW*nFrames];

		//������������ ���� � �������� �����
		for(int y = 0; y < frameH; y++)
		for(int x = 0; x < frameW; x++)
		{
			int cur_frame_x = nCurFrame % n_frames_x;
			int cur_frame_y = nCurFrame / n_frames_x;

			OutBits[cur_frame_x * frameW + x + ((n_frames_y - cur_frame_y - 1) * frameH + y) * (frameW * n_frames_x)] = 
				pFrameBits[x + y * frameW];
		}

		delete[] pFrameBits;


		char str[MAX_PATH + 64];
		sprintf(str, "frame#%d = '%s'\n", nCurFrame, pCurFile);
		CharToOem(str, str);
		printf(str);
	}

	//////////////////////////////////////////////////////////////////////////
	//����� ���������� ����������� � ����

	pFileName[0] = '\0';
	OPENFILENAME ofn = { sizeof(OPENFILENAME) };
	ofn.lpstrFilter = "Targa Image Files (*.tga)\0*.tga\0\0";
	ofn.lpstrDefExt = "tga";
	ofn.lpstrFile   = pFileName;
	ofn.nMaxFile    = MAX_PATH;
	ofn.lpstrTitle  = "Save as";
	ofn.Flags       = OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn))
	{
		HANDLE	hBMP_File = CreateFile(	pFileName, 
										GENERIC_WRITE, 
										FILE_SHARE_READ, 
										NULL, 
										CREATE_ALWAYS, 
										FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
										NULL);
		DWORD nBytesWritten = 0;
		
		header.width  = frameW * n_frames_x;
		header.height = frameH * n_frames_y;
		header.IdLeight = 0;

		WriteFile(hBMP_File, &header, sizeof(header), &nBytesWritten, NULL );
		WriteFile(hBMP_File, OutBits, header.width*header.height*sizeof(RGBQUAD), &nBytesWritten, NULL );
	
		CloseHandle(hBMP_File);
	}
	delete[] OutBits;

	return 0;
}
