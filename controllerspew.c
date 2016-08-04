/* 
 * CONTROLLERSPEW
 * Copyright © Camilla Berglund <elmindreda@glfw.org>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would
 *    be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 */

#define UNICODE
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <initguid.h>
#include <dinput.h>
#include <xinput.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct Context
{
    IDirectInput8W* dinput8;
    RAWINPUTDEVICELIST* ridl;
    UINT ridlCount;
    FILE* stream;
} Context;

static void fail(const WCHAR* message)
{
    MessageBoxW(NULL, L"Fail", message, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);

    /* Determined by D20 roll */
    ExitProcess(17);
}

static void terminate(Context* context)
{
    free(context->ridl);

    if (context->dinput8)
        IDirectInput8_Release(context->dinput8);

    if (context->stream)
        fclose(context->stream);
}

static BOOL supports_xinput(Context* context, const GUID* guid)
{
    UINT i;
    BOOL result = FALSE;

    for (i = 0;  i < context->ridlCount;  i++)
    {
        RID_DEVICE_INFO rdi = { 0 };
        char name[256];
        UINT size;

        if (context->ridl[i].dwType != RIM_TYPEHID)
            continue;

        rdi.cbSize = sizeof(rdi);
        size = sizeof(rdi);

        if ((INT) GetRawInputDeviceInfoA(context->ridl[i].hDevice,
                                         RIDI_DEVICEINFO,
                                         &rdi, &size) == -1)
        {
            continue;
        }

        if (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != guid->Data1)
            continue;

        memset(name, 0, sizeof(name));
        size = sizeof(name);

        if ((INT) GetRawInputDeviceInfoA(context->ridl[i].hDevice,
                                         RIDI_DEVICENAME,
                                         name, &size) == -1)
        {
            break;
        }

        name[sizeof(name) - 1] = '\0';
        if (strstr(name, "IG_"))
            return TRUE;
    }

    return FALSE;
}

static BOOL CALLBACK device_callback(const DIDEVICEINSTANCE* di, void* user)
{
    Context* context = user;
    IDirectInputDevice8W* device;
    DIDEVCAPS dc = { 0 };
    char name[MAX_PATH * 2];

    if (FAILED(IDirectInput8_CreateDevice(context->dinput8,
                                          &di->guidInstance,
                                          &device,
                                          NULL)))
    {
        fprintf(stderr, "Fail\n");
        return DIENUM_CONTINUE;
    }

    dc.dwSize = sizeof(dc);

    if (FAILED(IDirectInputDevice8_GetCapabilities(device, &dc)))
    {
        IDirectInputDevice8_Release(device);

        fprintf(stderr, "Fail\n");
        return DIENUM_CONTINUE;
    }

    IDirectInputDevice8_Release(device);

    if (!WideCharToMultiByte(CP_UTF8,
                             0,
                             di->tszInstanceName,
                             -1,
                             name,
                             sizeof(name),
                             NULL,
                             NULL))
    {
        fprintf(stderr, "Fail\n");
        return DIENUM_CONTINUE;
    }

    fprintf(context->stream,
            "%s (%u:%u:%u:%u): %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
            name,
            dc.dwAxes,
            dc.dwPOVs,
            dc.dwButtons,
            supports_xinput(context, &di->guidProduct),
            di->guidProduct.Data1,
            di->guidProduct.Data2,
            di->guidProduct.Data3,
            di->guidProduct.Data4[0],
            di->guidProduct.Data4[1],
            di->guidProduct.Data4[2],
            di->guidProduct.Data4[3],
            di->guidProduct.Data4[4],
            di->guidProduct.Data4[5],
            di->guidProduct.Data4[6],
            di->guidProduct.Data4[7]);

    return DIENUM_CONTINUE;
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prevInstance,
                     LPSTR commandLine,
                     int showCommand)
{
    Context context = { 0 };
    WCHAR path[MAX_PATH] = L"controllers.txt";
    OPENFILENAMEW ofn = { 0 };
    DWORD i;

    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = instance;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.TXT\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path) / sizeof(WCHAR);
    ofn.Flags = OFN_DONTADDTORECENT |
                OFN_EXPLORER |
                OFN_NOCHANGEDIR |
                OFN_OVERWRITEPROMPT;

    if (!GetSaveFileNameW(&ofn))
        ExitProcess(0);

    if (_wfopen_s(&context.stream, path, L"w") != 0)
    {
        terminate(&context);
        fail(L"Failed to open file");
    }

    if (GetRawInputDeviceList(NULL,
                              &context.ridlCount,
                              sizeof(RAWINPUTDEVICELIST)) != 0)
    {
        terminate(&context);
        fail(L"Failed to retrieve raw input device count");
    }

    context.ridl = calloc(context.ridlCount, sizeof(RAWINPUTDEVICELIST));

    if (GetRawInputDeviceList(context.ridl,
                              &context.ridlCount,
                              sizeof(RAWINPUTDEVICELIST)) == (UINT) -1)
    {
        terminate(&context);
        fail(L"Failed to retrieve raw input devices");
    }

    if (FAILED(DirectInput8Create(instance,
                                  DIRECTINPUT_VERSION,
                                  &IID_IDirectInput8W,
                                  (void**) &context.dinput8,
                                  NULL)))
    {
        terminate(&context);
        fail(L"Failed to create DirectInput8 interface");
    }

    if (FAILED(IDirectInput8_EnumDevices(context.dinput8,
                                         DI8DEVCLASS_GAMECTRL,
                                         device_callback,
                                         &context,
                                         DIEDFL_ALLDEVICES)))
    {
        terminate(&context);
        fail(L"Failed to enumerate DirectInput8 devices");
    }

    for (i = 0;  i < 4;  i++)
    {
        XINPUT_STATE xis = { 0 };

        if (XInputGetState(i, &xis) == ERROR_SUCCESS)
            fprintf(context.stream, "XInput Controller %u\n", i);
    }

    terminate(&context);
    return 0;
}
