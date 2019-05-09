#pragma once
#include "stdafx.h"

//��������
//    lpszDriverName�������ķ�����
//    driverPath������������·��
//    lpszDisplayName��nullptr
EXPORT_CFUNC(BOOL) LoadKernelDriver(const wchar_t* lpszDriverName, const wchar_t* driverPath, const wchar_t* lpszDisplayName);
//ж������
//    szSvrName��������
EXPORT_CFUNC(BOOL) UnLoadKernelDriver(const wchar_t* szSvrName);
//������
EXPORT_CFUNC(BOOL) OpenDriver();
//���������Ƿ����
EXPORT_CFUNC(BOOL) DriverLoaded();

EXPORT_CFUNC(BOOL) XLoadDriver();

EXPORT_CFUNC(BOOL) XUnLoadDriver();
