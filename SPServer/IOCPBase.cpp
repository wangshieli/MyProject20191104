#include "pch.h"
#include "IOCPBase.h"
#include <string>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

#ifdef UNICODE
#define __MYFINE__		__FILEW__
#define __MYFUNCTION__		__FUNCTIONW__
#ifdef _STRING_
typedef std::wstring	mystring;
#endif // _STRING_
#define myifstream std::wifstream
#define myofstream std::wofstream
#else
#define __MYFINE__		__FILE__
#define __MYFUNCTION__		__FUNCTION__
#ifdef _STRING_
typedef std::string	mystring;
#endif // _STRING_
#define myifstream std::ifstream
#define myofstream std::ofstream
#endif // UNICODE

#define SPServer_THE_ONE_INSTANCE	_T("Global\\Event_SPServer_The_One_Instance")

#define SERVER_IP	_T("192.168.24.104")
#define SERVER_PORT	6666

HANDLE g_evtListen[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_nListens = 0;

HANDLE g_evtConnect[WSA_MAXIMUM_WAIT_EVENTS];
SOCKET g_sockConnect[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_nConnects = 0;
void(*g_fucEvent[WSA_MAXIMUM_WAIT_EVENTS])(DWORD _dwIndex);

HANDLE g_evtWaitSunitThreadOn = INVALID_HANDLE_VALUE;

HANDLE g_evtSend = INVALID_HANDLE_VALUE;
HANDLE g_evtReConnect = INVALID_HANDLE_VALUE;
HANDLE g_evtSUnit = INVALID_HANDLE_VALUE;

LPFN_ACCEPTEX	IOCPBase::m_pfnAcceptEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS	IOCPBase::m_pfnAcceptExSockaddrs = NULL;
LPFN_DISCONNECTEX	IOCPBase::m_pfnConnectEx = NULL;
HANDLE IOCPBase::m_hIocp = INVALID_HANDLE_VALUE;
PListen_Handle IOCPBase::m_pListenHandle = NULL;
PSock_Handle IOCPBase::m_pSUnitHandle = NULL;
SOCKET IOCPBase::m_sockSend = INVALID_SOCKET;
SOCKET IOCPBase::m_sockReConnect = INVALID_SOCKET;
DWORD IOCPBase::m_dwCpunums = 0;
DWORD IOCPBase::m_dwThreadCounts = 0;
DWORD IOCPBase::m_dwPagesize = 0;
CBufferRing* IOCPBase::m_pCBufRing = new CBufferRing();

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

#ifdef _DEBUG
#define log_printf(format, ...) LOG(format, __MYFINE__	, __MYFUNCTION__, __LINE__, __VA_ARGS__)
#else
#define log_printf(format, ...)
#endif // _DEBUG

IOCPBase::IOCPBase(): m_hUniqueInstance(INVALID_HANDLE_VALUE)
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
		if (WSA_INVALID_EVENT != m_pListenHandle->evtPostAcceptEx)
			WSACloseEvent(m_pListenHandle->evtPostAcceptEx);
		delete m_pListenHandle;
		m_pListenHandle = NULL;
	}

	WSACleanup();

	CloseHandle(m_hUniqueInstance);  // CloseHandle减少对Event的引用，当最后一个引用被关闭的时候Event释放。进程结束的时候系统也会自动释放
	m_hUniqueInstance = INVALID_HANDLE_VALUE;
}

