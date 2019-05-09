#pragma once
#include "stdafx.h"
#include <string>
#include "Logger.h"
#include "SettingHlp.h"
#include "TrainerWorker.h"

#define PART_INI -1
#define PART_HOOKER 2
#define PART_DRIVER 3
#define PART_COUNT 6

#define FAST_STR_BINDER(str, fstr, size, ...) WCHAR str[size]; swprintf_s(str, fstr, __VA_ARGS__)

#define APP_TITLE L"JiYuTrainer"

enum EXTRACT_RES {
	ExtractUnknow,
	ExtractCreateFileError,
	ExtractWriteFileError,
	ExtractReadResError,
	ExtractSuccess
};

class JTApp
{
public:

	/*
		�����������Բ���ʼ��װ
	*/
	virtual int CheckInstall() { return 0; }
	/*
		���ָ��·���Ƿ���USB�豸·��
	*/
	virtual bool CheckIsPortabilityDevice(LPCWSTR path) { return false; }

	/*
		�ͷ�ģ����Դ���ļ�
		[resModule] ��Դ����ģ��
		[resId] ��Դid
		[resType] ��Դ����
		[extractTo] �ļ�·��
	*/
	virtual EXTRACT_RES InstallResFile(HINSTANCE resModule, LPWSTR resId, LPCWSTR resType, LPCWSTR extractTo) { return EXTRACT_RES::ExtractUnknow; }

	//��������в����Ƿ����ĳ������
	virtual bool IsCommandExists(LPCWSTR cmd) { return false; }

	//��ȡ�����в�������
	virtual LPWSTR *GetCommandLineArray() { return nullptr; }
	//��ȡ�����в��������С
	virtual int GetCommandLineArraySize() { return 0; }
	/*
		���������в����������е�λ��
		[szArgList] �����в�������
		[argCount] �����в��������С
		[arg] Ҫ���������в���
		[����] ����ҵ����������������򷵻�-1
	*/
	virtual int FindArgInCommandLine(LPWSTR *szArgList, int argCount, const wchar_t * arg) { return 0; }

	/*
		���г���
	*/
	virtual int Run() { return 0; }
	virtual int GetResult() { return 0; }
	virtual void Exit(int code) {  }

	/*
		��ȡ��ǰ���� HINSTANCE
	*/
	virtual HINSTANCE GetInstance() { return nullptr; }

	/*
		��ȡ��������λ��
		[partId] ��������
	*/
	virtual LPCWSTR GetPartFullPath(int partId) { return nullptr; }

	//��ȡ��ǰ��������·��
	virtual LPCWSTR GetFullPath() { return nullptr; }
	//��ȡ��ǰ����Ŀ¼
	virtual LPCWSTR GetCurrentDir() { return nullptr; }
	virtual LPCWSTR GetSourceInstallerPath() { return nullptr; }

	virtual Logger* GetLogger() { return nullptr; };
	virtual SettingHlp* GetSettings() { return nullptr; };
	virtual bool GetSelfProtect() { return false; }
	virtual TrainerWorker* GetTrainerWorker() { return nullptr; };
};
class JTAppInternal : public JTApp
{
public:


	JTAppInternal(HINSTANCE hInstance);
	~JTAppInternal();

	/*
		�����������Բ���ʼ��װ
	*/
	int CheckInstall();
	/*
		���ָ��·���Ƿ���USB�豸·��
	*/
	bool CheckIsPortabilityDevice(LPCWSTR path);

	/*
		�ͷ�ģ����Դ���ļ�
		[resModule] ��Դ����ģ��
		[resId] ��Դid
		[resType] ��Դ����
		[extractTo] �ļ�·��
	*/
	EXTRACT_RES InstallResFile(HINSTANCE resModule, LPWSTR resId, LPCWSTR resType, LPCWSTR extractTo);

	//��ȡ�����в�������
	LPWSTR *GetCommandLineArray() { return appArgList; };
	//��ȡ�����в��������С
	int GetCommandLineArraySize() { return appArgCount; };

	bool IsCommandExists(LPCWSTR cmd);

	int FindArgInCommandLine(LPWSTR *szArgList, int argCount, const wchar_t * arg);

	/*
		���г���
	*/
	int Run();
	int GetResult() { return appResult; }
	void Exit(int code);

	/*
		��ȡ��ǰ���� HINSTANCE
	*/
	HINSTANCE GetInstance() {
		return hInstance;
	}

	/*
		��ȡ��������λ��
		[partId] ��������
	*/
	LPCWSTR GetPartFullPath(int partId);

	//��ȡ��ǰ��������·��
	LPCWSTR GetFullPath() { return fullPath.c_str(); }
	//��ȡ��ǰ����Ŀ¼
	LPCWSTR GetCurrentDir() { return fullDir.c_str(); }
	LPCWSTR GetSourceInstallerPath() { return fullSourceInstallerPath.c_str(); }

	Logger* GetLogger() { return appLogger; };
	SettingHlp* GetSettings() { return appSetting; };
	bool GetSelfProtect() { return appForceNoSelfProtect; }
	TrainerWorker* GetTrainerWorker() { return appWorker; };

private:

	std::wstring fullPath;
	std::wstring fullDir;
	std::wstring fullSourceInstallerPath;
	std::wstring fullIniPath;

	std::wstring parts[PART_COUNT] = {
		std::wstring(L"JiYuTrainer.exe"),
		std::wstring(L"JiYuTrainerUI.dll"),
		std::wstring(L"JiYuTrainerHooks.dll"), 
		std::wstring(L"JiYuTrainerDriver.sys"),
		std::wstring(L"JiYuTrainerUpdater.dll"),
		std::wstring(L"sciter.dll"),
	};
	int partsResId[PART_COUNT] = {
		0,
		0,
		0,
		0,
		0,
		0,
	};

	enum AppStartType {
		AppStartTypeNormal,
		AppStartTypeInTemp,
		AppStartTypeUpdater,
	};

	int appResult = 0;
	static HINSTANCE hInstance;
	int appStartType = AppStartTypeNormal;

	bool appArgeementArgeed = false;
	bool appForceNoSelfProtect = false;
	bool appForceNoDriver = false;
	bool appArgForceNoInstall = false;
	bool appArgForceTemp = false;

	LPWSTR *appArgList = nullptr;
	int appArgCount = 0;

	Logger *appLogger = nullptr;
	SettingHlp *appSetting = nullptr;
	TrainerWorker *appWorker = nullptr;

	void InitPath();
	void InitCommandLine();
	void InitArgs();
	void InitLogger();
	void InitPrivileges();
	void InitSettings();
	void InitPP();

	int RunCheckRunningApp();
	bool RunArgeeMentDialog();

	int RunInternal();
	bool ExitInternal();
	void ExitClear();

	static HFONT hFontRed;

	static INT_PTR CALLBACK ArgeementWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};


