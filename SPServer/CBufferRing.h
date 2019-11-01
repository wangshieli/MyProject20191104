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
	DWORD readData(TCHAR* _buf, DWORD _dwCount);
	int getStart()
	{
		return dwReadPos;
	}
	int getEnd()
	{
		return dwWritePos;
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

