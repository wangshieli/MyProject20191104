#pragma once
class CBufferRing
{
public:
	CBufferRing();
	~CBufferRing();

public:
	void Init(DWORD _bufsize);
	DWORD writeData(TCHAR* _buf, DWORD _dwbufsize);
	DWORD readData(SOCKET s);

private:
	TCHAR *buf;
	DWORD dwBufSize;
	DWORD dwWritePos;
	DWORD dwReadPos;
	BOOL bEmpty;
	BOOL bFull;
	CRITICAL_SECTION cs;
};