BOOL IOCPBase::UniqueServerInstance()
{
	// OpenEvent/CreateEvent失败返回的是NULL
	m_hUniqueInstance = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL != m_hUniqueInstance)
	{
		return FALSE;
	}

	m_hUniqueInstance = ::CreateEvent(NULL, FALSE, FALSE, SPServer_THE_ONE_INSTANCE);
	if (NULL == m_hUniqueInstance)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL IOCPBase::InitWinsock2()
{
	WSADATA _wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &_wsadata))
	{
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
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs)
			, &m_pfnAcceptExSockaddrs, sizeof(m_pfnAcceptExSockaddrs)
			, &_dwBytes, NULL, NULL))
		{
			break;
		}

		if (SOCKET_ERROR == WSAIoctl(_s, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &GuidConnectEx, sizeof(GuidConnectEx)
			, &m_pfnConnectEx, sizeof(m_pfnConnectEx)
			, &_dwBytes, NULL, NULL))
		{
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
			break;
		}

		InitializeCriticalSection(&m_pListenHandle->cs);
		m_pListenHandle->evtPostAcceptEx = WSACreateEvent();
		if (WSA_INVALID_EVENT == m_pListenHandle->evtPostAcceptEx)
		{
			break;
		}

		m_pListenHandle->sListenSock = INVALID_SOCKET;

		m_pListenHandle->sListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == m_pListenHandle->sListenSock)
		{
			break;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)m_pListenHandle->sListenSock, m_hIocp, (ULONG_PTR)m_pListenHandle, 0))
		{
			break;
		}

		if (SOCKET_ERROR == WSAEventSelect(m_pListenHandle->sListenSock, m_pListenHandle->evtPostAcceptEx, FD_ACCEPT))
		{
			break;
		}
		g_evtListen[g_nListens++] = m_pListenHandle->evtPostAcceptEx;

		struct sockaddr_in laddr;
		memset(&laddr, 0x00, sizeof(laddr));
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(_port);
		laddr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (SOCKET_ERROR == bind(m_pListenHandle->sListenSock, (SOCKADDR*)&laddr, sizeof(laddr)))
		{
			break;
		}

		if (SOCKET_ERROR == listen(m_pListenHandle->sListenSock, SOMAXCONN))
		{
			break;
		}

		return TRUE;
	} while (FALSE);
	
	return FALSE;
}

BOOL IOCPBase::InitSUnit(CONST TCHAR* _sip, USHORT _port)
{
	m_sockSend = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	g_evtSend = WSACreateEvent();
	if (SOCKET_ERROR == WSAEventSelect(m_sockSend, g_evtSend, FD_WRITE))
	{
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_sockSend;
	g_evtConnect[g_nConnects] = g_evtSend;
	g_fucEvent[g_nConnects++] = Fuc_Send;

	m_sockReConnect = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	g_evtReConnect = WSACreateEvent();
	if (SOCKET_ERROR == WSAEventSelect(m_sockReConnect, g_evtReConnect, FD_WRITE))
	{
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_sockReConnect;
	g_evtConnect[g_nConnects] = g_evtReConnect;
	g_fucEvent[g_nConnects++] = Fuc_ReConnect;

	m_pCBufRing->Init(m_dwPagesize * PAGE_NUMS * 2);
	g_evtSUnit = WSACreateEvent();
	m_pSUnitHandle = (PSock_Handle)malloc(m_dwPagesize * PAGE_NUMS);
	if (NULL == m_pSUnitHandle)
	{
		return FALSE;
	}
	InitializeCriticalSection(&m_pSUnitHandle->cs);
	m_pSUnitHandle->lstSend = new std::list<PSock_Buf>;
	if (NULL == m_pSUnitHandle->lstSend)
	{
		return FALSE;
	}
	m_pSUnitHandle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
	m_pSUnitHandle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (NULL == CreateIoCompletionPort((HANDLE)m_pSUnitHandle->s, m_hIocp, (ULONG_PTR)m_pSUnitHandle, 0))
	{
		return FALSE;
	}

	if (SOCKET_ERROR == WSAEventSelect(m_pSUnitHandle->s, g_evtSUnit, FD_WRITE))
	{
		return FALSE;
	}
	g_sockConnect[g_nConnects] = m_pSUnitHandle->s;
	g_evtConnect[g_nConnects] = g_evtSUnit;
	g_fucEvent[g_nConnects++] = Fuc_SUnit;

	g_evtWaitSunitThreadOn = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, sunitthread, NULL, 0, NULL);
	if (WAIT_OBJECT_0 != WaitForSingleObject(g_evtWaitSunitThreadOn, 5000))
	{
		return FALSE;
	}

	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	InetPton(AF_INET, _sip, &addr.sin_addr.s_addr);
	if (SOCKET_ERROR == connect(m_pSUnitHandle->s, (const sockaddr*)&addr, sizeof(addr))) // 如果服务器没打开会卡着
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			return FALSE;
		}
	}

	PSock_Buf pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
	pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
	if (!SU_postZeroRecv(m_pSUnitHandle, pBuf))
	{
		if (0 == InterlockedDecrement(&m_pSUnitHandle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return FALSE;//  启动重连
		}
		free(pBuf);
	}

	return TRUE;
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
	//192.168.24.104 6666
	InitSUnit(SERVER_IP, SERVER_PORT);

	m_dwThreadCounts = m_dwCpunums;
	for (DWORD i = 0; i < m_dwThreadCounts; i++)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, workthread, NULL, 0, NULL);
	}

	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, toolthread, NULL, 0, NULL);

	return 0;
}

