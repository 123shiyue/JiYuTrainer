#include "stdafx.h"
#include "TrainerWorker.h"
#include "JiYuTrainer.h"
#include "AppPublic.h"
#include "NtHlp.h"
#include "PathHelper.h"
#include "StringHlp.h"
#include "MsgCenter.h"
#include "StringSplit.hpp"
#include "DriverLoader.h"
#include "KernelUtils.h"
#include "SysHlp.h"
#include "SettingHlp.h"

extern JTApp *currentApp;
TrainerWorkerInternal * TrainerWorkerInternal::currentTrainerWorker = nullptr;

extern NtQuerySystemInformationFun NtQuerySystemInformation;

using namespace std;

#define TIMER_RESET_PID 40115
#define TIMER_CK 40116

PSYSTEM_PROCESSES current_system_process = NULL;

TrainerWorkerInternal::TrainerWorkerInternal()
{
	currentTrainerWorker = this;
}
TrainerWorkerInternal::~TrainerWorkerInternal()
{
	if (hDesktop) {
		CloseDesktop(hDesktop);
		hDesktop = NULL;
	}

	StopInternal();
	ClearProcess();

	currentTrainerWorker = nullptr;
}

void TrainerWorkerInternal::Init()
{
	hDesktop = OpenDesktop(L"Default", 0, FALSE, DESKTOP_ENUMERATE);
	UpdateScreenSize();

	if (LocateStudentMainLocation()) JTLog(L"�Ѷ�λ������ӽ���λ�ã� %s", _StudentMainPath.c_str());
	else JTLogWarn(L"�޷���λ������ӽ���λ��");

	UpdateState();
	UpdateStudentMainInfo(false);

	InitSettings();
}
void TrainerWorkerInternal::InitSettings()
{
	SettingHlp*settings = currentApp->GetSettings();
	setAutoIncludeFullWindow = settings->GetSettingBool(L"AutoIncludeFullWindow");
	setCkInterval = currentApp->GetSettings()->GetSettingInt(L"CKInterval", 3100);
	if (setCkInterval < 1000 || setCkInterval > 10000) setCkInterval = 3000;

	if (_StudentMainControlled)
		SendMessageToVirus(L"hk:reset");
}
void TrainerWorkerInternal::UpdateScreenSize()
{
	screenWidth = GetSystemMetrics(SM_CXSCREEN);
	screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

void TrainerWorkerInternal::Start()
{
	if (!_Running) 
	{
		_Running = true;

		//Settings
		
		SetTimer(hWndMain, TIMER_CK, setCkInterval, TimerProc);
		SetTimer(hWndMain, TIMER_RESET_PID, 2000, TimerProc);

		UpdateState();

	}
}
void TrainerWorkerInternal::Stop()
{
	if (_Running) {
		StopInternal();
		UpdateState();
	}
}
void TrainerWorkerInternal::StopInternal() {
	if (_Running) {
		_Running = false;
		KillTimer(hWndMain, TIMER_CK);
		KillTimer(hWndMain, TIMER_RESET_PID);
	}
}

void TrainerWorkerInternal::SetUpdateInfoCallback(TrainerWorkerCallback *callback)
{
	if (callback) {
		_Callback = callback;
		hWndMain = callback->GetMainHWND();
	}
}

void TrainerWorkerInternal::HandleMessageFromVirus(LPCWSTR buf)
{
	wstring act(buf);
	vector<wstring> arr;
	SplitString(act, arr, L":");
	if (arr.size() >= 2)
	{
		if (arr[0] == L"hkb")
		{
			if (arr[1] == L"succ") {
				_StudentMainControlled = true;
				JTLogInfo(L"Receive ctl success message ");
				if (_Callback) _Callback->OnBeforeSendStartConf();
				UpdateState();
			}
			else if (arr[1] == L"immck") {
				RunCk();
				JTLogInfo(L"Receive  immck message ");
			}
		}
		else if (arr[0] == L"wcd")
		{
			//wwcd
			int wcdc = _wtoi(arr[1].c_str());
			if (wcdc % 20 == 0)
				JTLogInfo(L"Receive  watch dog message %d ", wcdc);
		}
	}
}
void TrainerWorkerInternal::SendMessageToVirus(LPCWSTR buf)
{
	MsgCenterSendToVirus(buf, hWndMain);
}

bool TrainerWorkerInternal::Kill(bool autoWork)
{
	if (_StudentMainPid <= 4) {
		JTLogError(L"δ�ҵ�����������");
		return false;
	}
	if (_StudentMainControlled){
		bool vkill = autoWork;
		if(!vkill){
			int drs = MessageBox(hWndMain, L"���Ƿ�ϣ��ʹ�ò������б��ƣ�", L"JiYuTrainer - ��ʾ", MB_ICONASTERISK | MB_YESNOCANCEL);
			if (drs == IDCANCEL) return false;
			else if (drs == IDYES) vkill = true;
		}
		if (vkill) {
			//Stop sginal
			SendMessageToVirus(L"ss2:0");
			return true;
		}
	}

	HANDLE hProcess;
	NTSTATUS status = MOpenProcessNt(_StudentMainPid, &hProcess);
	if (!NT_SUCCESS(status)) {
		if (status == STATUS_INVALID_CID || status == STATUS_INVALID_HANDLE) {
			_StudentMainPid = 0;
			_StudentMainControlled = false;
			UpdateState();
			UpdateStudentMainInfo(!autoWork);
			return true;
		}
		else {
			JTLogError(L"�򿪽��̴���0x%08X�����ֶ�����", status);
			return false;
		}
	}
	status = MTerminateProcessNt(0, hProcess);
	if (NT_SUCCESS(status)) {
		_StudentMainPid = 0;
		_StudentMainControlled = false;
		UpdateState();
		UpdateStudentMainInfo(!autoWork);
		CloseHandle(hProcess);
		return TRUE;
	}
	else {
		if (status == STATUS_ACCESS_DENIED) goto FORCEKILL;
		else if (status != STATUS_INVALID_CID && status != STATUS_INVALID_HANDLE) {
			JTLogError(L"�������̴���0x%08X�����ֶ�����", status);
			if (!autoWork)
				MessageBox(hWndMain, L"�޷�����������ӽ��ң�����Ҫʹ�����������ֶ�����", L"JiYuTrainer - ����", MB_ICONERROR);;
			CloseHandle(hProcess);
			return false;
		}
		else if (status == STATUS_INVALID_CID || status == STATUS_INVALID_HANDLE) {
			_StudentMainPid = 0;
			_StudentMainControlled = false;
			UpdateState();
			UpdateStudentMainInfo(!autoWork);
			CloseHandle(hProcess);
			return true;
		}
	}

FORCEKILL:
	if (DriverLoaded() && MessageBox(hWndMain, L"��ͨ�޷����������Ƿ����������������\n���������ܲ��ȶ��������á���Ҳ����ʹ�� PCHunter �Ȱ�ȫ�������ǿɱ��", L"JiYuTrainer - ��ʾ", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
	{
		if (KForceKill(_StudentMainPid, &status)) {
			_StudentMainPid = 0;
			_StudentMainControlled = false;
			UpdateState();
			UpdateStudentMainInfo(!autoWork);
			CloseHandle(hProcess);
			return true;
		}
		else if(!autoWork) MessageBox(hWndMain, L"����Ҳ�޷���������ʹ�� PCHunter �������ɣ�", L"����", MB_ICONEXCLAMATION);
	}
	CloseHandle(hProcess);
	return false;
}
bool TrainerWorkerInternal::Rerun(bool autoWork)
{
	if (!_StudentMainFileLocated) {
		JTLogWarn(L"δ�ҵ�������ӽ���");
		if (!autoWork && _Callback)
			_Callback->OnSimpleMessageCallback(L"<h5>�����޷��ڴ˼�������ҵ�������ӽ��ң�����Ҫ�ֶ�����</h5>");
		return false;
	}
	return SysHlp::RunApplication(_StudentMainPath.c_str(), NULL);
}
void* TrainerWorkerInternal::RunOperation(TrainerWorkerOp op) 
{
	switch (op)
	{
	case TrainerWorkerOpVirusBoom: {
		MsgCenterSendToVirus(L"ss:0", hWndMain);
		return nullptr;
	}
	case TrainerWorkerOpVirusQuit: {
		MsgCenterSendToVirus((LPWSTR)L"hk:ckend", hWndMain);
		return nullptr;
	}
	case TrainerWorkerOp1: {
		WCHAR s[300]; swprintf_s(s, L"hk:path:%s", currentApp->GetFullPath());
		MsgCenterSendToVirus(s, hWndMain);
		swprintf_s(s, L"hk:inipath:%s", currentApp->GetPartFullPath(PART_INI));
		MsgCenterSendToVirus(s, hWndMain);
		break;
	}
	case TrainerWorkerOpForceUnLoadVirus: {
		UnLoadAllVirus();
		break;
	}
	case TrainerWorkerOp2: {
		if (ReadTopDomanPassword()) {
			//mythware_super_password
			//155
			return (LPVOID)_TopDomainPassword.c_str();
		}
		return nullptr;
	}
	}
	return nullptr;
}

bool TrainerWorkerInternal::RunCk()
{
	_LastResolveWindowCount = 0;
	_LastResoveBroadcastWindow = false;
	_LastResoveBlackScreenWindow = false;
	_FirstBlackScreenWindow = false;

	EnumDesktopWindows(hDesktop, EnumWindowsProc, (LPARAM)this);

	MsgCenterSendHWNDS(hWndMain);

	return _LastResolveWindowCount > 0;
}
void TrainerWorkerInternal::RunResetPid()
{
	FlushProcess();

	//CK GET STAT DELAY
	if (_NextLoopGetCkStat) {
		_NextLoopGetCkStat = false;
		SendMessageToVirus(L"hk:ckstat");
	}

	//Find jiyu main process
	DWORD newPid = 0;
	if (LocateStudentMain(&newPid)) { //�ҵ�����

		if (_StudentMainPid != newPid)
		{
			_StudentMainPid = newPid;

			if (InstallVirus()) {
				_VirusInstalled = true;
				_NextLoopGetCkStat = true;

				JTLog(L"�� StudentMain.exe [%d] ע��DLL�ɹ�", newPid);
			}
			else  JTLogError(L"�� StudentMain.exe [%d] ע��DLLʧ��", newPid);

			JTLog(L"������ StudentMain.exe [%d]", newPid);

			UpdateState();
			UpdateStudentMainInfo(false);
		}
	}
	else { //û���ҵ�

		if (_StudentMainPid != 0)
		{
			_StudentMainPid = 0;

			JTLog(L"���������� StudentMain.exe ���˳�", newPid);

			UpdateState();
			UpdateStudentMainInfo(false);
		}

	}

	newPid = 0;
	if (LocateMasterHelper(&newPid)) {
		if (_MasterHelperPid != newPid)
		{
			_MasterHelperPid = newPid;
			if (InstallVirusForMaster()) JTLog(L"�� MasterHelper.exe [%d] ע��DLL�ɹ�", newPid);
			else  JTLogError(L"�� MasterHelper.exe [%d] ע��DLLʧ��", newPid);
		}
	}
	else {
		_MasterHelperPid = 0;
	}
}

bool TrainerWorkerInternal::FlushProcess()
{
	ClearProcess();

	DWORD dwSize = 0;
	NTSTATUS status = NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &dwSize);
	if (status == STATUS_INFO_LENGTH_MISMATCH && dwSize > 0)
	{
		current_system_process = (PSYSTEM_PROCESSES)malloc(dwSize);
		status = NtQuerySystemInformation(SystemProcessInformation, current_system_process, dwSize, 0);
		if (!NT_SUCCESS(status)) {
			JTLogError(L"NtQuerySystemInformation failed ! 0x%08X", status);
			return false;
		}
	}

	return true;
}
void TrainerWorkerInternal::ClearProcess()
{
	if (current_system_process) {
		free(current_system_process);
		current_system_process = NULL;
	}
}
bool TrainerWorkerInternal::FindProcess(LPCWSTR processName, DWORD * outPid)
{
	return false;
}
bool TrainerWorkerInternal::KillProcess(DWORD pid, bool force)
{
	HANDLE hProcess;
	NTSTATUS status = MOpenProcessNt(_StudentMainPid, &hProcess);
	if (!NT_SUCCESS(status)) {
		if (status == STATUS_INVALID_CID || status == STATUS_INVALID_HANDLE) {
			JTLogError(L"�Ҳ������� [%d] ", pid);
			return true;
		}
		else {
			JTLogError(L"�򿪽��� [%d] ����0x%08X�����ֶ�����", pid);
			return false;
		}
	}
	status = MTerminateProcessNt(0, hProcess);
	if (NT_SUCCESS(status)) {
		JTLog(L"���� [%d] �����ɹ�", pid);
		CloseHandle(hProcess);
		return TRUE;
	}
	else {
		if (status == STATUS_ACCESS_DENIED) {
			if (force) goto FORCEKILL;
			else JTLogError(L"�������� [%d] ���󣺾ܾ����ʡ��ɳ���ʹ����������", pid);
			CloseHandle(hProcess);
		}
		else if (status != STATUS_INVALID_CID && status != STATUS_INVALID_HANDLE) {
			JTLogError(L"�������� [%d] ����0x%08X�����ֶ�����", pid);
			CloseHandle(hProcess);
			return false;
		}
		else if (status == STATUS_INVALID_CID || status == STATUS_INVALID_HANDLE) {
			JTLogError(L"�Ҳ������� [%d] ", pid);
			CloseHandle(hProcess);
			return true;
		}
	}
FORCEKILL:
	if (DriverLoaded())
	{
		if (KForceKill(_StudentMainPid, &status)) {
			JTLog(L"���� [%d] ǿ�ƽ����ɹ�", pid);
			CloseHandle(hProcess);
			return true;
		}
		else {
			JTLogError(L"����ǿ�ƽ������� [%d] ����0x%08X", pid);
		}
	}
	else JTLog(L"����δ���أ��޷�ǿ�ƽ�������");
	CloseHandle(hProcess);
	return false;
}
bool TrainerWorkerInternal::ReadTopDomanPassword()
{
	_TopDomainPassword.clear();
	//��ͨע����ȡ��������4.0�汾

	HKEY hKey;
	LRESULT lastError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\TopDomain\\e-Learning Class Standard\\1.00", 0, KEY_WOW64_64KEY | KEY_READ, &hKey);
	if (lastError == ERROR_SUCCESS) {
		DWORD dwType = REG_SZ;
		WCHAR Data[32];
		DWORD cbData = 32;
		lastError = RegQueryValueEx(hKey, L"UninstallPasswd", 0, &dwType, (LPBYTE)Data, &cbData);
		RegCloseKey(hKey);

		if (lastError == ERROR_SUCCESS) {
			if (StrEqual(Data, L"Passwd[123456]")) goto READ_EX; //6.0�Ժ��ȡ�����ˣ�����ʾPasswd[123456]�����µķ�����ȡ
			else {
				_TopDomainPassword = Data;
				_TopDomainPassword = _TopDomainPassword.substr(6, _TopDomainPassword.size() - 6);
				return true;
			}
		}
	}

	//HKEY_LOCAL_MACHINE\SOFTWARE\TopDomain\e-Learning Class\Student Knock1
READ_EX:
	lastError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\TopDomain\\e-Learning Class\\Student", 0, KEY_WOW64_64KEY | KEY_READ, &hKey);
	if (lastError == ERROR_SUCCESS) {

		DWORD dwType = REG_BINARY;
		BYTE Data[120];
		DWORD cbData = 120;
		lastError = RegQueryValueEx(hKey, L"Knock1", 0, &dwType, (LPBYTE)Data, &cbData);
		if (lastError == ERROR_SUCCESS) {

			RegCloseKey(hKey);

			WCHAR ss[34] = { 0 };
			if (UnDecryptJiyuKnock(Data, cbData, ss)) {
				_TopDomainPassword = ss;
				return true;
			}
			else return false;
		}
		else JTLogWarn(L"RegQueryValueEx Failed : %d [in %s]", lastError, L"ReadTopDomanUnInstallPassword RegQueryValueEx(hKey, L\"Knock1\", 0, &dwType, (LPBYTE)szData, &dwSize);");
		RegCloseKey(hKey);
	}
	else JTLogWarn(L"RegOpenKeyEx Failed : %d [in %s]", lastError, L"ReadTopDomanUnInstallPassword");
	return false;
}
bool TrainerWorkerInternal::LocateStudentMainLocation()
{
	//ע������ ���� ·��
	HKEY hKey;
	LRESULT lastError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\e-Learing Class V6.0", 0, KEY_WOW64_64KEY | KEY_READ, &hKey);
	if (lastError == ERROR_SUCCESS) {

		DWORD dwType = REG_SZ;
		WCHAR szData[MAX_PATH];
		DWORD dwSize = MAX_PATH * sizeof(WCHAR);
		lastError = RegQueryValueEx(hKey, L"DisplayIcon", 0, &dwType, (LPBYTE)szData, &dwSize);
		if (lastError == ERROR_SUCCESS) {
			if (Path::Exists(szData)) {
				_StudentMainPath = szData;
				_StudentMainFileLocated = true;
				return true;
			}
			else JTLog(L"��ȡע��� [DisplayIcon] �����һ����Ч�ļ�����ӽ���·�� : %s", szData);
		}
		else JTLogWarn(L"RegQueryValueEx Failed : %d [in %s]", lastError, L"LocateStudentMainLocation RegQueryValueEx(hKey, L\"DisplayIcon\", 0, &dwType, (LPBYTE)szData, &dwSize);");

		RegCloseKey(hKey);
	}
	else JTLogWarn(L"RegOpenKeyEx Failed : %d [in %s]", lastError, L"LocateStudentMainLocation");

	//ֱ�ӳ��Բ���
	LPCWSTR mabeInHere[4] = {
		L"c:\\Program Files\\Mythware\\������ù���ϵͳ���V6.0 2016 ������\\StudentMain.exe",
		L"C:\\Program Files\\Mythware\\e-Learning Class\\StudentMain.exe",
		L"C:\\e-Learning Class\\StudentMain.exe",
		L"c:\\������ù���ϵͳ���V6.0 2016 ������\\StudentMain.exe",
	};
	for (int i = 0; i < 4; i++) {
		if (Path::Exists(mabeInHere[i])) {
			_StudentMainPath = mabeInHere[i];
			_StudentMainFileLocated = true;
			return true;
		}
	}

	return false;
}
bool TrainerWorkerInternal::LocateStudentMain(DWORD *outFirstPid)
{
	if (current_system_process)
	{
		bool done = false;
		for (PSYSTEM_PROCESSES p = current_system_process; !done; p = PSYSTEM_PROCESSES(PCHAR(p) + p->NextEntryOffset)) {
			if (p->ImageName.Length && StrEqual(p->ImageName.Buffer, L"StudentMain.exe"))
			{
				if (outFirstPid)*outFirstPid = (DWORD)p->ProcessId;
				if (!_StudentMainFileLocated) {
					//ֱ��ͨ��EXEȷ������λ��
					HANDLE hProcess;
					if (NT_SUCCESS(MOpenProcessNt((DWORD)p->ProcessId, &hProcess))) {
						WCHAR buffer[MAX_PATH];
						if (MGetProcessFullPathEx(hProcess, buffer)) {
							_StudentMainPath = buffer;
							_StudentMainFileLocated = true;
							JTLog(L"ͨ������ StudentMain.exe [%d] ��λ��λ�ã� %s", (DWORD)p->ProcessId, _StudentMainPath);
							CloseHandle(hProcess);
							return true;
						}
						CloseHandle(hProcess);
					}
				}

				return true;
			}
			done = p->NextEntryOffset == 0;
		}
	}
	return false;
}
bool TrainerWorkerInternal::LocateMasterHelper(DWORD *outFirstPid)
{
	if (current_system_process)
	{
		bool done = false;
		for (PSYSTEM_PROCESSES p = current_system_process; !done; p = PSYSTEM_PROCESSES(PCHAR(p) + p->NextEntryOffset)) {
			if (p->ImageName.Length && StrEqual(p->ImageName.Buffer, L"MasterHelper.exe"))
			{
				if (outFirstPid)*outFirstPid = (DWORD)p->ProcessId;
				return true;
			}
			done = p->NextEntryOffset == 0;
		}
	}
	return false;
}

void TrainerWorkerInternal::UpdateStudentMainInfo(bool byUser)
{
	if (_Callback)
		_Callback->OnUpdateStudentMainInfo(_StudentMainPid > 4, _StudentMainPath.c_str(), _StudentMainPid, byUser);
}
void TrainerWorkerInternal::UpdateState()
{
	if (_Callback) 
	{
		TrainerWorkerCallback::TrainerStatus status;
		if (_StudentMainPid > 4) {
			if (_StudentMainControlled) {
				_StatusTextMain = L"�ѿ��Ƽ�����ӽ���";

				if (!_Running) {
					_StatusTextMain += L" ��������δ����";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusStopped;
				}
				else if (_LastResoveBlackScreenWindow)
				{
					_StatusTextMore = L"�ѹرռ�����ӽ��Һ�������<br/>�����Է��ļ������Ĺ���";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusControlledAndUnLocked;
				}
				else {
					_StatusTextMore = L"�����Է��ļ������Ĺ���";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusControlled;
				}
			}
			else {
				_StatusTextMain = L"�޷����Ƽ�����ӽ���";
				if (!_Running) {
					_StatusTextMain = L"������δ����";
					_StatusTextMore = L"�����ֶ�ֹͣ������<br / >��ǰ����Լ������κβ���";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusStopped;
				}
				else if (_VirusInstalled) {
					_StatusTextMore = L"���Ѳ��뼫�򣬵�δ��������<br / ><span style=\"color:#f41702\">��������Ҫ������������</span>";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusUnknowProblem;
				}
				else {
					_StatusTextMore = L"������ӽ��Ҳ��벡��ʧ��<br / >����������鿴 <a id=\"link_log\">��־</a>";
					status = TrainerWorkerCallback::TrainerStatus::TrainerStatusControllFailed;
				}
			}
		}
		else {
			_StatusTextMain = L"������ӽ���δ����";
			if (!_Running) {
				_StatusTextMain = L"������ӽ���δ���� ���ҿ�����δ����";
				_StatusTextMore = L"�����ֶ�ֹͣ������<br / >��ǰ�����⼫�������";
				status = TrainerWorkerCallback::TrainerStatus::TrainerStatusStopped;
			}
			else if (_StudentMainFileLocated) {
				status = TrainerWorkerCallback::TrainerStatus::TrainerStatusNotRunning;
				_StatusTextMore = L"���ڴ˼�������ҵ�������ӽ���<br / >����Ե�� <b>�·���ť< / b> ������";
			}
			else {
				status = TrainerWorkerCallback::TrainerStatus::TrainerStatusNotFound;
				_StatusTextMore = L"δ�ڴ˼�������ҵ�������ӽ���";
			}
		}

		_Callback->OnUpdateState(status, _StatusTextMain.c_str(), _StatusTextMore.c_str());
	}
}

bool TrainerWorkerInternal::InstallVirus()
{
	return InjectDll(_StudentMainPid, currentApp->GetPartFullPath(PART_HOOKER));
}
bool TrainerWorkerInternal::InstallVirusForMaster()
{
	return InjectDll(_MasterHelperPid, currentApp->GetPartFullPath(PART_HOOKER));
}
bool TrainerWorkerInternal::InjectDll(DWORD pid, LPCWSTR dllPath)
{
	HANDLE hRemoteProcess;
	//�򿪽���
	NTSTATUS ntStatus = MOpenProcessNt(pid, &hRemoteProcess);
	if (!NT_SUCCESS(ntStatus)) {
		JTLogError(L"ע�벡��ʧ�ܣ��򿪽���ʧ�ܣ�0x%08X", ntStatus);
		return FALSE;
	}

	wchar_t *pszLibFileRemote;

	//ʹ��VirtualAllocEx������Զ�̽��̵��ڴ��ַ�ռ����DLL�ļ����ռ�
	pszLibFileRemote = (wchar_t *)VirtualAllocEx(hRemoteProcess, NULL, sizeof(wchar_t) * (lstrlen(dllPath) + 1), MEM_COMMIT, PAGE_READWRITE);

	//ʹ��WriteProcessMemory������DLL��·����д�뵽Զ�̽��̵��ڴ�ռ�
	WriteProcessMemory(hRemoteProcess, pszLibFileRemote, (void *)dllPath, sizeof(wchar_t) * (lstrlen(dllPath) + 1), NULL);

	//##############################################################################
		//����LoadLibraryA����ڵ�ַ
	PTHREAD_START_ROUTINE pfnStartAddr = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "LoadLibraryW");
	//(����GetModuleHandle������GetProcAddress����)

	//����Զ���߳�LoadLibraryW��ͨ��Զ���̵߳��ô����µ��߳�
	HANDLE hRemoteThread;
	if ((hRemoteThread = CreateRemoteThread(hRemoteProcess, NULL, 0, pfnStartAddr, pszLibFileRemote, 0, NULL)) == NULL)
	{
		JTLogError(L"ע���߳�ʧ��! ����CreateRemoteThread %d", GetLastError());
		return FALSE;
	}

	// �ͷž��

	CloseHandle(hRemoteProcess);
	CloseHandle(hRemoteThread);

	return true;
}
bool TrainerWorkerInternal::UnInjectDll(DWORD pid, LPCWSTR moduleName)
{
	HANDLE hProcess;
	//�򿪽���
	NTSTATUS ntStatus = MOpenProcessNt(pid, &hProcess);
	if (!NT_SUCCESS(ntStatus)) {
		JTLogError(L"ж�ز���ʧ�ܣ��򿪽���ʧ�ܣ�0x%08X", ntStatus);
		return FALSE;
	}
	DWORD pszLibFileRemoteSize = sizeof(wchar_t) * (lstrlen(moduleName) + 1);
	wchar_t *pszLibFileRemote;
	//ʹ��VirtualAllocEx������Զ�̽��̵��ڴ��ַ�ռ����DLL�ļ����ռ�
	pszLibFileRemote = (wchar_t *)VirtualAllocEx(hProcess, NULL, pszLibFileRemoteSize, MEM_COMMIT, PAGE_READWRITE);
	//ʹ��WriteProcessMemory������DLL��·����д�뵽Զ�̽��̵��ڴ�ռ�
	WriteProcessMemory(hProcess, pszLibFileRemote, (void *)moduleName, pszLibFileRemoteSize, NULL);

	DWORD dwHandle;
	DWORD dwID;
	LPVOID pFunc = GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "GetModuleHandleW");
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFunc, pszLibFileRemote, 0, &dwID);
	if (!hThread) {
		JTLogError(L"ж�ز���ʧ�ܣ�����Զ���߳�ʧ�ܣ�%d", GetLastError());
		return FALSE;
	}

	// �ȴ�GetModuleHandle�������
	WaitForSingleObject(hThread, INFINITE);
	// ���GetModuleHandle�ķ���ֵ
	GetExitCodeThread(hThread, &dwHandle);
	// �ͷ�Ŀ�����������Ŀռ�
	VirtualFreeEx(hProcess, pszLibFileRemote, pszLibFileRemoteSize, MEM_DECOMMIT);
	CloseHandle(hThread);
	// ʹĿ����̵���FreeLibrary��ж��DLL
	pFunc = GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "FreeLibrary"); ;
	hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFunc, (LPVOID)dwHandle, 0, &dwID);
	if (!hThread) {
		JTLogError(L"ж�ز���ʧ�ܣ�����Զ���߳�ʧ�ܣ�%d", GetLastError());
		return FALSE;
	}
	
	// �ȴ�FreeLibraryж�����
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	CloseHandle(hProcess);

	return true;
}
bool TrainerWorkerInternal::UnLoadAllVirus()
{
	if (_MasterHelperPid > 4) {
		if (UnInjectDll(_MasterHelperPid, L"JiYuTrainerHooks.dll"))
			JTLog(L"��ǿ��ж�� MasterHelper ����");
		//KillProcess(_MasterHelperPid, false);
	}
	if (_StudentMainPid > 4)
		if (UnInjectDll(_StudentMainPid, L"JiYuTrainerHooks.dll"))
			JTLog(L"��ǿ��ж�� StudentMain ����");

	return false;
}

