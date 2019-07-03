#pragma once
#include "stdafx.h"

//��������
//    lpszDriverName�������ķ�����
//    driverPath������������·��
//    lpszDisplayName��nullptr
BOOL MLoadKernelDriver(const wchar_t* lpszDriverName, const wchar_t* driverPath, const wchar_t* lpszDisplayName);
//ж������
//    szSvrName��������
BOOL MUnLoadKernelDriver(const wchar_t* szSvrName);
//������
BOOL XOpenDriver();
//���������Ƿ����
BOOL XDriverLoaded();

BOOL XInitSelfProtect();

BOOL XLoadDriver();

BOOL XCloseDriverHandle();

BOOL XUnLoadDriver();
