#pragma once
class CBufferRing
{
public:
	CBufferRing(DWORD _bufsize);
	~CBufferRing();

private:
	TCHAR *buf;
	DWORD dwBufSize;
	DWORD dwWritePos;
	DWORD dwReadPos;
	DWORD dwRightSize;
	DWORD dwLeftSize;
	BOOL bEempty;
	BOOL bFull;
};