void TrainerWorkerInternal::SwitchFakeFull()
{
	if (_FakeFull)_FakeFull = false;
	else _FakeFull = true;
	FakeFull(_FakeFull);
}
void TrainerWorkerInternal::FakeFull(bool fk) {
	if (_CurrentBroadcastWnd) {
		if (fk) {
			SetWindowLong(_CurrentBroadcastWnd, GWL_EXSTYLE, GetWindowLong(_CurrentBroadcastWnd, GWL_EXSTYLE) | WS_EX_TOPMOST);
			SetWindowLong(_CurrentBroadcastWnd, GWL_STYLE, GetWindowLong(_CurrentBroadcastWnd, GWL_STYLE) ^ (WS_BORDER | WS_OVERLAPPEDWINDOW));
			SetWindowPos(_CurrentBroadcastWnd, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
			SendMessage(_CurrentBroadcastWnd, WM_SIZE, 0, MAKEWPARAM(screenWidth, screenHeight));
			/*HWND jiYuGBDeskRdWnd = FindWindowExW(currentGbWnd, NULL, NULL, L"TDDesk Render Window");
			if (jiYuGBDeskRdWnd != NULL) {

			}*/
			_FakeBroadcastFull = true;
			JTLog(L"�����㲥���ڼ�װȫ��״̬");
		}
		else {
			_FakeBroadcastFull = false;
			FixWindow(_CurrentBroadcastWnd, (LPWSTR)L"");
			int w = (int)((double)screenWidth * (3.0 / 4.0)), h = (int)((double)screenHeight * (double)(4.0 / 5.0));
			SetWindowPos(_CurrentBroadcastWnd, 0, (screenWidth - w) / 2, (screenHeight - h) / 2, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
			JTLog(L"ȡ���㲥���ڼ�װȫ��״̬");
		}
	}
	if (_CurrentBlackScreenWnd) {
		if (fk) {
			SetWindowLong(_CurrentBlackScreenWnd, GWL_EXSTYLE, GetWindowLong(_CurrentBlackScreenWnd, GWL_EXSTYLE) | WS_EX_TOPMOST);
			SetWindowLong(_CurrentBlackScreenWnd, GWL_STYLE, GetWindowLong(_CurrentBlackScreenWnd, GWL_STYLE) ^ (WS_BORDER | WS_OVERLAPPEDWINDOW));
			SetWindowPos(_CurrentBlackScreenWnd, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
			SendMessage(_CurrentBlackScreenWnd, WM_SIZE, 0, MAKEWPARAM(screenWidth, screenHeight));
			SendMessage(_CurrentBlackScreenWnd, WM_SIZE, 0, MAKEWPARAM(screenWidth, screenHeight));
			_FakeBlackScreenFull = true;
			JTLog(L"�����������ڼ�װȫ��״̬");
		}
		else {
			_FakeBlackScreenFull = false;
			FixWindow(_CurrentBlackScreenWnd, (LPWSTR)L"BlackScreen Window");
			JTLog(L"ȡ���������ڼ�װȫ��״̬");
		}
	}
	if (!fk && !_CurrentBlackScreenWnd && !_CurrentBroadcastWnd && (_FakeBlackScreenFull || _FakeBroadcastFull)) {
		_FakeBroadcastFull = false;
		_FakeBlackScreenFull = false;
	}
}
bool TrainerWorkerInternal::ChecIsJIYuWindow(HWND hWnd, LPDWORD outPid, LPDWORD outTid) {
	if (_StudentMainPid == 0) return false;
	DWORD pid = 0, tid = GetWindowThreadProcessId(hWnd, &pid);
	if (outPid) *outPid = pid;
	if (outTid) *outTid = tid;
	return pid == _StudentMainPid;
}
bool TrainerWorkerInternal::CheckIsTargetWindow(LPWSTR text, HWND hWnd) {
	bool b = false;
	if (StrEqual(text, L"��Ļ�㲥") || StrEqual(text, L"��Ļ�ݲ��Ҵ���")) {
		b = true;
		_LastResoveBroadcastWindow = true;
		_CurrentBroadcastWnd = hWnd;
		if (_FakeBroadcastFull) return false;
	}
	if (StrEqual(text, L"BlackScreen Window")) {
		b = true;
		_LastResoveBlackScreenWindow = true;
		if (!_FirstBlackScreenWindow) {
			_FirstBlackScreenWindow = true;
			if (_Callback) _Callback->OnResolveBlackScreenWindow();
			//ShowTip(L"���ּ���ķǷ��������ڣ�", L"�ѽ��䴦���رգ������Լ������Ĺ�����", 10);
		}
		_CurrentBlackScreenWnd = hWnd;
		if (_FakeBlackScreenFull) return false;
	}
	return b;
}
void TrainerWorkerInternal::FixWindow(HWND hWnd, LPWSTR text)
{
	_LastResolveWindowCount++;
	//Un top
	LONG oldLong = GetWindowLong(hWnd, GWL_EXSTYLE);
	if ((oldLong & WS_EX_TOPMOST) == WS_EX_TOPMOST)
	{
		SetWindowLong(hWnd, GWL_EXSTYLE, oldLong ^ WS_EX_TOPMOST);
		SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	//Set border and sizeable
	SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) | (WS_BORDER | WS_OVERLAPPEDWINDOW));

	if (StrEqual(text, L"BlackScreen Window"))
	{
		oldLong = GetWindowLong(hWnd, GWL_EXSTYLE);

		{
			SetWindowLong(hWnd, GWL_EXSTYLE, oldLong ^ WS_EX_APPWINDOW | WS_EX_NOACTIVATE);
			SetWindowPos(hWnd, 0, 20, 20, 90, 150, SWP_NOZORDER | SWP_DRAWFRAME | SWP_NOACTIVATE);
			ShowWindow(hWnd, SW_HIDE);
		}
	}

	SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOACTIVATE);
}

BOOL CALLBACK TrainerWorkerInternal::EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	TrainerWorkerInternal *self =(TrainerWorkerInternal *)lParam;
	if (IsWindowVisible(hWnd) && self->ChecIsJIYuWindow(hWnd, NULL, NULL)) {
		WCHAR text[32];
		GetWindowText(hWnd, text, 32);
		if (StrEqual(text, L"JiYu Trainer Virus Window")) return TRUE;

		RECT rc;
		GetWindowRect(hWnd, &rc);
		if (self->CheckIsTargetWindow(text, hWnd)) {
			//JiYu window
			MsgCenteAppendHWND(hWnd);
			self->FixWindow(hWnd, text);
		}
		else if (self->setAutoIncludeFullWindow && rc.top == 0 && rc.left == 0 && rc.right == self->screenWidth && rc.bottom == self->screenHeight) {
			//Full window
			MsgCenteAppendHWND(hWnd);
			self->FixWindow(hWnd, text);
		}
	}
	return TRUE;
}
VOID TrainerWorkerInternal::TimerProc(HWND hWnd, UINT message, UINT_PTR iTimerID, DWORD dwTime)
{
	if (currentTrainerWorker != nullptr) 
	{
		if (iTimerID == TIMER_RESET_PID) {
			currentTrainerWorker->RunResetPid();
		}
		if (iTimerID == TIMER_CK) {
			currentTrainerWorker->RunCk();
		}
	}
}