unsigned int _stdcall IOCPBase::logthread(PVOID pVoid)
{
	return 0;
}

unsigned int _stdcall IOCPBase::sunitthread(PVOID pVoid)
{
	SetEvent(g_evtWaitSunitThreadOn);
	while (true)
	{
		DWORD dwIndex = WSAWaitForMultipleEvents(g_nConnects, g_evtConnect, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == dwIndex)
		{
			return 0;
		}
		DWORD dwRealIndex = dwIndex - WSA_WAIT_EVENT_0;
		// 返回的是触发事件的最小索引，为了确保此索引之后的事件能被及时处理，需要遍历一遍   目前只监听一个事件，先不处理
		for (DWORD i = dwRealIndex; i < g_nConnects; i++)
		{
			DWORD dwError = WSAWaitForMultipleEvents(1, &g_evtConnect[i], TRUE, 0, FALSE);
			if (WSA_WAIT_TIMEOUT == dwError || WSA_WAIT_FAILED == dwError)
				continue;

			WSAResetEvent(g_evtConnect[i]);

			g_fucEvent[i](i);
		}
	}
	return 0;
}

unsigned int _stdcall IOCPBase::toolthread(PVOID pVoid)
{
	DWORD _dwClienCounts = m_dwThreadCounts * 5;
	for (DWORD i = 0; i < _dwClienCounts; i++)
	{
		postAcceptEx();
	}

	while (true)
	{
		DWORD dwIndex = WSAWaitForMultipleEvents(g_nListens, g_evtListen, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == dwIndex)
		{
			return 0;
		}

		int nError = 0;
		int optval = 0;;
		int optlen = sizeof(int);
		EnterCriticalSection(&m_pListenHandle->cs);
		for (std::list<PSock_Buf>::const_iterator iter = m_pListenHandle->s_list.begin(); iter != m_pListenHandle->s_list.end(); iter++)
		{
			nError = getsockopt(((PSock_Handle)(*iter)->pRelateSockHandle)->s, SOL_SOCKET, SO_CONNECT_TIME, (char*)&optval, &optlen);
			if (SOCKET_ERROR == nError)
			{
				CancelIoEx((HANDLE)m_pListenHandle->sListenSock, &(*iter)->ol);
				//closesocket(*iter);
				continue;
			}


			if (0xFFFFFFFF != optval && optval > 3)
			{
				CancelIoEx((HANDLE)m_pListenHandle->sListenSock, &(*iter)->ol);
				//closesocket(*iter);
			}
		}
		LeaveCriticalSection(&m_pListenHandle->cs);

		WSAResetEvent(g_evtListen[dwIndex - WSA_WAIT_EVENT_0]);

		for (DWORD i = 0; i < _dwClienCounts; i++)
		{
			postAcceptEx();
		}
	}
	return 0;
}

