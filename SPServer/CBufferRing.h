#pragma once
class CBufferRing
{
public:
	CBufferRing(DWORD _bufsize);
	~CBufferRing();

public:
	void lock();
	void unlock();
	BOOL isFull();
	BOOL isEmpty();
	void empty();
	DWORD writeData(TCHAR* _buf, DWORD _dwbufsize);
	DWORD readData(SOCKET s);
	DWORD getStart()
	{
		return dwReadPos;
	}
	DWORD getEnd()
	{
		return dwWritePos;
	}
	DWORD GetLength()
	{

	}

private:
	TCHAR *buf;
	DWORD dwBufSize;
	DWORD dwWritePos;
	DWORD dwReadPos;
	BOOL bEempty;
	BOOL bFull;
	CRITICAL_SECTION cs;
};

