#include "pch.h"
#include "CBufferRing.h"


CBufferRing::CBufferRing():dwWritePos(0)
	, dwReadPos(0)
	, bEmpty(TRUE)
	, bFull(FALSE)
{
	InitializeCriticalSection(&cs);
}


CBufferRing::~CBufferRing()
{
	free(buf);
	DeleteCriticalSection(&cs);
}

void CBufferRing::Init(DWORD _bufsize)
{
	dwBufSize = _bufsize;
	buf = (TCHAR*)malloc(dwBufSize);
}

DWORD CBufferRing::writeData(TCHAR * _data, DWORD _datalen)
{
	if (_datalen <= 0)
		return 0;

	DWORD dwWriteCounts = 0;

	EnterCriticalSection(&cs);
	do
	{
		if (bFull)
			break;

		bEmpty = FALSE;
		if (dwReadPos <= dwWritePos)
		{
			// 缓冲区为空
			DWORD dwLeftSize = dwBufSize - dwWritePos;
			if (dwLeftSize > _datalen)
			{
				memcpy_s(buf + dwWritePos, dwLeftSize, _data, _datalen);
				dwWritePos += _datalen;
				bFull = (dwWritePos == dwReadPos);
				dwWriteCounts = _datalen;
				break;
			}
			else
			{
				memcpy_s(buf + dwWritePos, dwLeftSize, _data, dwLeftSize);
				dwWritePos = (dwReadPos > (_datalen - dwLeftSize) ? (_datalen - dwLeftSize) : dwReadPos);
				memcpy_s(buf, dwWritePos, _data + dwLeftSize, dwWritePos);
				bFull = (dwWritePos == dwReadPos);
				dwWriteCounts = dwLeftSize + dwWritePos;
				break;
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
				dwWriteCounts = _datalen;
				break;
			}
			else
			{
				memcpy_s(buf + dwWritePos, dwLeftSize, _data, dwLeftSize);
				dwWritePos += dwLeftSize;// 考虑发送完整问题
				//dwWritePos = (dwReadPos > (_datalen - dwLeftSize) ? (_datalen - dwLeftSize) : dwReadPos);
				//memcpy_s(buf, dwWritePos, _data + dwLeftSize, dwWritePos);
				bFull = (dwWritePos == dwReadPos);
				dwWriteCounts = dwLeftSize + dwWritePos;
				break;
			}
		}
	} while (FALSE);
	LeaveCriticalSection(&cs);

	return dwWriteCounts;
}

DWORD CBufferRing::readData(SOCKET s)
{
	int nSendBytes = 0;

	EnterCriticalSection(&cs);
	do
	{
		if (bEmpty)
			break;

		bFull = false; // 状态需要在有数据发送了之后设为false？？？
		if (dwReadPos < dwWritePos)
		{
			DWORD datelen = dwWritePos - dwReadPos;
			//memcpy(_buf, buf + dwReadPos, _dwCount);
			nSendBytes = send(s, buf + dwReadPos, datelen, 0);
			if (SOCKET_ERROR == nSendBytes)
			{
				if (WSAEWOULDBLOCK != WSAGetLastError())
				{
					// 错误处理
				}
			}
			//dwReadPos += _dwCount;
			dwReadPos += nSendBytes;
			bEmpty = (dwReadPos == dwWritePos);
			break;
		}
		else if (dwReadPos >= dwWritePos)
		{
			DWORD datelen = dwBufSize - (dwReadPos - dwWritePos);
			TCHAR* sendbuf = (TCHAR*)malloc(datelen);
			DWORD dwRight = dwBufSize - dwReadPos;
			memcpy_s(sendbuf, datelen, buf + dwReadPos, dwRight);
			memcpy_s(sendbuf + dwRight, datelen - dwRight, buf, dwWritePos);
			nSendBytes = send(s, buf + dwReadPos, datelen, 0);
			if (SOCKET_ERROR == nSendBytes)
			{
				if (WSAEWOULDBLOCK != WSAGetLastError())
				{

				}
			}

			dwReadPos = ((dwReadPos + nSendBytes) >= dwBufSize ? (dwReadPos + nSendBytes - dwBufSize) : (dwReadPos + nSendBytes));
			bEmpty = (dwReadPos == dwWritePos);
			break;
		}
	} while (FALSE);
	LeaveCriticalSection(&cs);

	return nSendBytes;
}
