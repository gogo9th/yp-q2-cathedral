
#include "framework.h"

#include "control.h"
#include "MainWindow.h"
#include "NotifyIcon.h"
#include "winservice.h"

#include "common/scopedResource.h"
#include "common/registry.h"
#include "common/settings.h"

#include <Shellapi.h>
#include <control/mmDevices.h>

#include "utils.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


static HINSTANCE hInstance_;

HINSTANCE hInstance()
{
    return hInstance_;
}

const wchar_t * windowNameW()
{
    // should differ from installer caption to be able to find app window
    return L"StarEcho Control";
}

const wchar_t * appNameW()
{
    return L"StarEcho";
}

const char * appName()
{
    return "StarEcho";
}

//

static const std::wstring selfExeFilepath()
{
    wchar_t selfName[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, selfName, (int)std::size(selfName));
    return selfName;
}

//

enum
{
    NI_BEGIN = WM_USER + 10,
    NI_Studio,
    NI_TOPITEM = NI_Studio,
    NI_Rock,
    NI_Classical,
    NI_Jazz,
    NI_Dance,
    NI_Ballad,
    NI_Club,
    NI_RnB,
    NI_Cafe,
    NI_LiveCafe,
    NI_Concert,
    NI_Church,
    NI_Disable,
    NI_BOTITEM = NI_Disable,
    NI_Setup,
    NI_Autostart,
    NI_AuUp,
};

static const std::map<int, const wchar_t *> g_modes_ = {
    { NI_Studio,      L"studio" },
    { NI_Rock,        L"rock" },
    { NI_Classical,   L"classical" },
    { NI_Jazz,        L"jazz" },
    { NI_Dance,       L"dance" },
    { NI_Ballad,      L"ballad" },
    { NI_Club,        L"club" },
    { NI_RnB,         L"rnb" },
    { NI_Cafe,        L"cafe" },
    { NI_LiveCafe,    L"livecafe" },
    { NI_Concert,     L"concert" },
    { NI_Church,      L"church" },
};

static const std::wstring auup_mode = { L"upscaling" };


static void startSetupInstalce()
{
    ShellExecuteW(NULL, L"runas", selfExeFilepath().data(), L"setup", NULL, SW_SHOWDEFAULT);
}


