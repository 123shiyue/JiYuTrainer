#include "stdafx.h"
#include "resource.h"
#include "JiYuTrainer.h"
#include "App.h"
#include "PathHelper.h"
#include "MD5Utils.h"
#include "SysHlp.h"
#include "StringHlp.h"
#include "NtHlp.h"
#include "KernelUtils.h"
#include "DriverLoader.h"
#include <Shlwapi.h>
#include <winioctl.h>
#include <CommCtrl.h>
#include <ShellAPI.h>

extern LoggerInternal * currentLogger;

JTAppInternal::JTAppInternal(HINSTANCE hInstance)
{
	this->hInstance = hInstance;
}
JTAppInternal::~JTAppInternal()
{
	ExitClear();
}

int JTAppInternal::CheckMd5()
{
	FILE *fp = NULL;
	std::wstring output(fullDir);
	output += L"\\JiYuTrainerMd5Result.txt";
	_wfopen_s(&fp, output.c_str(), L"w");
	if (fp) {
		for (int i = 1; i < PART_COUNT; i++) {
			LPCWSTR path = parts[i].c_str();
			if (partsResId[i] != 0 && Path::Exists(path)) {
				std::wstring*md5 = MD5Utils::GetFileMD5(path);
				fwprintf_s(fp, L"#define %s L\"%s\"\n", partsMd5CheckNames[i], md5->c_str());
				FreeStringPtr(md5);
			}
		}
		fclose(fp);
	}
	MessageBox(0, output.c_str(), L"JiYuTrainer MD5", 0);
	return 0;
}
int JTAppInternal::CheckInstall(APP_INSTALL_MODE mode)
{
	FILE *fp = NULL;
	bool startBatCreated = false;
	WCHAR installDir[MAX_PATH];

	if (SysHlp::CheckIsPortabilityDevice(fullPath.c_str()) || SysHlp::CheckIsDesktop(fullDir.c_str())) {//��usb�豸����������
		WCHAR sysTempPath[MAX_PATH + 1];//��TEMPĿ¼��װ
		GetTempPath(MAX_PATH, sysTempPath);
		wcscpy_s(installDir, sysTempPath);
		wcscat_s(installDir, L"JiYuTrainer");
		if (!Path::Exists(installDir) && !CreateDirectory(installDir, NULL)) {
			wcscpy_s(installDir, fullDir.c_str());
			MessageBox(NULL, L"�޷�������ʱĿ¼���볢��ʹ�ù���Ա������б�����", L"����", MB_ICONERROR);
			ExitProcess(-1);
		}
		appStartType = AppStartTypeInTemp;
	}
	else {//����·��
		wcscpy_s(installDir, fullDir.c_str());//��ǰĿ¼
		appStartType = AppStartTypeNormal;
	}

	//ƴ��·���ַ���
	for (int i = 0; i < PART_COUNT; i++) 
		parts[i] = std::wstring(installDir) + L"\\" + parts[i];

	if (mode == AppInstallNew) {
		Sleep(1500);//Sleep for a while
	}

	if (mode == AppInstallNew || appNeedInstallIniTemple)
	{		
		if (Path::GetFileName(fullPath) != L"JiYuTrainerUpdater.exe") {		
			//��װ ini ģ��
			_wfopen_s(&fp, fullIniPath.c_str(), L"w");
			if (fp) {
				fwprintf_s(fp, L"[JTArgeement]");
				fwprintf_s(fp, L"\nArgeed=TRUE");
				fwprintf_s(fp, L"\n[JTSettings]");
				fwprintf_s(fp, L"\nLastCheckUpdateTime=5/10");
				fwprintf_s(fp, L"\nTopMost=FALSE");
				fwprintf_s(fp, L"\nAutoIncludeFullWindow=FALSE");
				fwprintf_s(fp, L"\nAllowAllRunOp=FALSE");
				fwprintf_s(fp, L"\nAutoUpdate=TRUE");
				fwprintf_s(fp, L"\nCKInterval=3100");
				fwprintf_s(fp, L"\nAutoForceKill=FALSE");
				fwprintf_s(fp, L"\nDisableDriver=FALSE");
				fwprintf_s(fp, L"\nSelfProtect=TRUE");
				fwprintf_s(fp, L"\nBandAllRunOp=FALSE");
				fclose(fp);
				fp = NULL;
			}
		}
	}

	//��ģ��
	for (int i = 1; i < PART_COUNT; i++) {
		LPCWSTR path = parts[i].c_str();

		if (partsResId[i] != 0)
		{
			if (!Path::Exists(path))
			{
				if (InstallResFile(hInstance, MAKEINTRESOURCE(partsResId[i]), L"BIN", path) != ExtractSuccess) {
					FAST_STR_BINDER(str, L"��װ���� %s ʱ�����������볢���������ط���װ������", 400, path);
					MessageBox(NULL, str, APP_TITLE, MB_ICONERROR);
					return -1;
				}
			}
			else if ((mode == AppInstallNew || appArgForceCheckFileMd5) && !StrEmepty(partsMd5Checks[i])) {//���ԭ���ļ��汾�Ƿ�һ�£�����ɾ���������ļ�
				std::wstring *fileMd5 = MD5Utils::GetFileMD5(path);
				bool needReplace = (!fileMd5->empty() && (*fileMd5) != partsMd5Checks[i]);
				FreeStringPtr(fileMd5);
				if (needReplace) {//�滻
					if (!DeleteFileW(path) && InstallResFile(hInstance, MAKEINTRESOURCE(partsResId[i]), L"BIN", path) != ExtractSuccess) {
						if (i != PART_HOOKER) {
							FAST_STR_BINDER(str, L"���²��� %s ʱ���������볢���������ط���װ������", 400, path);
							MessageBox(NULL, str, APP_TITLE, MB_ICONERROR);
							return -1;
						}
					}
				}
			}
		} 
	}

	//��ģ��
	if (!Path::Exists(parts[PART_MAIN]))
	{
		if (!CopyFile(fullPath.c_str(), parts[PART_MAIN].c_str(), FALSE)) {
			MessageBox(NULL, L"��װ������ʱ�����������볢���������ط���װ������", APP_TITLE, MB_ICONERROR);
			return -1;
		}
	}
	else if (mode == AppInstallNew || appArgForceCheckFileMd5) {//���ԭ���ļ��汾�Ƿ�һ�£�����ɾ���������ļ�
		std::wstring *fileMd5 = MD5Utils::GetFileMD5(parts[PART_MAIN].c_str());
		std::wstring *thisMd5 = MD5Utils::GetFileMD5(fullPath.c_str());
		bool needReplace = (!fileMd5->empty() && !thisMd5->empty() && (*fileMd5) != (*thisMd5));
		FreeStringPtr(fileMd5);
		if (needReplace){//�滻
			if (!DeleteFileW(parts[PART_MAIN].c_str()) || !CopyFile(fullPath.c_str(), parts[PART_MAIN].c_str(), FALSE)) {
				MessageBox(NULL, L"��װ������ʱ�����������볢���������ط���װ������", APP_TITLE, MB_ICONERROR);
				return -1;
			}
		}
	}

	//��TempĿ¼��װ����Ҫ��������cmd
	if (appStartType == AppStartTypeInTemp) {

		_wfopen_s(&fp, parts[PART_RUN].c_str(), L"w");
		if (fp) {
			if (mode == AppInstallNew) {
				WCHAR delThisArg[MAX_PATH]; swprintf_s(delThisArg, L"-ia -rc %s", fullPath.c_str());
				fwprintf_s(fp, L"start \"\" \"%s\" %s\n", parts[PART_MAIN].c_str(), MakeFromSourceArg(delThisArg));
			}
			else  fwprintf_s(fp, L"start \"\" \"%s\" %s\n", parts[PART_MAIN].c_str(), MakeFromSourceArg(L""));
			fclose(fp);
			fp = NULL;

			startBatCreated = true;
		}
	}

	//����ж��cmd
	if (!Path::Exists(parts[PART_UNINSTALL]))
	{
		_wfopen_s(&fp, parts[PART_UNINSTALL].c_str(), L"w");
		if (fp) {
			fwprintf_s(fp, L"@echo off\n@ping 127.0.0.1 - n 6 > nul\n");
			int start = (appStartType == AppStartTypeInTemp ? 0 : 1); //�Ƿ���TempĿ¼�����Ƿ�ɾ��exe����
			for (int i = start; i < PART_COUNT; i++) {
				if (i != PART_UNINSTALL) {
					fwprintf_s(fp, L"del /F /Q %s\n", parts[i].c_str());
				}
			}
			if (start == 0) {
				WCHAR pathBuffer[MAX_PATH]; //Target ini
				wcscpy_s(pathBuffer, parts[PART_MAIN].c_str());
				PathRenameExtension(pathBuffer, L".ini");
				fwprintf_s(fp, L"del /F /Q %s\n", pathBuffer);
			}
			fwprintf_s(fp, L"del %%0\n");
			fclose(fp);
		}
	}

	if (mode == AppInstallNew) 
	{
		//�°�װ��Ҫ����Դ��װ��
		if (Path::Exists(fullSourceInstallerPath)) {

			if (!DeleteFile(fullSourceInstallerPath.c_str()) || !CopyFile(fullPath.c_str(), fullSourceInstallerPath.c_str(), TRUE))
				JTLogError(L"����Դ��װ�� %s ʱ��������%d", fullSourceInstallerPath.c_str(), GetLastError());
		}

		//�°�װ����Ժ����������򣬲�ɾ������
		if (appStartType == AppStartTypeInTemp) 
			SysHlp::RunApplicationPriviledge(parts[PART_RUN].c_str(), NULL);
		else if (appStartType == AppStartTypeNormal) {
			WCHAR delThisArg[MAX_PATH]; swprintf_s(delThisArg, L"-ia -rc %s", fullPath.c_str());
			SysHlp::RunApplicationPriviledge(parts[PART_MAIN].c_str(), MakeFromSourceArg(delThisArg));
		}
	}

	return 0;
}

