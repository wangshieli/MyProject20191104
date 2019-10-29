#include "pch.h"
#include "IOCPBase.h"
#include <string>

#pragma comment(lib, "ws2_32.lib")

#ifdef UNICODE
#define __MYFINE__		__FILEW__
#define __MYFUNCTION__		__FUNCTIONW__
#ifdef _STRING_
typedef std::wstring	mystring;
#endif // _STRING_
#else
#define __MY_FINE__		__FILE__
#define __MY_FUNCTION__		__FUNCTION__
#ifdef _STRING_
typedef std::string	mystring;
#endif // _STRING_
#endif // UNICODE


void LOG(LPCTSTR format, LPCTSTR _filename, LPCTSTR _funcname, LONG _linenum, ...)
{
	mystring _temp(_T("%s_%s_%d:"));
	_temp.append(format);
	_temp.append(_T("\n"));
	va_list va;
	va_start(va, format);
	_vtprintf_s(_temp.c_str(), va);
	va_end(va);
}

#define log_printf(format, ...) LOG(format, __MYFINE__	, __MYFUNCTION__, __LINE__, __VA_ARGS__)

#define SPServer_THE_ONE_INSTANCE	_T("Global\\Event_SPServer_The_One_Instance")

LPFN_ACCEPTEX	IOCPBase::m_pfnAcceptEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS	IOCPBase::m_pfnAcceptExSockaddrs = NULL;
LPFN_DISCONNECTEX	IOCPBase::m_pfnConnectEx = NULL;
HANDLE IOCPBase::m_hIocp = INVALID_HANDLE_VALUE;
PListen_Handle IOCPBase::m_pListenHandle = NULL;
DWORD IOCPBase::m_dwCpunums = 0;
DWORD IOCPBase::m_dwPagesize = 0;

IOCPBase::IOCPBase(): m_hUniqueInstance(INVALID_HANDLE_VALUE)
	//, m_pfnAcceptEx(NULL)
	//, m_pfnAcceptExSockaddrs(NULL)
	//, m_pfnConnectEx(NULL)
	//, m_hIocp(INVALID_HANDLE_VALUE)
	//, m_pListenHandle(NULL)
{
	if (!UniqueServerInstance() || !InitWinsock2() || !GetExtensFunction() || !CreateIoCompletionPortHandle() || !InitListenSocket(LISTEN_PORT) || !GetCpuNumsAndPagesize())
		return;

	StartServer();
}

IOCPBase::~IOCPBase()
{
	if (NULL != m_pListenHandle)
	{
		closesocket(m_pListenHandle->sListenSock);
		m_pListenHandle->sListenSock = INVALID_SOCKET;
		delete m_pListenHandle;
		m_pListenHandle = NULL;
	}

	WSACleanup();

	CloseHandle(m_hUniqueInstance);  // CloseHandle���ٶ�Event�����ã������һ�����ñ��رյ�ʱ��Event�ͷš����̽�����ʱ��ϵͳҲ���Զ��ͷ�
	m_hUniqueInstance = INVALID_HANDLE_VALUE;
}