unsigned int _stdcall IOCPBase::workthread(PVOID pVoid)
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

	PSock_Handle _pSock_Handle = (PSock_Handle)malloc(m_dwPagesize * PAGE_NUMS);
	PSock_Buf _pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);

	do
	{
		if (NULL == _pSock_Handle)
		{
			break;
		}
		InitializeCriticalSection(&_pSock_Handle->cs);
		_pSock_Handle->lstSend = new std::list<PSock_Buf>;
		if (NULL == _pSock_Handle->lstSend)
		{
			break;
		}
		_pSock_Handle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
		_pSock_Handle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (SOCKET_ERROR == _pSock_Handle->s)
		{
			break;
		}

		_pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		_pBuf->pRelateSockHandle = _pSock_Handle;
		_pBuf->pfnSuccess = AcceptExSuccess;
		_pBuf->pfnFailed = AcceptExFaile;
		m_pListenHandle->add2list(_pBuf);
		if (!m_pfnAcceptEx(m_pListenHandle->sListenSock, _pSock_Handle->s, _pSock_Handle->buf
			, _pSock_Handle->dwBufsize - ((sizeof(sockaddr_in) + 16) * 2)
			, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &dwBytes, &_pBuf->ol))
		{
			if (WSA_IO_PENDING != WSAGetLastError())
			{
				m_pListenHandle->del3list(_pBuf);
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

	pListen_Hnalde->del3list(pBuf);

	if (SOCKET_ERROR == WSAIoctl(pSock_Handle->s, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL))
	{
	}

	if (NULL == CreateIoCompletionPort((HANDLE)pSock_Handle->s, m_hIocp, (ULONG_PTR)pSock_Handle, 0))
	{
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
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		if (NULL == workBuf)
		{
			_tprintf_s(_T("(NULL == workBuf)%d"), WSAGetLastError());
			continue;
		}
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			pSock_Handle->DecRef();
			free(workBuf);
		}
		len = pSock_Handle->GetCmdDataLength();
	}

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

	pListen_Hnalde->del3list(pBuf);

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
	//_tprintf_s(_T("postZeroRecv"));
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
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	_tprintf_s(_T("ZeroRecvSuccess"));

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
	_tprintf_s(_T("ZeroRecvFaile"));
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

	int r = WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL);
	if (SOCKET_ERROR == r)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
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
	//_tprintf_s(_T("RecvSuccess"));

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		if (NULL == workBuf)
		{
			_tprintf_s(_T("PSock_Buf workBuf:%d"), GetLastError());
			continue;
		}
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			pSock_Handle->DecRef();
			free(workBuf);			
		}
		len = pSock_Handle->GetCmdDataLength();
	}

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
	//_tprintf_s(_T("RecvFaile"));
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
	//_tprintf_s(_T("%d_pBuf->dwSendedCount = %d"), _pSock_Handle->s, _pBuf->dwSendedCount);

	_pBuf->pfnFailed = SendFaile;
	_pBuf->pfnSuccess = SendSuccess;
	//_tprintf_s(_T("socket = %d"), _pSock_Handle->s);
	if (SOCKET_ERROR == WSASend(_pSock_Handle->s, &_pBuf->wsaBuf, 1, &dwBytes, 0, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SendSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	//_tprintf_s(_T("SendSuccess"));
	if (_dwTranstion <= 0)
		return SendFaile(_pSock_Handle, _pBuf);

	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (NULL == pSock_Handle)
	{
		_tprintf_s(_T("NULL == pSock_Handle"));
	}
	if (NULL == _pSock_Handle)
	{
		_tprintf_s(_T("NULL == _pSock_Handle"));
	}

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
	pBuf = NULL;
	pBuf = pSock_Handle->GetNextData();
	if (NULL != pBuf)
	{
		InterlockedDecrement(&pSock_Handle->nRef);
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
	_tprintf_s(_T("SendFaile"));
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

BOOL IOCPBase::SU_postZeroRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->wsabuf[0].buf = NULL;
	_pSock_Handle->wsabuf[0].len = 0;
	_pSock_Handle->wsabuf[1].buf = NULL;
	_pSock_Handle->wsabuf[1].len = 0;

	_pBuf->pfnFailed = SU_ZeroRecvFaile;
	_pBuf->pfnSuccess = SU_ZeroRecvSuccess;

	if (SOCKET_ERROR == WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SU_ZeroRecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	if (!SU_postRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return;// 重新连接
		}
		free(pBuf);
	}
}

void IOCPBase::SU_ZeroRecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(NULL, pBuf);
		ReConnect_PostEventMessage();
		return;// 重启连接
	}
	free(pBuf);
}

BOOL IOCPBase::SU_postRecv(PSock_Handle _pSock_Handle, PSock_Buf _pBuf)
{
	DWORD dwBytes = 0;
	DWORD dwFlags = 0;

	_pSock_Handle->InitWSABUFS();

	_pBuf->pfnFailed = SU_RecvFaile;
	_pBuf->pfnSuccess = SU_RecvSuccess;

	int r = WSARecv(_pSock_Handle->s, _pSock_Handle->wsabuf, 2, &dwBytes, &dwFlags, &_pBuf->ol, NULL);
	if (SOCKET_ERROR == r)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void IOCPBase::SU_RecvSuccess(DWORD _dwTranstion, PVOID _pSock_Handle, PVOID _pBuf)
{
	if (_dwTranstion <= 0)
		return SU_RecvFaile(_pSock_Handle, _pBuf);

	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	pSock_Handle->InitWRpos(_dwTranstion);
	DWORD len = pSock_Handle->GetCmdDataLength();
	while (len)
	{
		PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
		workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
		pSock_Handle->Read(workBuf->data, len);
		workBuf->dwRecvedCount = len;
		workBuf->pRelateSockHandle = pSock_Handle;
		pSock_Handle->AddRef();
		workBuf->pfnFailed = SU_DoWorkProcessFaile;	// 处理业务的过程
		workBuf->pfnSuccess = DoWorkProcessSuccess;
		if (!PostQueuedCompletionStatus(m_hIocp, len, (ULONG_PTR)workBuf, &workBuf->ol))
		{
			pSock_Handle->DecRef();
			free(workBuf);
		}
		len = pSock_Handle->GetCmdDataLength();
	}

	if (!SU_postZeroRecv(pSock_Handle, pBuf))
	{
		if (0 == InterlockedDecrement(&pSock_Handle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			ReConnect_PostEventMessage();
			return;
		}
		free(pBuf);
	}
}

void IOCPBase::SU_RecvFaile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		ClearSingleData(NULL, pBuf);
		ReConnect_PostEventMessage();
		return;// 重新连接
	}
	free(pBuf);
}