EXTRACT_RES JTAppInternal::InstallResFile(HINSTANCE resModule, LPWSTR resId, LPCWSTR resType, LPCWSTR extractTo)
{
	EXTRACT_RES result = ExtractUnknow;
	HANDLE hFile = CreateFile(extractTo, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		result = ExtractCreateFileError;
		return result;
	}

	HRSRC hResource = FindResourceW(resModule, resId, resType);
	if (hResource) {
		HGLOBAL hg = LoadResource(resModule, hResource);
		if (hg) {
			LPVOID pData = LockResource(hg);
			if (pData)
			{
				DWORD dwSize = SizeofResource(resModule, hResource);
				DWORD writed;
				if (WriteFile(hFile, pData, dwSize, &writed, NULL)) result = ExtractSuccess;
				else result = ExtractWriteFileError;;
			}
			else result = ExtractReadResError;
		}
		else result = ExtractReadResError;
	}
	else result = ExtractReadResError;
	CloseHandle(hFile);

	return result;
}
bool JTAppInternal::IsCommandExists(LPCWSTR cmd)
{
	return FindArgInCommandLine(appArgList, appArgCount, cmd) >= 0;
}
int JTAppInternal::FindArgInCommandLine(LPWSTR *szArgList, int argCount, const wchar_t * arg) {
	for (int i = 0; i < argCount; i++) {
		if (wcscmp(szArgList[i], arg) == 0)
			return i;
	}
	return -1;
}