BOOL IOCPBase::UniqueServerInstance()
{
	// OpenEvent/CreateEventʧ�ܷ��ص���NULL
	m_hUniqueInstance = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL != m_hUniqueInstance)
	{
		log_printf(_T("�Ѿ��ж�Ӧ�ķ�����������%s\n"), _T("test"));
		return FALSE;
	}

	m_hUniqueInstance = ::CreateEvent(NULL, FALSE, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL == m_hUniqueInstance)
	{
		log_printf(_T("UniqueInstance�¼���ʼ������%d\n"), GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IOCPBase::InitWinsock2()
{
	WSADATA _wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &_wsadata))
	{
		log_printf(_T("��ʼ��winsock2ʧ��:%d"), GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL IOCPBase::GetExtensFunction()
{
	SOCKET _s = INVALID_SOCKET;
	_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _s)
	{
		log_printf(_T("��ȡ�ⲿ��չ����ʧ��:%d"), WSAGetLastError());
		return FALSE;
	}

	do
	{
		DWORD _dwBytes = 0;
		GUID GuidAcceptEx = WSAID_ACCEPTEX,
			GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS,
			GuidConnectEx = WSAID_CONNECTEX;
		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidAcceptEx, sizeof(GuidAcceptEx)
			, &m_pfnAcceptEx, sizeof(m_pfnAcceptEx)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("��ȡ�ⲿ��չ����ʧ��:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs)
			, &m_pfnAcceptExSockaddrs, sizeof(m_pfnAcceptExSockaddrs)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("��ȡ�ⲿ��չ����ʧ��:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidConnectEx, sizeof(GuidConnectEx)
			, &m_pfnConnectEx, sizeof(m_pfnConnectEx)
			, &_dwBytes, NULL, NULL))
		{
			log_printf(_T("��ȡ�ⲿ��չ����ʧ��:%d"), WSAGetLastError());
			break;
		}
		closesocket(_s);
		_s = INVALID_SOCKET;
		return TRUE;
	} while (true);

	closesocket(_s);
	_s = INVALID_SOCKET;
	return FALSE;
}

BOOL IOCPBase::CreateIoCompletionPortHandle()
{
	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if (NULL == m_hIocp)
	{
		log_printf(_T("�����˿�ʧ��:%d"), WSAGetLastError());
		return FALSE;
	}
	return TRUE;
}

BOOL IOCPBase::InitListenSocket(USHORT _port)
{
	do
	{
		m_pListenHandle = new Listen_Handle();
		if (NULL == m_pListenHandle)
		{
			log_printf(_T("��ʼ�������˿�ʧ��:%d"), WSAGetLastError());
			break;
		}
		m_pListenHandle->sListenSock = INVALID_SOCKET;

		m_pListenHandle->sListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (INVALID_SOCKET == m_pListenHandle->sListenSock)
		{
			log_printf(_T("��ʼ�������˿�ʧ��:%d"), WSAGetLastError());
			break;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)m_pListenHandle->sListenSock, m_hIocp, (ULONG_PTR)m_pListenHandle, 0))
		{
			log_printf(_T("��ʼ�������˿�ʧ��:%d"), WSAGetLastError());
			break;
		}

		struct sockaddr_in laddr;
		memset(&laddr, 0x00, sizeof(laddr));
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(_port);
		laddr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (SOCKET_ERROR == bind(m_pListenHandle->sListenSock, (SOCKADDR*)&laddr, sizeof(laddr)))
		{
			log_printf(_T("��ʼ�������˿�ʧ��:%d"), WSAGetLastError());
			break;
		}

		if (SOCKET_ERROR == listen(m_pListenHandle->sListenSock, SOMAXCONN))
		{
			log_printf(_T("��ʼ�������˿�ʧ��:%d"), WSAGetLastError());
			break;
		}

		return TRUE;
	} while (FALSE);
	
	return FALSE;
}

BOOL IOCPBase::GetCpuNumsAndPagesize()
{
	SYSTEM_INFO sys;
	GetSystemInfo(&sys);
	m_dwCpunums = sys.dwNumberOfProcessors;
	m_dwPagesize = sys.dwPageSize;

	return TRUE;
}

BOOL IOCPBase::StartServer()
{
	DWORD _dwThreadCounts = m_dwCpunums * 2 + 2;
	for (DWORD i = 0; i < _dwThreadCounts; i++)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, workthread, NULL, 0, NULL);
	}

	DWORD _dwClienCounts = _dwThreadCounts * 5;
	for (DWORD i = 0; i < _dwClienCounts; i++)
	{
		postAcceptEx();
	}
	return 0;
}

unsigned int _stdcall IOCPBase::workthread(LPVOID pVoid)
{
	ULONG_PTR key;
	PSock_Buf pBuf;
	LPOVERLAPPED lpol;
	DWORD dwTranstion;
	BOOL bSuccess = FALSE;

	while (true)
	{
		bSuccess = GetQueuedCompletionStatus(m_hIocp, &dwTranstion, &key, &lpol, INFINITE);
		if (NULL == lpol)
		{
			log_printf(_T("�˴�Ӧ�������˳��ź�"));
			return 0;
		}

		pBuf = CONTAINING_RECORD(lpol, Sock_Buf, ol);

		if (!bSuccess)
			pBuf->pfnFailed((PVOID)key, pBuf);
		else
			pBuf->pfnSuccess(dwTranstion, (PVOID)key, pBuf);
	}

	return 0;
}

void IOCPBase::ClearSingleData(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	if (NULL != _pSock_Handle)
	{
		DeleteCriticalSection(&_pSock_Handle->cs);
		if (NULL != _pSock_Handle->lstSend)
			delete _pSock_Handle->lstSend;
		if (INVALID_SOCKET != _pSock_Handle->s)
		{
			closesocket(_pSock_Handle->s);
			_pSock_Handle->s = INVALID_SOCKET;
		}
		free(_pSock_Handle);
		_pSock_Handle = NULL;

	}

	if (NULL != _pBuf)
	{
		free(_pBuf);
		_pBuf = NULL;
	}
}

