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
			// ������Ϊ��
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
			// ������д
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
				dwWritePos += dwLeftSize;// ���Ƿ�����������
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

		bFull = false; // ״̬��Ҫ�������ݷ�����֮����Ϊfalse������
		if (dwReadPos < dwWritePos)
		{
			DWORD datelen = dwWritePos - dwReadPos;
			//memcpy(_buf, buf + dwReadPos, _dwCount);
			nSendBytes = send(s, buf + dwReadPos, datelen, 0);
			if (SOCKET_ERROR == nSendBytes)
			{
				if (WSAEWOULDBLOCK != WSAGetLastError())
				{
					// ������
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