LPCWSTR JTAppInternal::MakeFromSourceArg(LPCWSTR arg)
{
	if (!fullSourceInstallerPath.empty()) {
		fullArgBuffer = L"-f " + fullSourceInstallerPath + L" " + arg;
		return fullArgBuffer.c_str();
	}
	return arg;
}

int JTAppInternal::Run()
{
	appResult = RunInternal();
	this->Exit(appResult);
	return appResult;
}
int JTAppInternal::RunCheckRunningApp()//��������Ѿ���һ�������У��򷵻�true
{
	HWND oldWindow = FindWindow(L"sciter-jytrainer-main-window", L"JiYu Trainer Main Window");
	if (oldWindow != NULL) {
		if (!IsWindowVisible(oldWindow)) ShowWindow(oldWindow, SW_SHOW);
		if (IsIconic(oldWindow)) ShowWindow(oldWindow, SW_RESTORE);
		SetForegroundWindow(oldWindow);
		return -1;
	}
	HANDLE  hMutex = CreateMutex(NULL, FALSE, L"JYTMutex");
	if (hMutex && (GetLastError() == ERROR_ALREADY_EXISTS))
	{
		CloseHandle(hMutex);
		hMutex = NULL;
		return 1;
	}
	return 0;
}
bool JTAppInternal::RunArgeementDialog()
{
	int rs = DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_DIALOG_ARGEEMENT), NULL, ArgeementWndProc);
	if (rs == IDYES) {
		appSetting->SetSettingBool(L"Argeed", true, L"JTArgeement");
		return true;
	}
	return false;
}
int JTAppInternal::RunInternal()
{
	setlocale(LC_ALL, "chs");

	if (SysHlp::GetSystemVersion() == SystemVersionNotSupport) {
		MessageBox(NULL, L"���б��������Ҫ�� Windows XP����ʹ�ø��߰汾��ϵͳ", L"JiYuTrainer - ����", MB_ICONERROR);
		return 0;
	}

	MLoadNt();

	InitPrivileges();
	InitLogger();
	InitPath();
	InitCommandLine();
	InitArgs();
	InitSettings();

	if (appArgBreak) {
#ifdef _DEBUG
		MessageBox(NULL, L"This is a _DEBUG version", L"Break", NULL);
#else
		MessageBox(NULL, L"This is a Release version", L"Break", NULL);
#endif
	}

	if (appIsMd5CalcMode)
		return CheckMd5();
	if (!appArgInstallMode && !appArgeementArgeed && !RunArgeementDialog())
		return 0;
	if (appArgRemoveUpdater) {
		Sleep(1000);//Sleep for a while
		if (!DeleteFileW(updaterPath.c_str()))
			JTLogError(L"Remove updater file %s failed : %d", updaterPath.c_str(), GetLastError());
	}
	if (appArgInstallMode)
		return CheckInstall(AppInstallNew);
	if (!appArgForceNoInstall && CheckInstall(AppInstallCheck))
		return 0;

	if (!appIsRecover) {

		int oldStatus = RunCheckRunningApp();
		if (oldStatus == 1) {
			MessageBox(0, L"�Ѿ���һ�������������У�ͬʱֻ������һ��ʵ������ر�֮ǰ�Ǹ�", L"JiYuTrainer - ����", MB_ICONERROR);
			return 0;
		}
		if (oldStatus == -1)
			return 0;
	}

	appWorker = new TrainerWorkerInternal();

	JTLog(L"��ʼ������");

	if (appArgForceTemp || appStartType == AppStartTypeInTemp) {
		SysHlp::RunApplication(parts[0].c_str(), (L"-f " + fullPath).c_str());
	}
	else if (appStartType == AppStartTypeNormal) {
		HMODULE hMain = LoadLibrary(parts[PART_UI].c_str());
		if (!hMain) {
			LPCWSTR errStr = SysHlp::ConvertErrorCodeToString(GetLastError());
			FAST_STR_BINDER(str, L"�������������������볢�����°�װ������\n����%s (%d)", 300, errStr, GetLastError());
			LocalFree((HLOCAL)errStr);
			MessageBox(NULL, str, APP_TITLE, MB_ICONERROR);
		}
		else {
			typedef int(*fnJTUI_RunMain)();
			fnJTUI_RunMain JTUI_RunMain = (fnJTUI_RunMain)GetProcAddress(hMain, "JTUI_RunMain");
			if (JTUI_RunMain) {
				JTLog(L"����������");
				return JTUI_RunMain();
			}
			else MessageBox(NULL, L"��������������������������", APP_TITLE, MB_ICONERROR);
		}
	}
	else if (appStartType == AppStartTypeUpdater) {

	}

	return 0; 
}