std::unique_ptr<MainWindow> mw_;
static INT_PTR WINAPI dlgProc_(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return mw_->dlgProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    using namespace std::literals::string_literals;

    hInstance_ = hInstance;
    UNREFERENCED_PARAMETER(hPrevInstance);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    const bool isAdmin = isUserInAdminGroup();

    try
    {
        if (isAdmin)
        {
            if (wcscmp(lpCmdLine, L"installAll") == 0)
            {
                auto devices = getPlaybackDevices();
                for (auto & device : devices)
                {
                    if (!device->apoMfxInstalled)
                    {
                        device->installApo();
                        device->enableEnhancements();
                    }
                }
                restartAudioService();
                return 0;
            }
            else if (wcscmp(lpCmdLine, L"installDefault") == 0)
            {
                auto defaultDev = getDefaultMMDevice(eRender);
                auto devInfo = std::make_shared<MMDeviceInfo>(defaultDev);
                if (!devInfo->apoMfxInstalled)
                {
                    devInfo->installApo();
                    devInfo->enableEnhancements();
                    restartAudioService();
                }
                return 0;
            }
            else if (wcscmp(lpCmdLine, L"uninstall") == 0)
            {
                auto devices = getPlaybackDevices();
                bool wasRemoved = false;
                for (auto & device : devices)
                {
                    if (device->apoMfxInstalled)
                    {
                        device->removeApo();
                        wasRemoved = true;
                    }
                }
                if (wasRemoved)
                {
                    restartAudioService();
                }
                return 0;
            }
        }


        bool showConfig = false;
        auto wPreviousInstance = FindWindowW(nullptr, windowNameW());

        if (wcscmp(lpCmdLine, L"setup") == 0) 
        {
            // when launched with setup option, close previous instance
            if (wPreviousInstance != NULL)
            {
                //auto threadId = ::GetWindowThreadProcessId(wPreviousInstance, nullptr);
                //PostThreadMessageW(threadId, WM_QUIT, 0, 0);
                SendMessageW(wPreviousInstance, WM_QUIT, 0, 0);
            }

            if (isAdmin)
            {
                Settings::setupAppKey();
            }

            showConfig = true;
        }
        else if (wPreviousInstance != NULL)
        {
            // do not launch more instances without setup option
            return 0;
        }
        else
        {
            // not in setup and no previous instances
            // check if any playback device has apo installed, if none call setup instance
            bool hasDeviceInstalled = false;
            auto devices = getPlaybackDevices();
            for (auto & device : devices)
            {
                if (device->apoMfxInstalled)
                {
                    hasDeviceInstalled = true;
                    break;
                }
            }
            if (!hasDeviceInstalled)
            {
                startSetupInstalce();
                return 0;
            }
        }

        HMenu hMenu(CreatePopupMenu());

        if (Registry::keyExists(Settings::appPath))
        {
            InsertMenuW(hMenu, NI_Studio,   MF_BYCOMMAND, NI_Studio,    L"Studio");
            InsertMenuW(hMenu, NI_Rock,     MF_BYCOMMAND, NI_Rock,      L"Rock");
            InsertMenuW(hMenu, NI_Classical,MF_BYCOMMAND, NI_Classical, L"Classical");
            InsertMenuW(hMenu, NI_Jazz,     MF_BYCOMMAND, NI_Jazz,      L"Jazz");
            InsertMenuW(hMenu, NI_Dance,    MF_BYCOMMAND, NI_Dance,     L"Dance");
            InsertMenuW(hMenu, NI_Ballad,   MF_BYCOMMAND, NI_Ballad,    L"Ballad");
            InsertMenuW(hMenu, NI_Club,     MF_BYCOMMAND, NI_Club,      L"Club");
            InsertMenuW(hMenu, NI_RnB,      MF_BYCOMMAND, NI_RnB,       L"R&&B");
            InsertMenuW(hMenu, NI_Cafe,     MF_BYCOMMAND, NI_Cafe,      L"Cafe");
            InsertMenuW(hMenu, NI_LiveCafe, MF_BYCOMMAND, NI_LiveCafe,  L"Live Cafe");
            InsertMenuW(hMenu, NI_Concert,  MF_BYCOMMAND, NI_Concert,   L"Concert");
            InsertMenuW(hMenu, NI_Church,   MF_BYCOMMAND, NI_Church,    L"Church");
            InsertMenuW(hMenu, NI_Disable,  MF_BYCOMMAND, NI_Disable,   L"Off");
            InsertMenuW(hMenu, -1, MF_SEPARATOR, 0, nullptr);

            InsertMenuW(hMenu, NI_AuUp, MF_BYCOMMAND, NI_AuUp, L"Audio Upscaling");
            InsertMenuW(hMenu, -1, MF_SEPARATOR, 0, nullptr);

            auto effect = Settings::currentEffect();
            if (!Settings::isAnyEffectEnabled(effect))
            {
                CheckMenuRadioItem(hMenu, NI_TOPITEM, NI_BOTITEM, NI_Disable, MF_BYCOMMAND);
            }
            else
            {
                auto effects = stringSplit(effect, L";");
                auto iAuup = std::find(effects.begin(), effects.end(), auup_mode);
                if (iAuup != effects.end())
                {
                    effects.erase(iAuup);
                    CheckMenuItem(hMenu, NI_AuUp, MF_BYCOMMAND | MF_CHECKED);
                }
                
                int currentMode = -1;
                if (!effects.empty())
                {
                    auto & e = effects[0];
                    for (auto & [k,v] : g_modes_)
                    {
                        if (e == v)
                        {
                            currentMode = k;
                            CheckMenuRadioItem(hMenu, NI_TOPITEM, NI_BOTITEM, k, MF_BYCOMMAND);
                            break;
                        }
                    }
                }

                if (currentMode < 0)
                {
                    Settings::setEffect(std::wstring(g_modes_.at(NI_Church)));
                    CheckMenuRadioItem(hMenu, NI_TOPITEM, NI_BOTITEM, NI_Church, MF_BYCOMMAND);
                }
            }
        }

        InsertMenuW(hMenu, NI_Setup, MF_BYCOMMAND, NI_Setup, L"Setup");
        if (!isAdmin)
        {
            SHSTOCKICONINFO stockIconInfo;
            stockIconInfo.cbSize = sizeof(stockIconInfo);
            if (SHGetStockIconInfo(SHSTOCKICONID::SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &stockIconInfo) == S_OK)
            {
                Icon icon(stockIconInfo.hIcon, true);
                ICONINFO iconInfo;
                GetIconInfo(icon.small(), &iconInfo);

                MENUITEMINFOW mi;
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_BITMAP;
                //mi.hbmpItem = iconInfo.hbmColor;
                mi.hbmpItem = (HBITMAP)CopyImage(iconInfo.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
                SetMenuItemInfoW(hMenu, NI_Setup, FALSE, &mi);

                //SetMenuItemBitmaps()
            }
        }

        InsertMenuW(hMenu, NI_Autostart, MF_BYCOMMAND, NI_Autostart, L"Launch on Startup");
        CheckMenuItem(hMenu, NI_Autostart, MF_BYCOMMAND | (Settings::isAutostart(appNameW(), selfExeFilepath()) ? MF_CHECKED : MF_UNCHECKED));

        InsertMenuW(hMenu, WM_QUIT, MF_BYCOMMAND, WM_QUIT, L"Exit");

        mw_ = std::make_unique<MainWindow>(dlgProc_);
        mw_->create();

        NotifyIcon ni(IDC_MYICON, *mw_);
        ni.setMenu(hMenu);

        if (showConfig)
        {
            mw_->show();
        }

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            if (msg.message == NI_AuUp)
            {
                auto auupEnabled = GetMenuState(hMenu, NI_AuUp, MF_BYCOMMAND | MF_CHECKED) == MF_CHECKED;
                auupEnabled = !auupEnabled;
                CheckMenuItem(hMenu, NI_AuUp, MF_BYCOMMAND | (auupEnabled ? MF_CHECKED : MF_UNCHECKED));

                auto effects = stringSplit(Settings::currentEffect(), L";");
                auto iAuup = std::find(effects.begin(), effects.end(), auup_mode);
                if (iAuup != effects.end() && !auupEnabled)
                {
                    effects.erase(iAuup);
                    if (effects.empty())
                    {
                        Settings::setEffect(Settings::effectDisabledString());
                    }
                    else
                    {
                        Settings::setEffect(stringJoin(effects, L";"));
                    }
                }
                else if (iAuup == effects.end() && auupEnabled)
                {
                    effects.insert(effects.begin(), auup_mode);
                    auto iDisabled = std::find(effects.begin(), effects.end(), Settings::effectDisabledString());
                    if (iDisabled != effects.end())
                    {
                        effects.erase(iDisabled);
                    }
                    Settings::setEffect(stringJoin(effects, L";"));
                }
            }
            else
            {
                static const auto auupStr = [&hMenu] () -> std::wstring
                {
                    auto auupEnabled = GetMenuState(hMenu, NI_AuUp, MF_BYCOMMAND | MF_CHECKED) == MF_CHECKED;
                    if (auupEnabled)
                    {
                        return auup_mode + L';';
                    }
                    return {};
                };

                auto im = g_modes_.find(msg.message);
                if (im != g_modes_.end())
                {
                    CheckMenuRadioItem(hMenu, NI_TOPITEM, NI_BOTITEM, im->first, MF_BYCOMMAND);

                    auto effect = auupStr() + im->second;
                    Settings::setEffect(effect);
                }
                else if (msg.message == NI_Disable)
                {
                    CheckMenuRadioItem(hMenu, NI_TOPITEM, NI_BOTITEM, NI_Disable, MF_BYCOMMAND);
                    auto effect = auupStr();
                    Settings::setEffect(effect.empty() ? Settings::effectDisabledString() : effect);
                }
                else
                {
                    switch (msg.message)
                    {
                        case NI_Setup:
                            if (isAdmin)
                            {
                                mw_->show();
                            }
                            else
                            {
                                startSetupInstalce();
                            }
                            break;

                        case NI_Autostart:
                        {
                            auto current = GetMenuState(hMenu, NI_Autostart, MF_BYCOMMAND | MF_CHECKED) == MF_CHECKED;
                            current = !current;
                            if (current)
                            {
                                Settings::setAutostart(appNameW(), selfExeFilepath());
                            }
                            else
                            {
                                Settings::removeAutostart(appNameW());
                            }
                            CheckMenuItem(hMenu, NI_Autostart, MF_BYCOMMAND | (current ? MF_CHECKED : MF_UNCHECKED));
                            break;
                        }

                        default:
                            TranslateMessage(&msg);
                            DispatchMessageW(&msg);
                    }
                }
            }
        }

        // turn off effects on exit
        if (Registry::keyExists(Settings::appPath))
        {
            Settings::setEffect(Settings::effectDisabledString());
        }

        return 0;
    }
    catch (const Error & e)
    {
        return MessageBoxA(0, ("Error : "s + e.what()).c_str(), appName(), MB_OK | MB_ICONERROR);
    }
    catch (const WError & e)
    {
        return MessageBoxW(0, (L"Error : "s + e.what()).c_str(), appNameW(), MB_OK | MB_ICONERROR);
    }
    catch (const std::exception & e)
    {
        return MessageBoxA(0, ("Exception : "s + e.what()).c_str(), appName(), MB_OK | MB_ICONERROR);
    }

    CoUninitialize();

    return -1;
}

