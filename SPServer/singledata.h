#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <process.h>
#include <list>

typedef void(*PTIoReqSuccess)(DWORD dwTranstion, void* key, void* buf);
typedef void(*PTIoReqFailed)(void* key, void* buf);

typedef struct st_listen
{
	SOCKET sListenSock;
}Listen_Handle, *PListen_Handle;

typedef struct sock_buf_t
{
	WSAOVERLAPPED ol;
	PTIoReqFailed pfnFailed;
	PTIoReqSuccess pfnSuccess;
	void* pRelateSockHandle;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	DWORD datalen;
}SOCK_BUF_T;
#define SOCK_BUF_T_SIZE sizeof(SOCK_BUF_T)

typedef struct sock_buf
{
	WSAOVERLAPPED ol;
	PTIoReqFailed pfnFailed;
	PTIoReqSuccess pfnSuccess;
	void* pRelateSockHandle;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	DWORD datalen;
	CHAR data[1];

	void Init(DWORD dwSize)
	{
		memset(&ol, 0x00, sizeof(ol));
		pfnFailed = NULL;
		pfnSuccess = NULL;
		dwRecvedCount = 0;
		dwSendedCount = 0;
		pRelateSockHandle = NULL;
		datalen = dwSize;
	}
}Sock_Buf, *PSock_Buf;
#define SOCK_BUF_SIZE sizeof(Sock_Buf)

typedef struct sock_handle_t
{
	SOCKET s;
	WSABUF wsabuf[2];
	std::list<PSock_Buf>* lstSend;// 对象创建之后在为其分配内存
	CRITICAL_SECTION cs;
	DWORD dwBufsize;
	DWORD dwWritepos;
	DWORD dwReadpos;
	DWORD dwRightsize;
	DWORD dwLeftsize;
	BOOL bFull;
	BOOL bEmpty;
	BYTE sum;
	volatile long nRef;
}SOCK_HANDLE_T;
#define SOCK_HANDLE_T_SIZE sizeof(SOCK_HANDLE_T)