void JTAppInternal::Exit(int code)
{
	ExitInternal();
	ExitClear();
	ExitProcess(code);
}
bool JTAppInternal::ExitInternal()
{

	return false;
}
void JTAppInternal::ExitClear()
{
	if (DriverLoaded()) {
		XCloseDriverHandle();
		//XUnLoadDriver();
	}
	if (appArgList) {
		LocalFree(appArgList);
		appArgList = nullptr;
	}
	if (appWorker) {
		delete appWorker;
		appWorker = nullptr;
	}
	if (appSetting) {
		delete appSetting;
		appSetting = nullptr;
	}
	if (appLogger) {
		delete appLogger;
		appLogger = nullptr;
	}
}

LPCWSTR JTAppInternal::GetPartFullPath(int partId)
{
	if (partId == PART_INI) 
		return fullIniPath.c_str();
	if(partId >=0 && partId < PART_COUNT)
		return parts[partId].c_str();
	return NULL;
}

void JTAppInternal::LoadDriver()
{
	if (!appForceNoDriver && XLoadDriver())
		if (SysHlp::GetSystemVersion() == SystemVersionWindows7OrLater && !appForceNoSelfProtect && !XinitSelfProtect())
			JTLogWarn(L"�������ұ���ʧ�ܣ�");
}