BOOL IOCPBase::postAcceptEx()
{
	DWORD dwBytes = 0;

	PSock_Handle _pSock_Handle = (PSock_Handle)malloc(m_dwPagesize * 8);
	PSock_Buf _pBuf = (PSock_Buf)malloc(m_dwPagesize * 8);

	do
	{
		if (NULL == _pSock_Handle)
		{
			log_printf(_T("AcceptExʧ��:%d"), WSAGetLastError());
			break;
		}
		InitializeCriticalSection(&_pSock_Handle->cs);
		_pSock_Handle->lstSend = new std::list<PSock_Buf>;
		if (NULL == _pSock_Handle->lstSend)
		{
			log_printf(_T("AcceptExʧ��:%d"), WSAGetLastError());
			break;
		}
		_pSock_Handle->Init(m_dwPagesize * 8 - SOCK_HANDLE_T_SIZE);
		_pSock_Handle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (SOCKET_ERROR == _pSock_Handle->s)
		{
			log_printf(_T("AcceptExʧ��:%d"), WSAGetLastError());
			break;
		}

		_pBuf->Init(m_dwPagesize * 8 - SOCK_BUF_T_SIZE);
		_pBuf->pRelateSockHandle = _pSock_Handle;
		_pBuf->pfnSuccess = AcceptExSuccess;
		_pBuf->pfnFailed = AcceptExFaile;
		if (!m_pfnAcceptEx(m_pListenHandle->sListenSock, _pSock_Handle->s, _pSock_Handle->buf
			, _pSock_Handle->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2)
			, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &dwBytes, &_pBuf->ol))
		{
			if (WSA_IO_PENDING != WSAGetLastError())
			{
				log_printf(_T("AcceptExʧ��:%d"), WSAGetLastError());
				break;
			}
		}
		return TRUE;
	} while (FALSE);
	
	ClearSingleData(_pSock_Handle, _pBuf);

	return FALSE;
}

struct tcp_keepalive alive_in = { TRUE, 1000 * 10, 1000 };
struct tcp_keepalive alive_out = { 0 };
unsigned long ulBytesReturn = 0;