typedef struct sock_handle
{
	SOCKET s;
	WSABUF wsabuf[2];
	std::list<PSock_Buf>* lstSend;// 对象创建之后在为其分配内存
	CRITICAL_SECTION cs;
	DWORD dwBufsize;
	DWORD dwWritepos;
	DWORD dwReadpos;
	DWORD dwRightsize;
	DWORD dwLeftsize;
	BOOL bFull;
	BOOL bEmpty;
	BYTE sum;
	volatile long nRef;
	CHAR buf[1];

	void Init(DWORD _Bufsize)
	{
		if (INVALID_SOCKET != s)
		{
			closesocket(s);
			s = INVALID_SOCKET;
		}
		dwBufsize = _Bufsize;
		dwWritepos = 0;
		dwReadpos = 0;
		dwRightsize = 0;
		dwLeftsize = 0;
		bFull = FALSE;
		bEmpty = TRUE;
		sum = 0;
		nRef = 1;
		//InitializeCriticalSection(&cs); // 方便重新使用此对象，把临界区初始化和list初始化放一起
		//lstSend = new std::list<PSock_Buf>;// 如果使用对象指针，需要在new对象之后显示分配内存；如果直接使用对象，可以这样用
		lstSend->clear();
	}
	void Clear()
	{
		if (NULL != lstSend)
		{
			delete lstSend;
			lstSend = NULL;
		}
	}
	void InitRLsize()
	{
		if (bFull)
		{
			dwRightsize = 0;
			dwLeftsize = 0;
		}
		else if (dwReadpos <= dwWritepos)
		{
			dwRightsize = dwBufsize - dwWritepos;
			dwLeftsize = dwReadpos;
		}
		else
		{
			dwRightsize = dwReadpos - dwWritepos;
			dwLeftsize = 0;
		}
	}
	void InitWSABUFS()
	{
		InitRLsize();
		wsabuf[0].buf = buf + dwWritepos;
		wsabuf[0].len = dwRightsize;
		wsabuf[0].buf = buf;
		wsabuf[0].len = dwLeftsize;
	}
	void InitWRpos(DWORD dwTranstion)
	{
		if (dwTranstion <= 0 || bFull)
			return;

		dwWritepos = (dwTranstion > dwRightsize) ? (dwTranstion - dwRightsize) : (dwWritepos + dwTranstion);
		bFull = (dwWritepos == dwReadpos);
		bEmpty = false;
	}
	BYTE csum(UCHAR *addr, int count)
	{
		for (int i = 0; i < count; i++)
			sum += (BYTE)addr[i];
		return sum;
	}
	DWORD GetCmdDataLength()
	{
		// 在Read中设置吧Full，bEmpty的状态
		if (bEmpty)
			return 0;

		DWORD dwNum = 0;
		sum = 0;
		if (dwWritepos > dwReadpos)
		{
			DWORD nDataNum = dwWritepos - dwReadpos;
			if (nDataNum < 6)
				return 0;

			if ((UCHAR)buf[dwReadpos] != 0xfb || (UCHAR)buf[dwReadpos + 1] != 0xfc)//  没有数据开始标志
			{
				closesocket(s);
				s = INVALID_SOCKET;
				return 0;
			}

			DWORD dwFrameLen = *(INT*)(buf + dwReadpos + 2);
			if ((dwFrameLen + 8) > nDataNum)
				return 0;

			byte nSum = buf[dwReadpos + 6 + dwFrameLen];
			if (nSum != csum((unsigned char*)buf + dwReadpos + 6, dwFrameLen))
			{
				closesocket(s);
				s = INVALID_SOCKET;
				return 0;
			}

			if (0x0d != buf[dwReadpos + dwFrameLen + 7])
			{
				closesocket(s);
				s = INVALID_SOCKET;
				return 0;
			}

			dwReadpos += 6;

			return dwFrameLen + 2;
		}
		else
		{
			DWORD dwLeft = dwBufsize - dwReadpos;
			if (dwLeft < 6)
			{
				char subchar[6];
				memcpy(subchar, buf + dwReadpos, dwLeft);
				memcpy(subchar + dwLeft, buf, 6 - dwLeft);

				if ((UCHAR)subchar[0] != 0xfb || (UCHAR)subchar[1] != 0xfc)//  没有数据开始标志
				{
					closesocket(s);
					s = INVALID_SOCKET;
					return 0;
				}

				DWORD dwFrameLen = *(INT*)(subchar + 2);
				if ((dwFrameLen + 8) > (dwLeft + dwWritepos)) // 消息太长了
				{
					if (bFull)
					{
						closesocket(s);
						s = INVALID_SOCKET;
					}
					return 0;
				}

				DWORD dwIndex = (dwReadpos + 6 - dwBufsize);
				byte nSum = buf[dwIndex + dwFrameLen];
				if (nSum != csum((unsigned char*)buf + dwIndex, dwFrameLen))
				{
					closesocket(s);
					s = INVALID_SOCKET;
					return 0;
				}

				if (0x0d != buf[dwIndex + dwFrameLen + 1])
				{
					closesocket(s);
					s = INVALID_SOCKET;
					return 0;
				}

				dwReadpos = (dwReadpos + 6) > dwBufsize ? (dwReadpos + 6 - dwBufsize) : (dwReadpos + 6);

				return dwFrameLen + 2;
			}
			else
			{
				if ((UCHAR)buf[dwReadpos] != 0xfb || (UCHAR)buf[dwReadpos + 1] != 0xfc)//  没有数据开始标志
				{
					closesocket(s);
					s = INVALID_SOCKET;
					return 0;
				}

				DWORD dwFrameLen = *(INT*)(buf + dwReadpos + 2);
				if ((dwFrameLen + 8) > (dwLeft + dwWritepos)) // 消息太长了
				{
					if (bFull)
					{
						closesocket(s);
						s = INVALID_SOCKET;
					}
					return 0;
				}

				byte nSum = buf[(dwReadpos + 6 + dwFrameLen) >= dwBufsize ? (dwReadpos + 6 + dwFrameLen - dwBufsize) : (dwReadpos + 6 + dwFrameLen)];
				if ((dwFrameLen + 6) > dwLeft)
				{
					csum((unsigned char*)buf + dwReadpos + 6, dwLeft - 6);
					csum((unsigned char*)buf, dwFrameLen - dwLeft + 6);
					if (nSum != sum)
					{
						closesocket(s);
						s = INVALID_SOCKET;
						return 0;
					}
				}
				else
				{
					if (nSum != csum((unsigned char*)buf + dwReadpos + 6, dwFrameLen))
					{
						closesocket(s);
						s = INVALID_SOCKET;
						return 0;
					}
				}

				if (0x0d != buf[(dwReadpos + dwFrameLen + 7) >= dwBufsize ? (dwReadpos + dwFrameLen + 7 - dwBufsize) : (dwReadpos + dwFrameLen + 7)])
				{
					closesocket(s);
					s = INVALID_SOCKET;
					return 0;
				}

				dwReadpos = (dwReadpos + 6) > dwBufsize ? (dwReadpos + 6 - dwBufsize) : (dwReadpos + 6);

				return dwFrameLen + 2;
			}
		}
		return 0;
	}
	int Read(CHAR* _buf, DWORD dwNum)
	{
		if (dwNum <= 0)
			return 0;

		if (bEmpty)
			return 0;

		bFull = false;
		if (dwReadpos < dwWritepos)
		{
			memcpy(_buf, buf + dwReadpos, dwNum);
			dwReadpos += dwNum;
			bEmpty = (dwReadpos == dwWritepos);
			return dwNum;
		}
		else if (dwReadpos >= dwWritepos)
		{
			DWORD leftcount = dwBufsize - dwReadpos;
			if (leftcount > dwNum)
			{
				memcpy(_buf, buf + dwReadpos, dwNum);
				dwReadpos += dwNum;
				bEmpty = (dwReadpos == dwWritepos);
				return dwNum;
			}
			else
			{
				memcpy(_buf, buf + dwReadpos, leftcount);
				dwReadpos = dwNum - leftcount;
				memcpy(_buf + leftcount, buf, dwReadpos);
				bEmpty = (dwReadpos == dwWritepos);
				return leftcount + dwReadpos;
			}
		}

		return 0;
	}
	void AddRef()
	{
		InterlockedIncrement(&nRef);
	}
	void DecRef()
	{
		InterlockedDecrement(&nRef);
	}
	bool CheckSend(Sock_Buf* data)
	{
		EnterCriticalSection(&cs);
		if (lstSend->empty())
		{
			lstSend->push_front(data);
			LeaveCriticalSection(&cs);
			return true;
		}
		else
		{
			lstSend->push_front(data);
			LeaveCriticalSection(&cs);
			return false;
		}
	}
	Sock_Buf* GetNextData()
	{
		Sock_Buf* data = NULL;;
		EnterCriticalSection(&cs);
		lstSend->pop_back();
		if (!lstSend->empty())
			data = lstSend->back();
		LeaveCriticalSection(&cs);
		return data;
	}
}Sock_Handle, *PSock_Handle;
#define SOCK_HANDLE_SIZE sizeof(Sock_Handle)