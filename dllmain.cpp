#include <wrl.h>
#include "FlvSource.h"
#include "FlvByteStreamHandler.h"

#include <cassert>
#include <strsafe.h>

#include <initguid.h>
#pragma comment(lib, "runtimeobject.lib")

using namespace Microsoft::WRL;

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppv)
{
  return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}


STDAPI DllCanUnloadNow()
{
  return Module<InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}
HMODULE current_module = nullptr;
STDAPI_(BOOL) DllMain(_In_opt_ HINSTANCE hinst, DWORD reason, _In_opt_ void*)
{
  if (reason == DLL_PROCESS_ATTACH)
  {
    current_module = hinst;
    DisableThreadLibraryCalls(hinst);
  }
  return TRUE;
}

// {EFE6208A-0A2C-49fa-8A01-3768B559B6DA}
// DEFINE_GUID(CLSID_MFFlvByteStreamHandler, 0xefe6208a, 0xa2c, 0x49fa, 0x8a, 0x1, 0x37, 0x68, 0xb5, 0x59, 0xb6, 0xda);

void DllAddRef()
{
  Module<InProc>::GetModule().IncrementObjectCount();
}

void DllRelease()
{
  Module<InProc>::GetModule().DecrementObjectCount();
}

// Description string for the bytestream handler.
const wchar_t* ByteStreamHandlerDescription = L"Flv Source ByteStreamHandler";
const wchar_t* FileExtension = L".flv";
// Registry location for bytestream handlers. //HKCU/
const wchar_t* REGKEY_MF_BYTESTREAM_HANDLERS = L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers";


// Functions to register and unregister the byte stream handler.
HRESULT RegisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension, const wchar_t *sDescription);
HRESULT UnregisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension);

// Misc Registry helpers
HRESULT SetKeyValue(HKEY hKey, const wchar_t *sName, const wchar_t *sValue);


//注册在HKCU,而不是HKLM
HRESULT RegisterObject(HMODULE hModule, const GUID& guid, const wchar_t *pszDescription, const wchar_t *pszThreadingModel);
HRESULT UnregisterObject(const GUID& guid);

STDAPI DllRegisterServer()
{
    HRESULT hr = S_OK;

    // Register the bytestream handler's CLSID as a COM object.
    hr = RegisterObject(
            current_module,                              // Module handle
            __uuidof(FlvByteStreamHandler),//CLSID_MFFlvByteStreamHandler,   // CLSID
            ByteStreamHandlerDescription,          // Description
            L"Both"                            // Threading model
            );

    // Register the bytestream handler as the handler for the MPEG-1 file extension.
    if (SUCCEEDED(hr))
    {
        hr = RegisterByteStreamHandler(
          __uuidof(FlvByteStreamHandler),//CLSID_MFFlvByteStreamHandler,       // CLSID
            FileExtension,                             // Supported file extension
            ByteStreamHandlerDescription               // Description
            );
    }

    return hr;
}

STDAPI DllUnregisterServer()
{
  // Unregister the CLSIDs
  UnregisterObject(__uuidof(FlvByteStreamHandler));

  // Unregister the bytestream handler for the file extension.
  UnregisterByteStreamHandler(__uuidof(FlvByteStreamHandler), FileExtension);

  return S_OK;
}


///////////////////////////////////////////////////////////////////////
// Name: CreateRegistryKey
// Desc: Creates a new registry key. (Thin wrapper just to encapsulate
//       all of the default options.)
///////////////////////////////////////////////////////////////////////

HRESULT CreateRegistryKey(HKEY hKey, LPCTSTR subkey, HKEY *phKey)
{
    assert(phKey != NULL);

    LONG lreturn = RegCreateKeyEx(
        hKey,                 // parent key
        subkey,               // name of subkey
        0,                    // reserved
        NULL,                 // class string (can be NULL)
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,                 // security attributes
        phKey,
        NULL                  // receives the "disposition" (is it a new or existing key)
        );

    return HRESULT_FROM_WIN32(lreturn);
}

const DWORD CHARS_IN_GUID = 39;

///////////////////////////////////////////////////////////////////////
// Name: RegisterByteStreamHandler
// Desc: Register a bytestream handler for the Media Foundation
//       source resolver.
//
// guid:            CLSID of the bytestream handler.
// sFileExtension:  File extension.
// sDescription:    Description.
//
// Note: sFileExtension can also be a MIME type although that is not
//       illustrated in this project.
///////////////////////////////////////////////////////////////////////

