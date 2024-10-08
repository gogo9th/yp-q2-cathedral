
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "mmDeviceInfo.h"
#include "mmDevices.h"

#include "common/registry.h"
#include "common/stringUtils.h"
#include "apo/Q2APO.h"


static auto renderKey_ = L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render\\"s;


MMDeviceInfo::MMDeviceInfo(const std::wstring & deviceGuid)
{
    auto devRoot = Registry::openKey(renderKey_ + deviceGuid);

    Registry::getValue(devRoot, L"DeviceState", deviceState);

    if (deviceState & DEVICE_STATE_NOTPRESENT
        || deviceState & DEVICE_STATE_DISABLED)
        return;

    guid = deviceGuid;
    toUpperCase(guid);

    auto hKeyProps = Registry::openKey(devRoot, L"Properties");
    Registry::getValue(hKeyProps, toString(PKEY_Endpoint_Name), endpointName);
    Registry::getValue(hKeyProps, toString(PKEY_Device_DeviceDesc), deviceName);

    if (Registry::keyExists(devRoot, L"FxProperties"))
    {
        auto hKeyFxProps = Registry::openKey(devRoot, L"FxProperties");

        auto KEY_AudioEndpoint_Disable_SysFx = toString(PKEY_AudioEndpoint_Disable_SysFx);
        if (Registry::valueExists(hKeyFxProps, KEY_AudioEndpoint_Disable_SysFx))
        {
            DWORD ed;
            Registry::getValue(hKeyFxProps, KEY_AudioEndpoint_Disable_SysFx, ed);
            enhancementsDisabled = ed != ENDPOINT_SYSFX_ENABLED;
        }

        auto KEY_FX_ModeEffectClsid = toString(PKEY_FX_ModeEffectClsid);
        if (Registry::valueExists(hKeyFxProps, KEY_FX_ModeEffectClsid))
        {
            std::wstring apoGuid;
            Registry::getValue(hKeyFxProps, KEY_FX_ModeEffectClsid, apoGuid);
            if (apoGuid == toString(__uuidof(Q2APOMFX)))
            {
                apoMfxInstalled = true;
            }
        }
    }

}

void MMDeviceInfo::installApo()
{
    auto hKeyFxProps = Registry::openKey(renderKey_ + guid + L"\\FxProperties", KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY);

    auto KEY_FX_ModeEffectClsid = toString(PKEY_FX_ModeEffectClsid);
    if (Registry::valueExists(hKeyFxProps, KEY_FX_ModeEffectClsid))
    {
        std::wstring previousApoGuid;
        Registry::getValue(hKeyFxProps, KEY_FX_ModeEffectClsid, previousApoGuid);
        Registry::setValue(hKeyFxProps, toString(PKEY_Q2FX_PreviousEffectClsid), previousApoGuid);
    }
    Registry::setValue(hKeyFxProps, KEY_FX_ModeEffectClsid, toString(__uuidof(Q2APOMFX)));

    apoMfxInstalled = true;
}

void MMDeviceInfo::removeApo()
{
    auto hKeyFxProps = Registry::openKey(renderKey_ + guid + L"\\FxProperties", KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY);

    auto KEY_FX_ModeEffectClsid = toString(PKEY_FX_ModeEffectClsid);
    if (Registry::valueExists(hKeyFxProps, KEY_FX_ModeEffectClsid))
    {
        auto KEY_Q2FX_PreviousEffectClsid = toString(PKEY_Q2FX_PreviousEffectClsid);
        if (Registry::valueExists(hKeyFxProps, KEY_Q2FX_PreviousEffectClsid))
        {
            std::wstring previousApoGuid;
            Registry::getValue(hKeyFxProps, KEY_Q2FX_PreviousEffectClsid, previousApoGuid);
            Registry::setValue(hKeyFxProps, KEY_FX_ModeEffectClsid, previousApoGuid);
            Registry::deleteValue(hKeyFxProps, KEY_Q2FX_PreviousEffectClsid);
        }
        else
        {
            Registry::deleteValue(hKeyFxProps, KEY_FX_ModeEffectClsid);
        }
    }

    apoMfxInstalled = false;
}

void MMDeviceInfo::applyTodo()
{
    if (guid.empty())
    {
        return;
    }

    switch (toDoState)
    {
        case ApoToBeInstalled:
        {
            installApo();
            toDoState = ToDoNothing;
            break;
        }

        case ApoToBeRemoved:
        {
            removeApo();
            toDoState = ToDoNothing;
            break;
        }
    }
}

void MMDeviceInfo::enableEnhancements()
{
    auto hKeyFxProps = Registry::openKey(renderKey_ + guid + L"\\FxProperties", KEY_SET_VALUE | KEY_WOW64_64KEY);
    Registry::setValue(hKeyFxProps, toString(PKEY_AudioEndpoint_Disable_SysFx), 0);
    enhancementsDisabled = false;
}

std::vector<std::shared_ptr<MMDeviceInfo>> getPlaybackDevices()
{
    std::vector<std::shared_ptr<MMDeviceInfo>> r;

    auto devices = Registry::subKeys(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render");
    for (auto & device : devices)
    {
        auto devInfo = std::make_shared<MMDeviceInfo>(device);
        if (!devInfo->isPresent())
        {
            continue;
        }

        r.push_back(devInfo);
    }
    return r;
}
