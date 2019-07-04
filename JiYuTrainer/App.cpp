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
#include <dbghelp.h>

extern LoggerInternal * currentLogger;
extern JTApp * currentApp;

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
	return RunMd5ShowDialog();
}
int JTAppInternal::CheckInstall(APP_INSTALL_MODE mode)
{
	FILE *fp = NULL;
	bool startBatCreated = false;
	WCHAR installDir[MAX_PATH];

	if (!appForceInstallInCurrentDir && appSysHlp->CheckIsPortabilityDevice(fullPath.c_str()) || appSysHlp->CheckIsDesktop(fullDir.c_str())) {//��usb�豸����������
		WCHAR sysTempPath[MAX_PATH + 1];//��TEMPĿ¼��װ
		GetTempPath(MAX_PATH, sysTempPath);
		wcscpy_s(installDir, sysTempPath);
		wcscat_s(installDir, L"JiYuTrainer");
		if (!Path::Exists(installDir) && !CreateDirectory(installDir, NULL)) {
			wcscpy_s(installDir, fullDir.c_str());
			MessageBox(NULL, L"�޷�������ʱĿ¼���볢��ʹ�ù���Ա������б�����", L"����", MB_ICONERROR);
			return 0;
		}
		appStartType = AppStartTypeInTemp;
	}
	else {//����·��
		wcscpy_s(installDir, fullDir.c_str());//��ǰĿ¼
		appStartType = AppStartTypeNormal;
	}

	//ƴ��·���ַ���
	MergePathString(installDir);

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
				fwprintf_s(fp, L"\nTopMost=FALSE");
				fwprintf_s(fp, L"\nAutoIncludeFullWindow=FALSE");
				fwprintf_s(fp, L"\nAutoForceKill=FALSE");
				fwprintf_s(fp, L"\nAllowGbTop=FALSE");
				fwprintf_s(fp, L"\nAllowAllRunOp=FALSE");
				fwprintf_s(fp, L"\nAutoUpdate=TRUE");
				fwprintf_s(fp, L"\nCKInterval=3100");
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
				std::wstring *fileMd5 = appMD5Utils->GetFileMD5(path);
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
		std::wstring *fileMd5 = appMD5Utils->GetFileMD5(parts[PART_MAIN].c_str());
		std::wstring *thisMd5 = appMD5Utils->GetFileMD5(fullPath.c_str());
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
				currentLogger->LogError(L"����Դ��װ�� %s ʱ��������%d", fullSourceInstallerPath.c_str(), GetLastError());
		}

		//�°�װ����Ժ����������򣬲�ɾ������
		if (appStartType == AppStartTypeInTemp) 
			appSysHlp->RunApplicationPriviledge(parts[PART_RUN].c_str(), NULL);
		else if (appStartType == AppStartTypeNormal) {
			WCHAR delThisArg[MAX_PATH]; swprintf_s(delThisArg, L"-ia -rc %s", fullPath.c_str());
			appSysHlp->RunApplicationPriviledge(parts[PART_MAIN].c_str(), MakeFromSourceArg(delThisArg));
		}
	}

	return 0;
}
void JTAppInternal::UnInstall() {
	if (appWorker) {
		//ж�ز���
		appWorker->RunOperation(TrainerWorkerOpVirusBoom);
		appWorker->RunOperation(TrainerWorkerOpForceUnLoadVirus);
	}
	Sleep(1000);
	appSysHlp->RunApplicationPriviledge(parts[PART_UNINSTALL].c_str(), NULL);
	TerminateProcess(GetCurrentProcess(), 0);
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

int JTAppInternal::Run(int nCmdShow)
{
	this->appShowCmd = nCmdShow;
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
bool JTAppInternal::RunMd5ShowDialog()
{
	int rs = DialogBoxW(hInstance, MAKEINTRESOURCE(IDD_DIALOG_MD5SHOW), NULL, Md5ShowWndProc);
	return (rs == IDYES);
}
int JTAppInternal::RunInternal()
{
	setlocale(LC_ALL, "chs");
	SetUnhandledExceptionFilter(UnhandledExceptionFilter);

	MLoadNt();
	InitUtils();

	if (appSysHlp->GetSystemVersion() == SystemVersionNotSupport)
		return APP_FAIL_SYSTEM_NOT_SUPPORT;
	
	InitPrivileges();
	InitLogger();
	InitPath();
	InitCommandLine();
	InitArgs();
	InitSettings();

	if (appArgBreak) {
#ifdef _DEBUG
		if (MessageBox(NULL, L"This is a Debug version", L"JiYuTrainer - Debug Break", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
			DebugBreak();
#else
		if (MessageBox(NULL, L"This is a Release version", L"JiYuTrainer - Debug Break", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
			DebugBreak();
#endif
	}

	if (appIsMd5CalcMode)
		return CheckMd5();
	if (!appArgInstallMode && !appArgeementArgeed && !RunArgeementDialog())
		return 0;
	if (appIsConfigMode) {
		appStartType = AppStartTypeConfig;
		goto CONFIG;
	}
	if (appArgRemoveUpdater) {
		Sleep(1000);//Sleep for a while
		if (!DeleteFileW(updaterPath.c_str()))
			currentLogger->LogError(L"Remove updater file %s failed : %d", updaterPath.c_str(), GetLastError());
	}
	if (appArgInstallMode)
		return CheckInstall(AppInstallNew);
	if (!appArgForceNoInstall && CheckInstall(AppInstallCheck))
		return 0;

	if (!appIsRecover) {

		int oldStatus = RunCheckRunningApp();
		if (oldStatus == 1)
			return APP_FAIL_ALEDAY_RUN;
		if (oldStatus == -1)
			return 0;
	}

	appWorker = new TrainerWorkerInternal();
	appLogger->Log(L"��ʼ������");

	CONFIG:
	if (appArgForceTemp || appStartType == AppStartTypeInTemp) appSysHlp->RunApplication(parts[PART_RUN].c_str(), NULL);
	else {
		typedef int(*fnJTUI_RunMain)();

		HMODULE hMain = LoadLibrary(parts[PART_UI].c_str());
		if (!hMain) {
			LPCWSTR errStr = appSysHlp->ConvertErrorCodeToString(GetLastError());
			FAST_STR_BINDER(str, L"�������������������볢�����°�װ������\n����%s (%d)", 300, errStr, GetLastError());
			appStartErr = str;
			LocalFree((HLOCAL)errStr);
			return APP_FAIL_MAIN_PART_LOADERR;
		}

		if (appStartType == AppStartTypeNormal) {

			fnJTUI_RunMain JTUI_RunMain = (fnJTUI_RunMain)GetProcAddress(hMain, (LPCSTR)2);
			if (JTUI_RunMain) {
				appLogger->Log(L"����������");
				return JTUI_RunMain();
			}
			else return APP_FAIL_MAIN_PART_BROKED;
		}
		else if (appStartType == AppStartTypeUpdater) {

			fnJTUI_RunMain JTUI_RunUpdate = (fnJTUI_RunMain)GetProcAddress(hMain, (LPCSTR)3);
			if (JTUI_RunUpdate) {
				appLogger->Log(L"����������²���ģʽ");
				return JTUI_RunUpdate();
			}
			else return APP_FAIL_MAIN_PART_BROKED;
		}
		else if (appStartType == AppStartTypeConfig) {
		
			fnJTUI_RunMain JTUI_RunConfig = (fnJTUI_RunMain)GetProcAddress(hMain, (LPCSTR)4);
			if (JTUI_RunConfig) {
				appLogger->Log(L"������������ģʽ");
				return JTUI_RunConfig();
			}
			else return APP_FAIL_MAIN_PART_BROKED;
		}
	}
	return 0; 
}

void JTAppInternal::Exit(int code)
{
	ExitInternal();
	ExitClear();
	//ExitProcess(code);
}
bool JTAppInternal::ExitInternal()
{

	return false;
}
void JTAppInternal::ExitClear()
{
	if (XDriverLoaded()) {
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
}

LPCWSTR JTAppInternal::GetPartFullPath(int partId)
{
	if (partId == PART_INI) 
		return fullIniPath.c_str();
	if(partId >=0 && partId < PART_COUNT)
		return parts[partId].c_str();
	return NULL;
}
void *JTAppInternal::GetUtils(int utilsId)
{
	return utilsPointer[utilsId];
}

LPVOID JTAppInternal::RunOperation(AppOperation op)
{
	switch (op)
	{
	case AppOperation1: LoadDriver(); break;
	case AppOperation2: MUnLoadKernelDriver(L"TDProcHook"); break;
	case AppOperationUnLoadDriver: {
		if (XUnLoadDriver())
			currentLogger->Log(L"����ж�سɹ�");
		break;
	}
	case AppOperationKReboot:  KFReboot(); break;
	case AppOperationKShutdown: KFShutdown();  break;
	default:
		break;
	}
	return nullptr;
}

void JTAppInternal::LoadDriver()
{
	if (!appForceNoDriver && XLoadDriver())
		if (appSysHlp->GetSystemVersion() == SystemVersionWindows7OrLater && !appForceNoSelfProtect && !XInitSelfProtect())
			currentLogger->LogWarn(L"�������ұ���ʧ�ܣ�");
}

void JTAppInternal::MergePathString(LPCWSTR path)
{		
	//ƴ��·���ַ���
	for (int i = 0; i < PART_COUNT; i++)
		parts[i] = std::wstring(path) + L"\\" + parts[i];
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
	if (FindArgInCommandLine(appArgList, appArgCount, L"-h") != -1) appIsHiddenMode = true;
	if (FindArgInCommandLine(appArgList, appArgCount, L"-c") != -1) appIsConfigMode = true;

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
	appLogger = currentLogger;
	appLogger->SetLogOutPut(LogOutPutConsolne);
}
void JTAppInternal::InitPrivileges()
{
	appSysHlp->EnableDebugPriv(SE_DEBUG_NAME);
	appSysHlp->EnableDebugPriv(SE_SHUTDOWN_NAME);
	appSysHlp->EnableDebugPriv(SE_LOAD_DRIVER_NAME);
}
void JTAppInternal::InitSettings()
{
	appSetting = new SettingHlpInternal(fullIniPath.c_str());

	appArgeementArgeed = appSetting->GetSettingBool(L"Argeed", false, L"JTArgeement");
	appForceNoDriver = appSetting->GetSettingBool(L"DisableDriver", false);
	appForceNoSelfProtect = !appSetting->GetSettingBool(L"SelfProtect", true);
	appForceInstallInCurrentDir = !appSetting->GetSettingBool(L"ForceInstallInCurrentDir", false);
}
void JTAppInternal::InitUtils()
{
	utilsPointer[UTILS_SYSHLP] = new SysHlpInternal();
	utilsPointer[UTILS_MD5UTILS] = new MD5UtilsInternal();

	appMD5Utils = (MD5UtilsInternal*)utilsPointer[UTILS_MD5UTILS];
	appSysHlp = (SysHlpInternal*)utilsPointer[UTILS_SYSHLP];
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
INT_PTR CALLBACK JTAppInternal::Md5ShowWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;

	switch (message)
	{
	case WM_INITDIALOG: {
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)));
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP)));
		
		JTAppInternal * app = dynamic_cast<JTAppInternal *>(currentApp);
		app->MergePathString(app->GetCurrentDir());
		WCHAR buffer[1024] = { 0 };
		WCHAR buffer2[128] = { 0 };
		for (int i = 1; i < PART_COUNT; i++) {
			LPCWSTR path = app->parts[i].c_str();
			if (app->partsResId[i] != 0 && Path::Exists(path)) {
				std::wstring*md5 = app->appMD5Utils->GetFileMD5(path);
				swprintf_s(buffer2, L"#define %s L\"%s\"\r\n", app->partsMd5CheckNames[i], md5->c_str());
				wcscat_s(buffer, buffer2);
				FreeStringPtr(md5);
			}
		}
		lResult = TRUE;

		SetDlgItemText(hDlg, IDC_EDIT_MD5, buffer);

		break;
	}
	case WM_COMMAND: {
		if (wParam == IDOK) {
			EndDialog(hDlg, wParam);
			lResult = wParam;
		}
		break;
	}
	default: return DefWindowProc(hDlg, message, wParam, lParam);
	}
	return lResult;
}

LONG WINAPI JTAppInternal::UnhandledExceptionFilter(PEXCEPTION_POINTERS pExInfo)
{
	// ������һЩ�쳣�Ĺ��˻���ʾ
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;
	return GenerateMiniDump(pExInfo);
}
LONG JTAppInternal::GenerateMiniDump(PEXCEPTION_POINTERS pExInfo)
{
	TCHAR dmp_path[MAX_PATH];
	wcscpy_s(dmp_path, currentApp->GetCurrentDir());

	SYSTEMTIME tm;
	GetLocalTime(&tm);//��ȡʱ��
	TCHAR file_name[128];
	swprintf_s(file_name, L"%s\\JiYuTrainerCrashDump%d%02d%02d-%02d%02d%02d.dmp", dmp_path,
		tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond);//����dmp�ļ�����
	HANDLE hFile = CreateFile(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION expParam;
		expParam.ThreadId = GetCurrentThreadId();
		expParam.ExceptionPointers = pExInfo;
		expParam.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, (pExInfo ? &expParam : NULL), NULL, NULL);

		CloseHandle(hFile);

		const TCHAR *fmt =
		{
			L"�������������Ҫ�ر�,���������п��ܶ�ʧ,\n\n"
			L"���ǶԴ�����Ĳ����ʾ��Ǹ;�뽫\n\n"
			L"\"%s\"\n\n"
			L"���͸������Ա���ٲ�������֮����,лл��\n\n"
		};
		TCHAR msg[400];
		swprintf_s(msg, fmt, file_name);
		MessageBox(NULL, msg, L"�����쳣����", MB_ICONERROR | MB_SYSTEMMODAL);
	}
	else
	{
		TCHAR info[300] = { L"fail to create dump file:" };
		wcscat_s(info, file_name);
		MessageBox(NULL, info, L"�����쳣����", MB_ICONERROR | MB_SYSTEMMODAL);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}


