#include "pch.h"
#include "CBufferRing.h"


CBufferRing::CBufferRing(DWORD _bufsize):dwBufSize(_bufsize)
	, dwWritePos(0)
	, dwReadPos(0)
	, bEempty(TRUE)
	, bFull(FALSE)
{
	InitializeCriticalSection(&cs);
	buf = (TCHAR*)malloc(dwBufSize);
}


CBufferRing::~CBufferRing()
{
	free(buf);
	DeleteCriticalSection(&cs);
}

void CBufferRing::lock()
{
	EnterCriticalSection(&cs);
}

void CBufferRing::unlock()
{
	LeaveCriticalSection(&cs);
}

BOOL CBufferRing::isFull()
{
	return 0;
}

BOOL CBufferRing::isEmpty()
{
	return 0;
}

void CBufferRing::empty()
{
}

DWORD CBufferRing::writeData(TCHAR * _data, DWORD _datalen)
{
	if (_datalen <= 0)
		return 0;
	if (bFull)
		return 0;

	bEempty = FALSE;
	if (dwReadPos == dwWritePos)
	{
		// 缓冲区为空
		DWORD dwLeftSize = dwBufSize - dwWritePos;
		if (dwLeftSize > _datalen)
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, _datalen);
			dwWritePos += _datalen;
			bFull = (dwWritePos == dwReadPos);
			return _datalen;
		}
		else
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, dwLeftSize);
			dwWritePos = (dwReadPos > (_datalen - dwLeftSize) ? (_datalen - dwLeftSize) : dwReadPos);
			memcpy_s(buf, dwWritePos, _data + dwLeftSize, dwWritePos);
			bFull = (dwWritePos == dwReadPos);
			return dwLeftSize + dwWritePos;
		}
	}
	else if (dwReadPos < dwWritePos)
	{
		// 顺序
		DWORD dwLeftSize = dwBufSize - dwWritePos;
		if (dwLeftSize > _datalen)
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, _datalen);
			dwWritePos += _datalen;
			bFull = (dwWritePos == dwReadPos);
			return _datalen;
		}
		else
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, dwLeftSize);
			dwWritePos = (dwReadPos > (_datalen - dwLeftSize) ? (_datalen - dwLeftSize) : dwReadPos);
			memcpy_s(buf, dwWritePos, _data + dwLeftSize, dwWritePos);
			bFull = (dwWritePos == dwReadPos);
			return dwLeftSize + dwWritePos;
		}
	}
	else /*if (dwReadPos > dwWritePos)*/
	{
		// 跨区间写
		DWORD dwLeftSize = dwBufSize - dwWritePos;
		if (dwLeftSize > _datalen)
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, _datalen);
			dwWritePos += _datalen;
			bFull = (dwWritePos == dwReadPos);
			return _datalen;
		}
		else
		{
			memcpy_s(buf + dwWritePos, dwLeftSize, _data, dwLeftSize);
			dwWritePos += dwLeftSize;// 考虑发送完整问题
			//dwWritePos = (dwReadPos > (_datalen - dwLeftSize) ? (_datalen - dwLeftSize) : dwReadPos);
			//memcpy_s(buf, dwWritePos, _data + dwLeftSize, dwWritePos);
			bFull = (dwWritePos == dwReadPos);
			return dwLeftSize + dwWritePos;
		}
	}

	return 0;
}

DWORD CBufferRing::readData(TCHAR * _buf, DWORD _dwCount)
{
	bFull = FALSE;
	if (bEempty)
		return 0;

	if (dwReadPos == dwWritePos)
	{

	}
	else if (dwReadPos < dwWritePos)
	{

	}
	else /*if (dwReadPos > dwWritePos)*/
	{
		
	}
	return 0;
}
