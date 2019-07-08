#include "stdafx.h"
#include "DriverLoader.h"
#include "JiYuTrainer.h"
#include "KernelUtils.h"
#include "SysHlp.h"
#include "AppPublic.h"
#include <shlwapi.h>

extern JTApp * currentApp;
extern LoggerInternal * currentLogger;

HANDLE hKDrv = NULL;

//ɾ��ע�����Լ��Ӽ�
BOOL MRegDeleteKey(HKEY hRootKey, LPWSTR path) {

	DWORD lastErr = SHDeleteKey(hRootKey, path);
	if (lastErr == ERROR_SUCCESS || lastErr == ERROR_FILE_NOT_FOUND)
		return TRUE;
	else
	{
		SetLastError(lastErr);
		return 0;
	}
}
BOOL MRegForceDeleteServiceRegkey(LPWSTR lpszDriverName)
{
	BOOL rs = FALSE;
	wchar_t regPath[MAX_PATH];
	wsprintf(regPath, L"SYSTEM\\CurrentControlSet\\services\\%s", lpszDriverName);
	rs = MRegDeleteKey(HKEY_LOCAL_MACHINE, regPath);

	if (!rs) currentLogger->LogWarn2(L"RegDeleteTree failed : %d in delete key HKEY_LOCAL_MACHINE\\%s", GetLastError(), regPath);
	else currentLogger->LogInfo(L"Service Key deleted : HKEY_LOCAL_MACHINE\\%s", regPath);

	wchar_t regName[MAX_PATH];
	wcscpy_s(regName, lpszDriverName);
	_wcsupr_s(regName);
	wsprintf(regPath, L"SYSTEM\\CurrentControlSet\\Enum\\Root\\LEGACY_%s", regName);
	rs = MRegDeleteKey(HKEY_LOCAL_MACHINE, regPath);

	if (!rs) {
		currentLogger->LogWarn2(L"RegDeleteTree failed : %d in delete key HKEY_LOCAL_MACHINE\\%s", GetLastError(), regPath);
		rs = TRUE;
	}
	else currentLogger->LogInfo(L"Service Key deleted : HKEY_LOCAL_MACHINE\\%s", regPath);

	return rs;
}
//��������
//    lpszDriverName�������ķ�����
//    driverPath������������·��
//    lpszDisplayName��nullptr
BOOL MLoadKernelDriver(const wchar_t* lpszDriverName, const wchar_t* driverPath, const wchar_t* lpszDisplayName)
{
	//MessageBox(0, driverPath, L"driverPath", 0);
	wchar_t sDriverName[32];
	wcscpy_s(sDriverName, lpszDriverName);

	bool recreatee = false;

RECREATE:
	DWORD dwRtn = 0;
	BOOL bRet = FALSE;
	SC_HANDLE hServiceMgr = NULL;
	SC_HANDLE hServiceDDK = NULL;
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		currentLogger->LogError2(L"Load driver error in OpenSCManager : %d", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}

	hServiceDDK = CreateService(hServiceMgr, lpszDriverName, lpszDisplayName, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, driverPath, NULL, NULL, NULL, NULL, NULL);


	if (hServiceDDK == NULL)
	{
		dwRtn = GetLastError();
		if (dwRtn == ERROR_SERVICE_MARKED_FOR_DELETE)
		{
			currentLogger->LogError2(L"Load driver error in CreateService : ERROR_SERVICE_MARKED_FOR_DELETE");
			if (!recreatee) {
				recreatee = true;
				if (hServiceDDK) CloseServiceHandle(hServiceDDK);
				if (hServiceMgr) CloseServiceHandle(hServiceMgr);
				if (MRegForceDeleteServiceRegkey(sDriverName)) goto RECREATE;
			}
		}
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_EXISTS)
		{
			currentLogger->LogError2(L"Load driver error in CreateService : %d", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
		hServiceDDK = OpenService(hServiceMgr, lpszDriverName, SERVICE_ALL_ACCESS);
		if (hServiceDDK == NULL)
		{
			dwRtn = GetLastError();
			currentLogger->LogError2(L"Load driver error in OpenService : %d", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
	}
	bRet = StartService(hServiceDDK, NULL, NULL);
	if (!bRet)
	{
		DWORD dwRtn = GetLastError();
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_ALREADY_RUNNING)
		{
			currentLogger->LogError2(L"Load driver error in StartService : %d", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
		else
		{
			if (dwRtn == ERROR_IO_PENDING)
			{
				bRet = FALSE;
				goto BeforeLeave;
			}
			else
			{
				bRet = TRUE;
				goto BeforeLeave;
			}
		}
	}
	bRet = TRUE;
	//�뿪ǰ�رվ��
BeforeLeave:
	if (hServiceDDK) CloseServiceHandle(hServiceDDK);
	if (hServiceMgr) CloseServiceHandle(hServiceMgr);
	return bRet;
}
//ж������
//    szSvrName��������
BOOL MUnLoadKernelDriver(const wchar_t* szSvrName)
{
	if (hKDrv && wcscmp(szSvrName, L"JiYuTrainerDriver") == 0) {
		CloseHandle(hKDrv);
		hKDrv = NULL;
	}

	BOOL bDeleted = FALSE;
	BOOL bRet = FALSE;
	SC_HANDLE hServiceMgr = NULL;
	SC_HANDLE hServiceDDK = NULL;
	SERVICE_STATUS SvrSta;
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		currentLogger->LogError2(L"UnLoad driver error in OpenSCManager : %d", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}
	//����������Ӧ�ķ���
	hServiceDDK = OpenService(hServiceMgr, szSvrName, SERVICE_ALL_ACCESS);
	if (hServiceDDK == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
			currentLogger->LogWarn(L"UnLoad driver error because driver not load.");
		else currentLogger->LogError2(L"UnLoad driver error in OpenService : %d", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}
	//ֹͣ�����������ֹͣʧ�ܣ�ֻ�������������ܣ��ٶ�̬���ء� 
	if (!ControlService(hServiceDDK, SERVICE_CONTROL_STOP, &SvrSta)) {
		currentLogger->LogError2(L"UnLoad driver error in ControlService : %d", GetLastError());
	}
	//��̬ж���������� 
	if (!DeleteService(hServiceDDK)) {
		currentLogger->LogError2(L"UnLoad driver error in DeleteService : %d", GetLastError());
		bRet = FALSE;
	}
	else bDeleted = TRUE;

BeforeLeave:
	//�뿪ǰ�رմ򿪵ľ��
	if (hServiceDDK) CloseServiceHandle(hServiceDDK);
	if (hServiceMgr) CloseServiceHandle(hServiceMgr);

	if (bDeleted) bRet = MRegForceDeleteServiceRegkey((LPWSTR)szSvrName);

	return bRet;
}
//������
BOOL XOpenDriver()
{
	hKDrv = CreateFile(L"\\\\.\\JKRK",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (!hKDrv || hKDrv == INVALID_HANDLE_VALUE)
	{
		currentLogger->LogError(L"Get Kernel driver handle (CreateFile) failed : %d . ", GetLastError());
		return FALSE;
	}
	return TRUE;
}
BOOL XDriverLoaded() {
	return hKDrv != NULL;
}

BOOL XInitSelfProtect()
{
	return KFInstallSelfProtect();
}
BOOL XLoadDriver() {

	bool isWin7 = SysHlp::GetSystemVersion() == SystemVersionWindows7OrLater;
	bool isXp = SysHlp::GetSystemVersion() == SystemVersionWindowsXP;

	if (SysHlp::Is64BitOS()) {
		currentLogger->LogWarn(L"������֧�� 64λ ϵͳ");
		return FALSE;
	}
	if (!SysHlp::IsRunasAdmin() && !isXp) {
		currentLogger->LogWarn(L"Ҫ�������������Թ���Ա������б�����");
		return FALSE;
	}

	if (MLoadKernelDriver(L"JiYuTrainerDriver", currentApp->GetPartFullPath(PART_DRIVER), NULL))
	{
		if (XOpenDriver()) {
			
			ULONG sysBulidVersion = SysHlp::GetWindowsBulidVersion();

			currentLogger->Log(L"Windows Bulid version %d", sysBulidVersion);
			KFSendDriverinitParam(isXp, isWin7, sysBulidVersion);
			
			currentLogger->LogInfo(L"�������سɹ�");
			return TRUE;
		}
		else currentLogger->LogWarn2(L"�������سɹ�����������ʧ��");
	}

	return FALSE;
}
BOOL XCloseDriverHandle() {
	if (hKDrv) {
		KFUnInstallSelfProtect();
		CloseHandle(hKDrv);
		hKDrv = nullptr;
		return TRUE;
	}
	return FALSE;
}
BOOL XUnLoadDriver()
{
	if (XDriverLoaded()) 
	{
		KFBeforeUnInitDriver();
		return MUnLoadKernelDriver(L"JiYuTrainerDriver");
	}
	return TRUE;
}