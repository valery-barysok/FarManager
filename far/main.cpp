﻿/*
main.cpp

Функция main.
*/
/*
Copyright © 1996 Eugene Roshal
Copyright © 2000 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "headers.hpp"
#pragma hdrstop

#include "platform.security.hpp"
#include "keys.hpp"
#include "chgprior.hpp"
#include "farcolor.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "fileedit.hpp"
#include "fileview.hpp"
#include "manager.hpp"
#include "ctrlobj.hpp"
#include "scrbuf.hpp"
#include "lang.hpp"
#include "language.hpp"
#include "farexcpt.hpp"
#include "imports.hpp"
#include "syslog.hpp"
#include "interf.hpp"
#include "keyboard.hpp"
#include "clipboard.hpp"
#include "pathmix.hpp"
#include "dirmix.hpp"
#include "elevation.hpp"
#include "cmdline.hpp"
#include "console.hpp"
#include "configdb.hpp"
#include "colormix.hpp"
#include "treelist.hpp"
#include "plugins.hpp"
#include "notification.hpp"
#include "datetime.hpp"
#include "tracer.hpp"
#include "constitle.hpp"
#include "string_utils.hpp"
#include "cvtname.hpp"
#include "drivemix.hpp"
#include "new_handler.hpp"

global *Global = nullptr;

static void show_help()
{
	static const auto HelpMsg =
		L"Usage: far [switches] [apath [ppath]]\n\n"
		L"where\n"
		L"  apath - path to a folder (or a file or an archive or command with prefix)\n"
		L"          for the active panel\n"
		L"  ppath - path to a folder (or a file or an archive or command with prefix)\n"
		L"          for the passive panel\n"
		L"The following switches may be used in the command line:\n"
		L" -?   This help.\n"
		L" -a   Disable display of characters with codes 0 - 31 and 255.\n"
		L" -ag  Disable display of pseudographics characters.\n"
		L" -clearcache [profilepath [localprofilepath]]\n"
		L"      Clear plugins cache.\n"
		L" -co  Forces FAR to load plugins from the cache only.\n"
#ifdef DIRECT_RT
		L" -do  Direct output.\n"
#endif
		L" -e[<line>[:<pos>]] <filename>\n"
		L"      Edit the specified file.\n"
		L" -export <out.farconfig> [profilepath [localprofilepath]]\n"
		L"      Export settings.\n"
		L" -import <in.farconfig> [profilepath [localprofilepath]]\n"
		L"      Import settings.\n"
		L" -m   Do not load macros.\n"
		L" -ma  Do not execute auto run macros.\n"
		L" -p[<path>]\n"
		L"      Search for \"common\" plugins in the directory, specified by <path>.\n"
		L" -ro[-] Read-Only or Normal config mode.\n"
		L" -s <profilepath> [<localprofilepath>]\n"
		L"      Custom location for Far configuration files - overrides Far.exe.ini.\n"
		L" -set:<parameter>=<value>\n"
		L"      Override the configuration parameter, see far:config for details.\n"
		L" -t <path>\n"
		L"      Location of Far template configuration file - overrides Far.exe.ini.\n"
		L" -title[:<valuestring>]\n"
		L"      If <valuestring> is given, use it as the window title;\n"
		L"      otherwise, inherit the console window's title.\n"
#ifndef NO_WRAPPER
		L" -u <username>\n"
		L"      Allows to have separate registry settings for different users.\n"
		L"      Affects only 1.x Far Manager plugins.\n"
#endif // NO_WRAPPER
		L" -v <filename>\n"
		L"      View the specified file. If <filename> is -, data is read from the stdin.\n"
		L" -w[-] Stretch to console window instead of console buffer or vise versa.\n"
		""_sv;

	std::wcout << HelpMsg << std::flush;
}

static int MainProcess(
    const string& EditName,
    const string& ViewName,
    const string& DestName1,
    const string& DestName2,
    int StartLine,
    int StartChar
)
{
		SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
		FarColor InitAttributes={};
		Console().GetTextAttributes(InitAttributes);
		SetRealColor(colors::PaletteColorToFarColor(COL_COMMANDLINEUSERSCREEN));

		string ename(EditName),vname(ViewName), apanel(DestName1),ppanel(DestName2);
		if (ConfigProvider().ShowProblems())
		{
			ename.clear();
			vname.clear();
			StartLine = StartChar = -1;
			apanel = Global->Opt->ProfilePath;
			ppanel = Global->Opt->LocalProfilePath;
		}

		if (!ename.empty() || !vname.empty())
		{
			Global->OnlyEditorViewerUsed = true;

			_tran(SysLog(L"create dummy panels"));
			Global->CtrlObject->CreateDummyFilePanels();
			Global->WindowManager->PluginCommit();

			Global->CtrlObject->Plugins->LoadPlugins();
			Global->CtrlObject->Macro.LoadMacros(true, true);

			if (!ename.empty())
			{
				const auto ShellEditor = FileEditor::create(ename, CP_DEFAULT, FFILEEDIT_CANNEWFILE | FFILEEDIT_ENABLEF6, StartLine, StartChar);
				_tran(SysLog(L"make shelleditor %p",ShellEditor));

				if (!ShellEditor->GetExitCode())  // ????????????
				{
					Global->WindowManager->ExitMainLoop(0);
				}
			}
			// TODO: Этот else убрать только после разборок с возможностью задавать несколько /e и /v в ком.строке
			else if (!vname.empty())
			{
				const auto ShellViewer = FileViewer::create(vname, true);

				if (!ShellViewer->GetExitCode())
				{
					Global->WindowManager->ExitMainLoop(0);
				}

				_tran(SysLog(L"make shellviewer, %p",ShellViewer));
			}

			Global->WindowManager->EnterMainLoop();
		}
		else
		{
			int DirCount=0;

			// воспользуемся тем, что ControlObject::Init() создает панели
			// юзая Global->Opt->*

			const auto& SetupPanel = [&](bool active)
			{
				++DirCount;
				string strPath = active? apanel : ppanel;
				if (os::fs::is_file(strPath))
				{
					CutToParent(strPath);
				}

				bool Root = false;
				const auto Type = ParsePath(strPath, nullptr, &Root);
				if(Root && (Type == root_type::drive_letter || Type == root_type::unc_drive_letter || Type == root_type::volume))
				{
					AddEndSlash(strPath);
				}

				auto& CurrentPanelOptions = (Global->Opt->LeftFocus == active)? Global->Opt->LeftPanel : Global->Opt->RightPanel;
				CurrentPanelOptions.m_Type = static_cast<int>(panel_type::FILE_PANEL);  // сменим моду панели
				CurrentPanelOptions.Visible = true;     // и включим ее
				CurrentPanelOptions.Folder = strPath;
			};

			if (!apanel.empty())
			{
				SetupPanel(true);

				if (!ppanel.empty())
				{
					SetupPanel(false);
				}
			}

			// теперь все готово - создаем панели!
			Global->CtrlObject->Init(DirCount);

			// а теперь "провалимся" в каталог или хост-файл (если получится ;-)
			if (!apanel.empty())  // активная панель
			{
				const auto ActivePanel = Global->CtrlObject->Cp()->ActivePanel();
				const auto AnotherPanel = Global->CtrlObject->Cp()->PassivePanel();

				if (!ppanel.empty())  // пассивная панель
				{
					FarChDir(AnotherPanel->GetCurDir());

					if (IsPluginPrefixPath(ppanel))
					{
						AnotherPanel->Parent()->SetActivePanel(AnotherPanel);

						execute_info Info;
						Info.Command = ppanel;

						Global->CtrlObject->CmdLine()->ExecString(Info);
						ActivePanel->Parent()->SetActivePanel(ActivePanel);
					}
					else
					{
						const auto strPath = PointToName(ppanel);

						if (!strPath.empty())
						{
							if (AnotherPanel->GoToFile(strPath))
								AnotherPanel->ProcessKey(Manager::Key(KEY_CTRLPGDN));
						}
					}
				}

				FarChDir(ActivePanel->GetCurDir());

				if (IsPluginPrefixPath(apanel))
				{
					execute_info Info;
					Info.Command = apanel;

					Global->CtrlObject->CmdLine()->ExecString(Info);
				}
				else
				{
					const auto strPath = PointToName(apanel);

					if (!strPath.empty())
					{
						if (ActivePanel->GoToFile(strPath))
							ActivePanel->ProcessKey(Manager::Key(KEY_CTRLPGDN));
					}
				}

				// !!! ВНИМАНИЕ !!!
				// Сначала редравим пассивную панель, а потом активную!
				AnotherPanel->Redraw();
				ActivePanel->Redraw();
			}

			Global->WindowManager->EnterMainLoop();
		}

		TreeList::FlushCache();

		// очистим за собой!
		SetScreen(0,0,ScrX,ScrY,L' ',colors::PaletteColorToFarColor(COL_COMMANDLINEUSERSCREEN));
		Console().SetTextAttributes(InitAttributes);
		Global->ScrBuf->ResetLockCount();
		Global->ScrBuf->Flush();

		return 0;
}

static void InitTemplateProfile(string &strTemplatePath)
{
	if (strTemplatePath.empty())
	{
		strTemplatePath = GetFarIniString(L"General", L"TemplateProfile", L"%FARHOME%\\Default.farconfig");
	}

	if (!strTemplatePath.empty())
	{
		strTemplatePath = ConvertNameToFull(unquote(os::env::expand(strTemplatePath)));
		DeleteEndSlash(strTemplatePath);

		if (os::fs::is_directory(strTemplatePath))
			strTemplatePath += L"\\Default.farconfig";

		Global->Opt->TemplateProfilePath = strTemplatePath;
	}
}

static void InitProfile(string &strProfilePath, string &strLocalProfilePath)
{
	if (!strProfilePath.empty())
	{
		strProfilePath = ConvertNameToFull(unquote(os::env::expand(strProfilePath)));
	}
	if (!strLocalProfilePath.empty())
	{
		strLocalProfilePath = ConvertNameToFull(unquote(os::env::expand(strLocalProfilePath)));
	}

	if (strProfilePath.empty())
	{
		int UseSystemProfiles = GetFarIniInt(L"General", L"UseSystemProfiles", 1);
		if (UseSystemProfiles)
		{
			// roaming data default path: %APPDATA%\Far Manager\Profile
			wchar_t Buffer[MAX_PATH];
			SHGetFolderPath(nullptr, CSIDL_APPDATA|CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, Buffer);
			Global->Opt->ProfilePath = Buffer;
			AddEndSlash(Global->Opt->ProfilePath);
			Global->Opt->ProfilePath += L"Far Manager";

			if (UseSystemProfiles == 2)
			{
				Global->Opt->LocalProfilePath = Global->Opt->ProfilePath;
			}
			else
			{
				// local data default path: %LOCALAPPDATA%\Far Manager\Profile
				SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, Buffer);
				Global->Opt->LocalProfilePath = Buffer;
				AddEndSlash(Global->Opt->LocalProfilePath);
				Global->Opt->LocalProfilePath += L"Far Manager";
			}

			string* Paths[]={&Global->Opt->ProfilePath, &Global->Opt->LocalProfilePath};
			std::for_each(RANGE(Paths, i)
			{
				AddEndSlash(*i);
				*i += L"Profile";
				CreatePath(*i, true);
			});
		}
		else
		{
			const auto strUserProfileDir = GetFarIniString(L"General", L"UserProfileDir", L"%FARHOME%\\Profile");
			const auto strUserLocalProfileDir = GetFarIniString(L"General", L"UserLocalProfileDir", strUserProfileDir);
			Global->Opt->ProfilePath = ConvertNameToFull(unquote(os::env::expand(strUserProfileDir)));
			Global->Opt->LocalProfilePath = ConvertNameToFull(unquote(os::env::expand(strUserLocalProfileDir)));
		}
	}
	else
	{
		Global->Opt->ProfilePath = strProfilePath;
		Global->Opt->LocalProfilePath = strLocalProfilePath.empty() ? strProfilePath : strLocalProfilePath;
	}

	Global->Opt->LoadPlug.strPersonalPluginsPath = Global->Opt->ProfilePath + L"\\Plugins";

	os::env::set(L"FARPROFILE", Global->Opt->ProfilePath);
	os::env::set(L"FARLOCALPROFILE", Global->Opt->LocalProfilePath);

	if (Global->Opt->ReadOnlyConfig < 0) // do not override 'far /ro', 'far /ro-'
		Global->Opt->ReadOnlyConfig = GetFarIniInt(L"General", L"ReadOnlyConfig", 0);

	if (!Global->Opt->ReadOnlyConfig)
	{
		CreatePath(Global->Opt->ProfilePath + L"\\PluginsData", true);
		if (Global->Opt->ProfilePath != Global->Opt->LocalProfilePath)
		{
			CreatePath(Global->Opt->LocalProfilePath + L"\\PluginsData", true);
		}
	}
}

static bool ProcessServiceModes(const range<wchar_t**>& Args, int& ServiceResult)
{
	const auto& isArg = [](const wchar_t* Arg, const wchar_t* Name)
	{
		return (*Arg == L'/' || *Arg == L'-') && equal_icase(Arg + 1, Name);
	};

	if (Args.size() == 4 && IsElevationArgument(Args[0])) // /service:elevation {GUID} PID UsePrivileges
	{
		ServiceResult = ElevationMain(Args[1], std::wcstoul(Args[2], nullptr, 10), *Args[3] == L'1');
		return true;
	}

	if (InRange(2u, Args.size(), 5u) && (isArg(Args[0], L"export") || isArg(Args[0], L"import")))
	{
		bool Export = isArg(Args[0], L"export");
		string strProfilePath(Args.size() > 2 ? Args[2] : L""), strLocalProfilePath(Args.size() > 3 ? Args[3] : L""), strTemplatePath(Args.size() > 4 ? Args[4] : L"");
		InitTemplateProfile(strTemplatePath);
		InitProfile(strProfilePath, strLocalProfilePath);
		Global->m_ConfigProvider = new config_provider(Export? config_provider::mode::m_export : config_provider::mode::m_import);
		ServiceResult = !ConfigProvider().ServiceMode(Args[1]);
		return true;
	}

	if (InRange(1u, Args.size(), 3u) && isArg(Args[0], L"clearcache"))
	{
		string strProfilePath(Args.size() > 1 ? Args[1] : L"");
		string strLocalProfilePath(Args.size() > 2 ? Args[2] : L"");
		InitProfile(strProfilePath, strLocalProfilePath);
		config_provider::ClearPluginsCache();
		ServiceResult = 0;
		return true;
	}

	return false;
}

static void UpdateErrorMode()
{
	Global->ErrorMode |= SEM_NOGPFAULTERRORBOX;
	long long IgnoreDataAlignmentFaults = 0;
	ConfigProvider().GeneralCfg()->GetValue(L"System.Exception", L"IgnoreDataAlignmentFaults", IgnoreDataAlignmentFaults, IgnoreDataAlignmentFaults);
	if (IgnoreDataAlignmentFaults)
	{
		Global->ErrorMode |= SEM_NOALIGNMENTFAULTEXCEPT;
	}
	SetErrorMode(Global->ErrorMode);
}

static void SetDriveMenuHotkeys()
{
	long long InitDriveMenuHotkeys = 1;
	ConfigProvider().GeneralCfg()->GetValue(L"Interface", L"InitDriveMenuHotkeys", InitDriveMenuHotkeys, InitDriveMenuHotkeys);

	if (InitDriveMenuHotkeys)
	{
		static const struct
		{
			const wchar_t* PluginId;
			GUID MenuId;
			const wchar_t* Hotkey;
		}
		DriveMenuHotkeys[] =
		{
			{ L"1E26A927-5135-48C6-88B2-845FB8945484", { 0x61026851, 0x2643, 0x4C67, { 0xBF, 0x80, 0xD3, 0xC7, 0x7A, 0x3A, 0xE8, 0x30 } }, L"0" }, // ProcList
			{ L"B77C964B-E31E-4D4C-8FE5-D6B0C6853E7C", { 0xF98C70B3, 0xA1AE, 0x4896, { 0x93, 0x88, 0xC5, 0xC8, 0xE0, 0x50, 0x13, 0xB7 } }, L"1" }, // TmpPanel
			{ L"42E4AEB1-A230-44F4-B33C-F195BB654931", { 0xC9FB4F53, 0x54B5, 0x48FF, { 0x9B, 0xA2, 0xE8, 0xEB, 0x27, 0xF0, 0x12, 0xA2 } }, L"2" }, // NetBox
			{ L"773B5051-7C5F-4920-A201-68051C4176A4", { 0x24B6DD41, 0xDF12, 0x470A, { 0xA4, 0x7C, 0x86, 0x75, 0xED, 0x8D, 0x2E, 0xD4 } }, L"3" }, // Network
		};

		std::for_each(CONST_RANGE(DriveMenuHotkeys, i)
		{
			ConfigProvider().PlHotkeyCfg()->SetHotkey(i.PluginId, i.MenuId, hotkey_type::drive_menu, i.Hotkey);
		});

		ConfigProvider().GeneralCfg()->SetValue(L"Interface", L"InitDriveMenuHotkeys", 0ull);
	}
}

static int mainImpl(const range<wchar_t**>& Args)
{
	setlocale(LC_ALL, "");

	// Must be static - dependent static objects exist
	static SCOPED_ACTION(os::com::initialize);

	SCOPED_ACTION(global);

	auto NoElevationDuringBoot = std::make_unique<elevation::suppress>();

	SetErrorMode(Global->ErrorMode);

	TestPathParser();

	RegisterTestExceptionsHook();

	os::EnableLowFragmentationHeap();

	if(!Console().IsFullscreenSupported())
	{
		const BYTE ReserveAltEnter = 0x8;
		Imports().SetConsoleKeyShortcuts(TRUE, ReserveAltEnter, nullptr, 0);
	}

	os::fs::InitCurrentDirectory();

	if (os::fs::GetModuleFileName(nullptr, nullptr, Global->g_strFarModuleName))
	{
		Global->g_strFarModuleName = ConvertNameToLong(Global->g_strFarModuleName);
		PrepareDiskPath(Global->g_strFarModuleName);
	}

	Global->g_strFarINI = Global->g_strFarModuleName+L".ini";
	Global->g_strFarPath = Global->g_strFarModuleName;
	CutToSlash(Global->g_strFarPath,true);
	os::env::set(L"FARHOME", Global->g_strFarPath);
	AddEndSlash(Global->g_strFarPath);

	if (os::security::is_admin())
		os::env::set(L"FARADMINMODE", L"1");
	else
		os::env::del(L"FARADMINMODE");

	{
		int ServiceResult;
		if (ProcessServiceModes(Args, ServiceResult))
			return ServiceResult;
	}

	SCOPED_ACTION(listener)(update_environment, &ReloadEnvironment);
	SCOPED_ACTION(listener)(update_intl, &OnIntlSettingsChange);
	SCOPED_ACTION(listener_ex)(update_devices, &UpdateSavedDrives);

	_OT(SysLog(L"[[[[[[[[New Session of FAR]]]]]]]]]"));

	string strEditName;
	string strViewName;
	string DestNames[2];
	int StartLine=-1,StartChar=-1;
	int CntDestName=0; // количество параметров-имен каталогов

	string strProfilePath, strLocalProfilePath, strTemplatePath;

	// TODO: std::optional
	std::pair<string, bool> CustomTitle;

	std::unordered_map<string, string, hash_icase, equal_to_icase> Overrides;
	FOR_RANGE(Args, Iter)
	{
		const auto& Arg = *Iter;
		if ((Arg[0]==L'/' || Arg[0]==L'-') && Arg[1])
		{
			switch (upper(Arg[1]))
			{
				case L'A':
					switch (upper(Arg[2]))
					{
					case 0:
						Global->Opt->CleanAscii = true;
						break;

					case L'G':
						if (!Arg[3])
							Global->Opt->NoGraphics = true;
						break;
					}
					break;

				case L'E':
					if (std::iswdigit(Arg[2]))
					{
						StartLine = static_cast<int>(std::wcstol(Arg + 2, nullptr, 10));
						const wchar_t *ChPtr = wcschr(Arg + 2, L':');

						if (ChPtr)
							StartChar = static_cast<int>(std::wcstol(ChPtr + 1, nullptr, 10));;
					}

					if (Iter + 1 != Args.end())
					{
						strEditName = *++Iter;
					}
					break;

				case L'V':
					if (Iter + 1 != Args.end())
					{
						strViewName = *++Iter;
					}
					break;

				case L'M':
					switch (upper(Arg[2]))
					{
					case L'\0':
						Global->Opt->Macro.DisableMacro|=MDOL_ALL;
						break;

					case L'A':
						if (!Arg[3])
							Global->Opt->Macro.DisableMacro|=MDOL_AUTOSTART;
						break;
					}
					break;

#ifndef NO_WRAPPER
				case L'U':
					if (Iter + 1 != Args.end())
					{
						//Affects OEM plugins only!
						Global->strRegUser = *++Iter;
					}
					break;
#endif // NO_WRAPPER

				case L'S':
					{
						constexpr auto SetParam = L"set:"_sv;
						if (starts_with_icase(Arg + 1, SetParam))
						{
							if (const auto EqualPtr = wcschr(Arg + 1, L'='))
							{
								Overrides.emplace(string(Arg + 1 + SetParam.size(), EqualPtr), EqualPtr + 1);
							}
						}
						else if (Iter + 1 != Args.end())
						{
							strProfilePath = *++Iter;
							const auto Next = Iter + 1;
							if (Next != Args.end() && *Next[0] != L'-'  && *Next[0] != L'/')
							{
								strLocalProfilePath = *Next;
								Iter = Next;
							}
						}
					}
					break;

				case L'T':
					{
						const auto Title = L"title"_sv;
						if (starts_with_icase(Arg + 1, Title))
						{
							CustomTitle.second = true;
							if (Arg[1 + Title.size()] == L':')
								CustomTitle.first = Arg + 1 + Title.size() + 1;
						}
						else if (Iter + 1 != Args.end())
						{
							strTemplatePath = *++Iter;
						}
					}
					break;

				case L'P':
					{
						Global->Opt->LoadPlug.PluginsPersonal = false;
						Global->Opt->LoadPlug.MainPluginDir = false;

						if (Arg[2])
						{
							// we can't expand it here - some environment variables might not be available yet
							Global->Opt->LoadPlug.strCustomPluginsPath = &Arg[2];
						}
						else
						{
							// если указан -P без <путь>, то, считаем, что основные
							//  плагины не загружать вооообще!!!
							Global->Opt->LoadPlug.strCustomPluginsPath.clear();
						}
					}
					break;

				case L'C':
					if (upper(Arg[2])==L'O' && !Arg[3])
					{
						Global->Opt->LoadPlug.PluginsCacheOnly = true;
						Global->Opt->LoadPlug.PluginsPersonal = false;
					}
					break;

				case L'?':
				case L'H':
					ControlObject::ShowCopyright(1);
					show_help();
					return 0;

#ifdef DIRECT_RT
				case L'D':
					if (upper(Arg[2])==L'O' && !Arg[3])
						Global->DirectRT=true;
					break;
#endif
				case L'W':
					{
						if(Arg[2] == L'-')
						{
							Global->Opt->WindowMode= false;
						}
						else if(!Arg[2])
						{
							Global->Opt->WindowMode= true;
						}
					}
					break;

				case L'R':
					if (upper(Arg[2]) == L'O')
					{
						if (!Arg[3]) // -ro
						{
							Global->Opt->ReadOnlyConfig = TRUE;
						}
						else if (Arg[3] == L'-') // -ro-
						{
							Global->Opt->ReadOnlyConfig = FALSE;
						}
					}
					break;
			}
		}
		else // простые параметры. Их может быть max две штукА.
		{
			if (CntDestName < 2)
			{
				if (IsPluginPrefixPath(Arg))
				{
					DestNames[CntDestName++] = Arg;
				}
				else
				{
					auto ArgvI = ConvertNameToFull(unquote(os::env::expand(Arg)));
					if (os::fs::exists(ArgvI))
					{
						DestNames[CntDestName++] = ArgvI;
					}
				}
			}
		}
	}

	InitTemplateProfile(strTemplatePath);
	InitProfile(strProfilePath, strLocalProfilePath);
	Global->m_ConfigProvider = new config_provider;

	Global->Opt->Load(std::move(Overrides));

	//Инициализация массива клавиш.
	InitKeysArray();

	if (!Global->Opt->LoadPlug.MainPluginDir) //если есть ключ /p то он отменяет /co
		Global->Opt->LoadPlug.PluginsCacheOnly=false;

	if (Global->Opt->LoadPlug.PluginsCacheOnly)
	{
		Global->Opt->LoadPlug.strCustomPluginsPath.clear();
		Global->Opt->LoadPlug.MainPluginDir=false;
		Global->Opt->LoadPlug.PluginsPersonal=false;
	}

	InitConsole();
	if (CustomTitle.second)
		ConsoleTitle::SetUserTitle(CustomTitle.first.empty() ? Global->strInitTitle : CustomTitle.first);

	SCOPE_EXIT
	{
		// BUGBUG, reorder to delete only in ~global()
		delete Global->CtrlObject;
		Global->CtrlObject = nullptr;

		ClearInternalClipboard();
		CloseConsole();
	};

	far_language::instance().load(Global->g_strFarPath, Global->Opt->strLanguage, static_cast<int>(lng::MNewFileName + 1));

	os::env::set(L"FARLANG", Global->Opt->strLanguage);

	if (!Global->Opt->LoadPlug.strCustomPluginsPath.empty())
		Global->Opt->LoadPlug.strCustomPluginsPath = ConvertNameToFull(unquote(os::env::expand(Global->Opt->LoadPlug.strCustomPluginsPath)));

	UpdateErrorMode();

	SetDriveMenuHotkeys();

	Global->CtrlObject = new ControlObject;

	NoElevationDuringBoot.reset();

	try
	{
		return MainProcess(strEditName, strViewName, DestNames[0], DestNames[1], StartLine, StartChar);
	}
	catch (const std::exception& e)
	{
		if (ProcessStdException(e, L"mainImpl"_sv))
			std::terminate();
		throw;
	}
#if COMPILER == C_GCC
	catch (...)
	{
		if (ProcessUnknownException(L"mainImpl"_sv))
			std::terminate();
		throw;
	}
#else
	// Absence of catch (...) block here is deliberate:
	// Unknown C++ exceptions will be caught by FarUnhandledExceptionFilter
	// and processed as SEH exceptions in more advanced way
#endif

}

void override_stream_buffers()
{
	std::ios::sync_with_stdio(false);

	static consolebuf
		BufIn,
		BufOut,
		BufErr,
		BufLog;

	auto Color = colors::ConsoleColorToFarColor(F_LIGHTRED);
	MAKE_TRANSPARENT(Color.BackgroundColor);
	BufErr.color(Color);

	static const io::wstreambuf_override
		In(std::wcin, BufIn),
		Out(std::wcout, BufOut),
		Err(std::wcerr, BufErr),
		Log(std::wclog, BufLog);
}

static int wmain_seh(int Argc, wchar_t *Argv[])
{
#if defined(SYSLOG)
	atexit(PrintSysLogStat);
#endif

	override_stream_buffers();

	SCOPED_ACTION(tracer);
	SCOPED_ACTION(unhandled_exception_filter);
	SCOPED_ACTION(new_handler);

	try
	{
		return mainImpl(make_range(Argv + 1, Argv + Argc));
	}
	catch (const std::exception& e)
	{
		if (ProcessStdException(e, L"wmain"_sv))
			std::terminate();

		unhandled_exception_filter::dismiss();
		RestoreGPFaultUI();
		throw;
	}
#if COMPILER == C_GCC
	catch (...)
	{
		if (ProcessUnknownException(L"mainImpl"_sv))
			std::terminate();

		unhandled_exception_filter::dismiss();
		RestoreGPFaultUI();
		throw;
	}
#else
	// Absence of catch (...) block here is deliberate:
	// Unknown C++ exceptions will be caught by FarUnhandledExceptionFilter
	// and processed as SEH exceptions in more advanced way
#endif
}

int main()
{
	return seh_invoke_with_ui(
	[]
	{
		// wmain is a non-standard extension and not available in gcc.
		int Argc;
		const os::memory::local::ptr<wchar_t*> Argv(CommandLineToArgvW(GetCommandLine(), &Argc));
		return wmain_seh(Argc, Argv.get());
	},
	[]() -> int
	{
		std::terminate();
	},
	L"main");
}