void IOCPBase::AcceptExSuccess(DWORD _dwTranstion, PVOID _pListen_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return AcceptExFaile(_pListen_Handle, _pBuf);

	PListen_Handle pListen_Hnalde = (PListen_Handle)_pListen_Handle;
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	if (SOCKET_ERROR == WSAIoctl(pSock_Handle->s, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL))
	{
		log_printf(_T("���ÿͻ��˳�ʱ���ʧ��:%d"), WSAGetLastError());
	}

	if (NULL == CreateIoCompletionPort((HANDLE)pSock_Handle->s, m_hIocp, (ULONG_PTR)pSock_Handle, 0))
	{
		log_printf(_T("�ͻ��˰󶨶˿�ʧ��:%d"), WSAGetLastError());
		AcceptExFaile(_pListen_Handle, _pBuf);
		return;
	}

	SOCKADDR* localAddr = NULL;
	SOCKADDR* remoteAddr = NULL;
	int localAddrlen = 0;
	int remoteAddrlen = 0;

	m_pfnAcceptExSockaddrs(pSock_Handle->buf, pSock_Handle->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2)
		, sizeof(sockaddr_in) + 16
		, sizeof(sockaddr_in) + 16
		, &localAddr, &localAddrlen
		, &remoteAddr, &remoteAddrlen);

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * 8);
		workBuf->Init(m_dwPagesize * 8 - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = DoWorkProcessFaile;	// ����ҵ��Ĺ���
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			log_printf(_T("ҵ��Ͷ�ݴ���:%d"), WSAGetLastError());
			pSock_Handle->DecRef();
			free(workBuf);
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	pBuf->pfnFailed = ZeroRecvFaile;
	pBuf->pfnSuccess = ZeroRecvSuccess;
	if (!postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::AcceptExFaile(PVOID _pListen_Handle, PVOID _pBuf)
{
	PListen_Handle pListen_Hnalde = (PListen_Handle)_pListen_Handle;
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	if (NULL != pSock_Handle)
	{
		DeleteCriticalSection(&pSock_Handle->cs);
		if (NULL != pSock_Handle->lstSend)
			delete pSock_Handle->lstSend;
		if (INVALID_SOCKET != pSock_Handle->s)
		{
			closesocket(pSock_Handle->s);
			pSock_Handle->s = INVALID_SOCKET;
		}
		free(pSock_Handle);
		pSock_Handle = NULL;

	}

	if (NULL != pBuf)
	{
		free(pBuf);
		pBuf = NULL;
	}
}

BOOL IOCPBase::postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->wsabuf[0].buf = NULL;
	_pSock_Handle->wsabuf[0].len = 0;
	_pSock_Handle->wsabuf[1].buf = NULL;
	_pSock_Handle->wsabuf[1].len = 0;

	_pBuf->pfnFailed = ZeroRecvFaile;
	_pBuf->pfnSuccess = ZeroRecvSuccess;

	if (SOCKET_ERROR == WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING == WSAGetLastError())
		{
			log_printf(_T("��ͻ���������ʧ�ܣ�0�ֽڣ�:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	pBuf->pfnFailed = RecvFaile;
	pBuf->pfnSuccess = RecvSuccess;
	if (!postRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(pSock_Handle, pBuf);
		return;
	}
	free(pBuf);
}

BOOL IOCPBase::postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->InitWSABUFS();

	_pBuf->pfnFailed = RecvFaile;
	_pBuf->pfnSuccess = RecvSuccess;

	if (SOCKET_ERROR == WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING == WSAGetLastError())
		{
			log_printf(_T("���ջ����͵�����ʧ��:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return RecvFaile(_pSock_Handle, _pBuf);
	
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = new Sock_Buf;
		workBuf->Init(123456);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = NULL;	// ����ҵ��Ĺ���
		workBuf->pfnSuccess = NULL;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			log_printf(_T("ҵ��Ͷ�ݴ���:%d"), WSAGetLastError());
			pSock_Handle->DecRef();
			free(workBuf);			
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	pBuf->pfnFailed = ZeroRecvFaile;
	pBuf->pfnSuccess = ZeroRecvSuccess;
	if (!postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(pSock_Handle, pBuf);
			return;
		}
		free(pBuf);
	}

}

void IOCPBase::RecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(pSock_Handle, pBuf);
		return;
	}
	free(pBuf);
}

BOOL IOCPBase::postSend(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;

	_pBuf->wsaBuf.buf = _pBuf->data + _pBuf->dwSendedCount;
	_pBuf->wsaBuf.len = _pBuf->dwRecvedCount - _pBuf->dwSendedCount;

	_pBuf->pfnFailed = SendFaile;
	_pBuf->pfnSuccess = SendSuccess;

	if (SOCKET_ERROR == WSASend(_pSock_Handle->s, &_pBuf->wsaBuf, 1, &dwBytes, 0, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING == WSAGetLastError())
		{
			log_printf(_T("��ͻ���������ʧ��:%d"), WSAGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return SendFaile(_pSock_Handle, _pBuf);

	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	pBuf->dwSendedCount += _dwTranstion;
	if (pBuf->dwSendedCount < pBuf->dwRecvedCount)
	{
		if (!postSend(pSock_Handle, pBuf))
		{
			return SendFaile(_pSock_Handle, _pBuf);
		}
		return;
	}

	free(pBuf);
	pBuf = pSock_Handle->GetNextData();
	if (pBuf)
	{
		InterlockedDecrement(&pSock_Handle->nRef);
		pBuf->pfnFailed = SendFaile;
		pBuf->pfnSuccess = SendSuccess;
		if (!postSend(pSock_Handle, pBuf))
		{
			SendFaile(pSock_Handle, pBuf);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			if (NULL != _pSock_Handle)
			{
				DeleteCriticalSection(&pSock_Handle->cs);
				if (NULL != pSock_Handle->lstSend)
					delete pSock_Handle->lstSend;
				if (INVALID_SOCKET != pSock_Handle->s)
				{
					closesocket(pSock_Handle->s);
					pSock_Handle->s = INVALID_SOCKET;
				}
				free(pSock_Handle);
				pSock_Handle = NULL;
			}
		}
	}
}

void IOCPBase::SendFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	free(pBuf);

	pBuf = pSock_Handle->GetNextData();
	if (pBuf)
	{
		InterlockedDecrement(&pSock_Handle->nRef);
		pBuf->pfnFailed = SendFaile;
		pBuf->pfnSuccess = SendSuccess;
		if (!postSend(pSock_Handle, pBuf))
		{
			SendFaile(pSock_Handle, pBuf);
		}
	}
	else
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			if (NULL != pSock_Handle)
			{
				DeleteCriticalSection(&pSock_Handle->cs);
				if (NULL != pSock_Handle->lstSend)
					delete pSock_Handle->lstSend;
				if (INVALID_SOCKET != pSock_Handle->s)
				{
					closesocket(pSock_Handle->s);
					pSock_Handle->s = INVALID_SOCKET;
				}
				free(pSock_Handle);
				pSock_Handle = NULL;
			}
		}
	}
}

void IOCPBase::DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	// do something

	if (pSock_Handle->CheckSend(pBuf))
	{
		pBuf->pfnFailed = SendFaile;
		pBuf->pfnSuccess = SendSuccess;
		if (!postSend(pSock_Handle, pBuf))
		{
			return SendFaile(pSock_Handle, pBuf);
		}
	}
}

void IOCPBase::DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		if (NULL != pSock_Handle)
		{
			DeleteCriticalSection(&pSock_Handle->cs);
			if (NULL != pSock_Handle->lstSend)
				delete pSock_Handle->lstSend;
			if (INVALID_SOCKET != pSock_Handle->s)
			{
				closesocket(pSock_Handle->s);
				pSock_Handle->s = INVALID_SOCKET;
			}
			free(pSock_Handle);
			pSock_Handle = NULL;
		}
	}
	free(pBuf);
}