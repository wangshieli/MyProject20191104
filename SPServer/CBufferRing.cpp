#include "pch.h"
#include "CBufferRing.h"


CBufferRing::CBufferRing(DWORD _bufsize):dwBufSize(_bufsize)
	, dwWritePos(0)
	, dwReadPos(0)
	, dwRightSize(0)
	, dwLeftSize(0)
	, bEempty(TRUE)
	, bFull(FALSE)
{
	buf = (TCHAR*)malloc(dwBufSize);
}


CBufferRing::~CBufferRing()
{
}
