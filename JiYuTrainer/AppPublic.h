#pragma once

#include "Logger.h"
#include "SettingHlp.h"
#include "TrainerWorker.h" 

#define PART_INI -1
#define PART_MAIN 0
#define PART_RUN 1
#define PART_UNINSTALL 2
#define PART_UI 3
#define PART_HOOKER 4
#define PART_DRIVER 5
#define PART_COUNT 8

enum EXTRACT_RES {
	ExtractUnknow,
	ExtractCreateFileError,
	ExtractWriteFileError,
	ExtractReadResError,
	ExtractSuccess
};
enum APP_INSTALL_MODE {
	AppInstallCheck,
	AppInstallNew,
};

class JTApp
{
public:

	/*
		�����������Բ���ʼ��װ
	*/
	virtual int CheckInstall(APP_INSTALL_MODE mode) { return 0; }

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

	virtual LPCWSTR MakeFromSourceArg(LPCWSTR arg) { return nullptr; }

	virtual void LoadDriver() {}

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