void JTAppInternal::InitPath()
{
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(hInstance, buffer, MAX_PATH);

	fullPath = buffer;

	PathRemoveFileSpec(buffer);

	fullDir = buffer;

	GetModuleFileName(hInstance, buffer, MAX_PATH);
	PathRenameExtension(buffer, L".ini");

	fullIniPath = buffer;

	appArgInstallMode = Path::GetFileName(fullPath) == L"JiYuTrainerUpdater.exe";
	appNeedInstallIniTemple = !Path::Exists(fullIniPath);
}
void JTAppInternal::InitCommandLine()
{
	appArgList = CommandLineToArgvW(GetCommandLine(), &appArgCount);
	if (appArgList == NULL)
	{
		MessageBox(NULL, L"Unable to parse command line", L"Error", MB_OK);
		ExitProcess( -1);
	}
}
void JTAppInternal::InitArgs()
{
	if (FindArgInCommandLine(appArgList, appArgCount, L"-no-install") != -1) appArgForceNoInstall = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-install-full") != -1) appArgInstallMode = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-rt") != -1) appArgForceTemp = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-wf") != -1) appArgForceCheckFileMd5 = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-b") != -1) appArgBreak = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-r1") != -1) appIsRecover = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-md5ck") != -1) appIsMd5CalcMode = true;

	int argFIndex = FindArgInCommandLine(appArgList, appArgCount, L"-f");
	if (argFIndex >= 0 && (argFIndex + 1) < appArgCount) {
		fullSourceInstallerPath = appArgList[argFIndex + 1];
		if (Path::Exists(fullSourceInstallerPath)) {
			WCHAR buffer[MAX_PATH];
			wcscpy_s(buffer, fullSourceInstallerPath.c_str());
			PathRenameExtension(buffer, L".ini");
			if (Path::Exists(fullSourceInstallerPath))
				fullIniPath = buffer;
		}
	}
	argFIndex = FindArgInCommandLine(appArgList, appArgCount, L"-rc");
	if (argFIndex >= 0 && (argFIndex + 1) < appArgCount) {
		LPCWSTR updaterFullPath = appArgList[argFIndex + 1];
		if (Path::Exists(updaterFullPath)) {
			updaterPath = updaterFullPath;
			appArgRemoveUpdater = true;
		}
	}
}
void JTAppInternal::InitLogger()
{
	currentLogger = new LoggerInternal();
	appLogger = currentLogger;
	appLogger->SetLogOutPut(LogOutPutConsolne);
}
void JTAppInternal::InitPrivileges()
{
	SysHlp::EnableDebugPriv(SE_DEBUG_NAME);
	SysHlp::EnableDebugPriv(SE_SHUTDOWN_NAME);
	SysHlp::EnableDebugPriv(SE_LOAD_DRIVER_NAME);
}
void JTAppInternal::InitSettings()
{
	appSetting = new SettingHlp(fullIniPath.c_str());

	appArgeementArgeed = appSetting->GetSettingBool(L"Argeed", false, L"JTArgeement");
	appForceNoDriver = appSetting->GetSettingBool(L"DisableDriver", false);
	appForceNoSelfProtect = !appSetting->GetSettingBool(L"SelfProtect", true);
}

HFONT JTAppInternal::hFontRed = NULL;
HINSTANCE JTAppInternal::hInstance = NULL;

INT_PTR CALLBACK JTAppInternal::ArgeementWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;

	switch (message)
	{
	case WM_INITDIALOG: {
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)));
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)));

		hFontRed = CreateFontW(20, 0, 0, 0, 0, FALSE, FALSE, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"����");//��������
		SendDlgItemMessage(hDlg, IDC_STATIC_RED, WM_SETFONT, (WPARAM)hFontRed, TRUE);//��������������Ϣ

		lResult = TRUE;
		break;
	}
	case WM_COMMAND: EndDialog(hDlg, wParam); lResult = wParam;  break;
	case WM_CTLCOLORSTATIC: {
		if ((HWND)lParam == GetDlgItem(hDlg, IDC_STATIC_RED))  SetTextColor((HDC)wParam, RGB(255, 0, 0));
		return (INT_PTR)GetStockObject(WHITE_BRUSH);
	}
	case WM_CTLCOLORDLG: {
		return (INT_PTR)(HBRUSH)GetStockObject(WHITE_BRUSH);
	}
	case WM_DESTROY: {
		DeleteObject(hFontRed);
		break;
	}
	default: return DefWindowProc(hDlg, message, wParam, lParam);
	}
	return lResult;
}