bool UnDecryptJiyuKnock(BYTE* Data, DWORD cbData, WCHAR* ss)
{
	//������Ĵ���
	__try {
		DWORD v5; // esi
		DWORD v6; // ecx
		BYTE *v7; // eax
		DWORD v8; // edx
		BYTE *i;
		v5 = cbData;
		v6 = cbData >> 2;
		v7 = Data;
		if (cbData >> 2)
		{
			v8 = cbData >> 2;
			do
			{
				*(DWORD *)v7 ^= 0x50434C45u;
				v7 += 4;
				--v8;
			} while (v8);
		}
		for (i = Data; v6; --v6)
		{
			*(DWORD *)i ^= 0x454C4350u;
			i += 4;
		}
		WORD v4[34];
		v4[0] = 0;
		memset(&v4[1], 0, 0x40u);

		int a1 = (int)&v4;

		int v13; // edi
		BYTE *v14; // eax
		__int16 v15; // cx
		v13 = a1 - Data[0];
		v14 = &Data[Data[0]];
		do
		{
			v15 = *(WORD *)v14;
			*(WORD *)&v14[v13 - (DWORD)Data] = *(WORD *)v14;
			v14 += 2;
		} while (v15);


		for (int i = 0; i < 32; i++) ss[i] = v4[i];
		return true;
	}
	__except (1) {
		return false;
	}
}