bool isUserInAdminGroup()
{
    BOOL r;

    PSid adminGroup;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    r = AllocateAndInitializeSid(&NtAuthority,
                                 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup);
    if (r)
    {
        if (!CheckTokenMembership(NULL, adminGroup, &r))
        {
            r = FALSE;
        }
    }

    return r;
}

static bool switchServiceOff(std::shared_ptr<SCM::Service> &service, int state)
{
    return (state != SERVICE_RUNNING
        || (service->stop() && service->status().state == SERVICE_STOPPED));
}

static bool switchServiceOn(std::shared_ptr<SCM::Service> &service, int state)
{
    return (state == SERVICE_RUNNING
        || (service->start() && service->status().state == SERVICE_RUNNING));
}

bool restartAudioService()
{
    auto scm = std::make_shared<SCM>();

    auto service = scm->service(L"audiosrv");

    auto deps = service->dependencies();
    std::vector<SCM::Service::Status> depStates;
    for (auto & dep : deps)
    {
        depStates.push_back(dep->status());
    }

    // will stop if was running
    if (service->status().state == SERVICE_RUNNING)
    {
        // deps first
        try
        {
            for (size_t i = 0; i < deps.size(); i++)
            {
                if (!switchServiceOff(deps[i], depStates[i].state)) return false;
            }
        }
        catch (const WError & e)
        {
            throw WError(L"Error while stopping dependent services: " + e.what());
        }

        if (!switchServiceOff(service, service->status().state)) return false;
    }

    // then start

    if (!switchServiceOn(service, service->status().state)) return false;

    // and then deps 
    try
    {
        for (size_t i = 0; i < deps.size(); i++)
        {
            if (depStates[i].state == SERVICE_RUNNING)
            {
                if (!switchServiceOn(deps[i], deps[i]->status().state)) return false;
            }
        }
    }
    catch (const WError & )
    {
        // return false as partial error but start service anyway
        return false;
    }

    return true;
}