HRESULT RegisterByteStreamHandler(const GUID& guid, const wchar_t *sFileExtension, const wchar_t *sDescription)
{
    HRESULT hr = S_OK;

    // Open HKCU/<byte stream handlers>/<file extension>

    // Create {clsid} = <description> key

    HKEY    hKey = NULL;
    HKEY    hSubKey = NULL;

    OLECHAR szCLSID[CHARS_IN_GUID];

    size_t  cchDescription = 0;
    hr = StringCchLength(sDescription, STRSAFE_MAX_CCH, &cchDescription);

    if (SUCCEEDED(hr))
    {
        StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
    }

    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, REGKEY_MF_BYTESTREAM_HANDLERS, &hKey);
    }

    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(hKey, sFileExtension, &hSubKey);
    }

    if (SUCCEEDED(hr))
    {
        hr = RegSetValueExW(
            hSubKey,
            szCLSID,
            0,
            REG_SZ,
            (BYTE*)sDescription,
            static_cast<DWORD>((cchDescription + 1) * sizeof(wchar_t))
            );
    }

    if (hSubKey != NULL)
    {
        RegCloseKey( hSubKey );
    }

    if (hKey != NULL)
    {
        RegCloseKey( hKey );
    }

    return hr;
}


HRESULT UnregisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension)
{
    TCHAR szKey[MAX_PATH];
    OLECHAR szCLSID[CHARS_IN_GUID];

    DWORD result = 0;
    HRESULT hr = S_OK;

    // Create the subkey name.
    hr = StringCchPrintf(szKey, MAX_PATH, TEXT("%s\\%s"), REGKEY_MF_BYTESTREAM_HANDLERS, sFileExtension);


    // Create the CLSID name in canonical form.
    if (SUCCEEDED(hr))
    {
        hr = StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
    }

    // Delete the CLSID entry under the subkey.
    // Note: There might be multiple entries for this file extension, so we should not delete
    // the entire subkey, just the entry for this CLSID.
    if (SUCCEEDED(hr))
    {
      result = RegDeleteKeyValue(HKEY_CURRENT_USER, szKey, szCLSID);
        if (result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(result);
        }
    }

    return hr;
}

// Converts a CLSID into a string with the form "CLSID\{clsid}"
HRESULT CreateObjectKeyName(const GUID& guid, wchar_t *sName, DWORD cchMax)
{
    // convert CLSID uuid to string
    OLECHAR szCLSID[CHARS_IN_GUID];
    HRESULT hr = StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
    if (SUCCEEDED(hr))
    {
        // Create a string of the form "CLSID\{clsid}"
        hr = StringCchPrintfW(sName, cchMax, L"Software\\Classes\\CLSID\\%ls", szCLSID);
    }
    return hr;
}

// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndValue(
    HKEY hKey,
    PCWSTR pszSubKeyName,
    PCWSTR pszValueName,
    PCWSTR pszData,
    PHKEY phkResult
    )
{
    *phkResult = NULL;

    LONG lRet = RegCreateKeyExW(
        hKey, pszSubKeyName,
        0,  NULL, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, NULL, phkResult, NULL);

    if (lRet == ERROR_SUCCESS)
    {
        lRet = RegSetValueExW(
            (*phkResult),
            pszValueName, 0, REG_SZ,
            (LPBYTE) pszData,
            ((DWORD) wcslen(pszData) + 1) * sizeof(WCHAR)
            );

        if (lRet != ERROR_SUCCESS)
        {
            RegCloseKey(*phkResult);
        }
    }

    return HRESULT_FROM_WIN32(lRet);
}

// Creates the registry entries for a COM object.

HRESULT RegisterObject(
    HMODULE hModule,
    const GUID& guid,
    const wchar_t *pszDescription,
    const wchar_t *pszThreadingModel
    )
{  
    HKEY hKey = NULL;
    HKEY hSubkey = NULL;

    wchar_t achTemp[MAX_PATH];

    // Create the name of the key from the object's CLSID
    HRESULT hr = CreateObjectKeyName(guid, achTemp, MAX_PATH);

    // Create the new key.
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
          HKEY_CURRENT_USER,
            achTemp,
            NULL,
            pszDescription,
            &hKey
            );
    }

    if (SUCCEEDED(hr))
    {
        (void)GetModuleFileNameW(hModule, achTemp, MAX_PATH);

        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // Create the "InprocServer32" subkey
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
            hKey,
            L"InProcServer32",
            NULL,
            achTemp,
            &hSubkey
            );

        RegCloseKey(hSubkey);
    }

    // Add a new value to the subkey, for "ThreadingModel" = <threading model>
    if (SUCCEEDED(hr))
    {
        hr = CreateRegKeyAndValue(
            hKey,
            L"InProcServer32",
            L"ThreadingModel",
            pszThreadingModel,
            &hSubkey
            );

        RegCloseKey(hSubkey);
    }

    // close hkeys

    RegCloseKey(hKey);

    return hr;
}

// Deletes the registry entries for a COM object.

HRESULT UnregisterObject(const GUID& guid)
{
  wchar_t achTemp[MAX_PATH];

    HRESULT hr = CreateObjectKeyName(guid, achTemp, MAX_PATH);

    if (SUCCEEDED(hr))
    {
        // Delete the key recursively.
      LONG lRes = RegDeleteTreeW(HKEY_CURRENT_USER, achTemp);

        hr = HRESULT_FROM_WIN32(lRes);
    }

    return hr;
}