void IOCPBase::DoWorkProcessSuccess(DWORD _dwTranstion, PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;

	msgpack::unpacker upk;
	upk.reserve_buffer(pBuf->dwRecvedCount - 2);
	memcpy_s(upk.buffer(), pBuf->dwRecvedCount - 2, pBuf->data, pBuf->dwRecvedCount - 2);
	upk.buffer_consumed(pBuf->dwRecvedCount - 2);
	msgpack::object_handle oh;
	mystring str;
	while (upk.next(oh))
	{
		str = oh.get().as<mystring>();
	}
	
	_stprintf_s(pBuf->data, pBuf->datalen - 1, _T("RetrunData:%s"), str.c_str());
	pBuf->dwRecvedCount = _tcslen(pBuf->data);

//	Send_PostEventMessage(pBuf->data, pBuf->dwRecvedCount);

	SendFile(pSock_Handle, pBuf);

	//if (pSock_Handle->CheckSend(pBuf))
	//{
	//	if (!postSend(pSock_Handle, pBuf))
	//	{
	//		return SendFaile(pSock_Handle, pBuf);
	//	}
	//}
	//else
	//{
	//}
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

void IOCPBase::SU_DoWorkProcessFaile(PVOID _pBuf, PVOID pBuf_)
{
	PSock_Buf pBuf = (PSock_Buf)pBuf_;
	PSock_Handle pSock_Handle = (PSock_Handle)pBuf->pRelateSockHandle;
	if (0 == InterlockedDecrement(&pSock_Handle->nRef))
	{
		// 重新连接
		//ReConnect_PostEventMessage();
	}
	free(pBuf);
}

void IOCPBase::FD_Connect()
{
}

void IOCPBase::FD_Read()
{
	TCHAR buf[1024] = { 0 };
	recv(m_pSUnitHandle->s, buf, 1024, 0);
	//PostQueuedCompletionStatus(m_hIocp, 0, NULL, NULL);
}

void IOCPBase::FD_Write()
{
	m_pCBufRing->readData(m_pSUnitHandle->s);
	/*
	const TCHAR* psend = _T("TEST");
	if (SOCKET_ERROR == send(m_pSUnitHandle->s, psend, _tcslen(psend), 0))
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			return;
		}
	}*/
}

void IOCPBase::FD_Close()
{
}

void IOCPBase::Fuc_SUnit(DWORD _dwIndex)
{
	WSANETWORKEVENTS networkEvents;
	if (SOCKET_ERROR == WSAEnumNetworkEvents(g_sockConnect[_dwIndex], g_evtConnect[_dwIndex], &networkEvents))
	{
		return;
	}
	if (networkEvents.lNetworkEvents & FD_READ)
	{
		if (networkEvents.iErrorCode[FD_READ_BIT])
		{
			return;
		}
		else
		{
			FD_Read();
		}
	}
	else if (networkEvents.lNetworkEvents & FD_WRITE)
	{
		if (networkEvents.iErrorCode[FD_WRITE_BIT])
		{
			return;
		}
		else
		{
			FD_Write();
		}
	}
	else if (networkEvents.lNetworkEvents & FD_CONNECT)
	{
		if (networkEvents.iErrorCode[FD_CONNECT_BIT])
		{
			return;
		}
		else // 处理连接
		{
			FD_Connect();
		}
	}
	else/* if (networkEvents.lNetworkEvents & FD_CLOSE)*/
	{
		if (networkEvents.iErrorCode[FD_CLOSE_BIT])
		{
			return;
		}
		else
		{
			FD_Close();
		}
	}
}

void IOCPBase::Send_PostEventMessage(TCHAR* _buf, DWORD _bufsize)
{
	m_pCBufRing->writeData(_buf, _bufsize);
	WSASetEvent(g_evtSend);
}

void IOCPBase::ReConnect_PostEventMessage()
{
	//log_printf(_T("ReConnect_PostEventMessage"));
	WSASetEvent(g_evtReConnect);
}

void IOCPBase::Fuc_Send(DWORD _dwIndex)
{
	//log_printf(_T("Fuc_Send"));
	m_pCBufRing->readData(m_pSUnitHandle->s);
	/*
	const TCHAR* psend = _T("TEST");
	if (SOCKET_ERROR == send(m_pSUnitHandle->s, psend, _tcslen(psend), 0))
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			return;
		}
	}*/
}

void IOCPBase::Fuc_ReConnect(DWORD _dwIndex)
{
	closesocket(m_pSUnitHandle->s);
	m_pSUnitHandle->s = INVALID_SOCKET;
	m_pCBufRing->Init(m_dwPagesize * PAGE_NUMS * 2);
	m_pSUnitHandle->Init(m_dwPagesize * PAGE_NUMS - SOCK_HANDLE_T_SIZE);
	m_pSUnitHandle->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (NULL == CreateIoCompletionPort((HANDLE)m_pSUnitHandle->s, m_hIocp, (ULONG_PTR)m_pSUnitHandle, 0))
	{
		return;
	}

	if (SOCKET_ERROR == WSAEventSelect(m_pSUnitHandle->s, g_evtSUnit, FD_CLOSE | FD_WRITE))
	{
		return;
	}
	g_sockConnect[g_nConnects - 1] = m_pSUnitHandle->s;

	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	InetPton(AF_INET, SERVER_IP, &addr.sin_addr.s_addr);
	if (SOCKET_ERROR == connect(m_pSUnitHandle->s, (const sockaddr*)&addr, sizeof(addr))) // 如果服务器没打开会卡着
	{
		if (WSAEWOULDBLOCK != WSAGetLastError())
		{
			return ;
		}
	}

	PSock_Buf pBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
	pBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
	if (!SU_postZeroRecv(m_pSUnitHandle, pBuf))
	{
		if (0 == InterlockedDecrement(&m_pSUnitHandle->nRef))
		{
			ClearSingleData(NULL, pBuf);
			return ;//  启动重连
		}
		free(pBuf);
	}
}

void IOCPBase::SendFile(PVOID _pSock_Handle, PVOID _pBuf)
{
	PSock_Buf pBuf = (PSock_Buf)_pBuf;
	PSock_Handle pSock_Handle = (PSock_Handle)_pSock_Handle;

	do
	{
		myifstream inf(_T("src.exe"), std::ios::binary | std::ios::_Nocreate);
		if (!inf)
		{
			_stprintf_s(pBuf->data, pBuf->datalen, _T("打开文件失败"));
			pBuf->dwRecvedCount = _tcslen(pBuf->data);
			break;
		}

		DWORD len = (DWORD)inf.seekg(0, std::ios::end).tellg();
		inf.seekg(0, std::ios::beg);
		_stprintf_s(pBuf->data, pBuf->datalen, _T("%d"), len);
		pBuf->dwRecvedCount = _tcslen(pBuf->data);
		if (pSock_Handle->CheckSend(pBuf))
		{
			if (!postSend(pSock_Handle, pBuf))
			{
				inf.close();
				return SendFaile(pSock_Handle, pBuf);
			}
		}

		while (!inf.eof())
		{
			PSock_Buf workBuf = (PSock_Buf)malloc(m_dwPagesize * PAGE_NUMS);
			if (NULL == workBuf)
			{
				_tprintf_s(_T("workBuf内存分配失败:%d"), GetLastError());
				continue;
			}
			workBuf->Init(m_dwPagesize * PAGE_NUMS - SOCK_BUF_T_SIZE);
			pSock_Handle->AddRef();
			workBuf->pRelateSockHandle = pSock_Handle;

			inf.read(workBuf->data, workBuf->datalen);
			workBuf->dwRecvedCount = (DWORD)inf.gcount();

			if (pSock_Handle->CheckSend(workBuf))
			{
				if (!postSend(pSock_Handle, workBuf))
				{
					inf.close();
					return SendFaile(pSock_Handle, workBuf);
				}
			}
		}
		inf.close();
		return;
	} while (FALSE);

	if (pSock_Handle->CheckSend(pBuf))
	{
		if (!postSend(pSock_Handle, pBuf))
		{
			return SendFaile(pSock_Handle, pBuf);
		}
	}
}
