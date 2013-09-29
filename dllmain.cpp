//////////////////////////////////////////////////////////////////////////
//
// dllmain.cpp : Implements DLL exports and COM class factory
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Note: This source file implements the class factory for the sample
//       media source, plus the following DLL functions:
//       - DllMain
//       - DllCanUnloadNow
//       - DllRegisterServer
//       - DllUnregisterServer
//       - DllGetClassObject
//
//////////////////////////////////////////////////////////////////////////

#include "FlvSource.h"
#include "FlvByteStreamHandler.h"

#include <assert.h>
#include <strsafe.h>

#include <initguid.h>
const DWORD CHARS_IN_GUID = 39;


HRESULT RegisterObject(
    HMODULE hModule,
    const GUID& guid,
    const TCHAR *pszDescription,
    const TCHAR *pszThreadingModel
    );

HRESULT UnregisterObject(const GUID& guid);

// {EFE6208A-0A2C-49fa-8A01-3768B559B6DA}
DEFINE_GUID(CLSID_MFFlvByteStreamHandler,
0xefe6208a, 0xa2c, 0x49fa, 0x8a, 0x1, 0x37, 0x68, 0xb5, 0x59, 0xb6, 0xda);

// Module Ref count
long g_cRefModule = 0;

// Handle to the DLL's module
HMODULE g_hModule = NULL;

void DllAddRef()
{
    InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRefModule);
}



// Text strings

// Description string for the bytestream handler.
const TCHAR* sByteStreamHandlerDescription = TEXT("Flv Source ByteStreamHandler");

// File extension for WAVE files.
const TCHAR* sFileExtension = TEXT(".flv");

// Registry location for bytestream handlers.
const TCHAR* REGKEY_MF_BYTESTREAM_HANDLERS
                = TEXT("Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers");


// Functions to register and unregister the byte stream handler.

HRESULT RegisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension, const TCHAR *sDescription);
HRESULT UnregisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension);

// Misc Registry helpers
HRESULT SetKeyValue(HKEY hKey, const TCHAR *sName, const TCHAR *sValue);


//
// IClassFactory implementation
//

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT
{
    const CLSID *pClsid;
    PFNCREATEINSTANCE pfnCreate;
};

// Classes supported by this module:
const CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
  { &CLSID_MFFlvByteStreamHandler, FlvByteStreamHandler_CreateInstance },
};

class CClassFactory : public IClassFactory
{
public:

    static HRESULT CreateInstance(
        REFCLSID clsid,                                 // The CLSID of the object to create (from DllGetClassObject)
        const CLASS_OBJECT_INIT *pClassObjectInits,     // Array of class factory data.
        size_t cClassObjectInits,                       // Number of elements in the array.
        REFIID riid,                                    // The IID of the interface to retrieve (from DllGetClassObject)
        void **ppv                                      // Receives a pointer to the interface.
        )
    {
        *ppv = NULL;

        HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

        for (size_t i = 0; i < cClassObjectInits; i++)
        {
            if (clsid == *pClassObjectInits[i].pClsid)
            {
                IClassFactory *pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);

                if (pClassFactory)
                {
                    hr = pClassFactory->QueryInterface(riid, ppv);
                    pClassFactory->Release();
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
                break; // match found
            }
        }
        return hr;
    }

    // IUnknown methods
    IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CClassFactory, IClassFactory),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    // IClassFactory methods

    IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
    {
        return punkOuter ? CLASS_E_NOAGGREGATION : m_pfnCreate(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock)
        {
            DllAddRef();
        }
        else
        {
            DllRelease();
        }
        return S_OK;
    }

private:

    CClassFactory(PFNCREATEINSTANCE pfnCreate) : m_cRef(1), m_pfnCreate(pfnCreate)
    {
        DllAddRef();
    }

    ~CClassFactory()
    {
        DllRelease();
    }

    long m_cRef;
    PFNCREATEINSTANCE m_pfnCreate;
};

//
// Standard DLL functions
//

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hModule = (HMODULE)hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
    return CClassFactory::CreateInstance(clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}


STDAPI DllRegisterServer()
{
    HRESULT hr = S_OK;

    // Register the bytestream handler's CLSID as a COM object.
    hr = RegisterObject(
            g_hModule,                              // Module handle
            CLSID_MFFlvByteStreamHandler,   // CLSID
            sByteStreamHandlerDescription,          // Description
            TEXT("Both")                            // Threading model
            );

    // Register the bytestream handler as the handler for the MPEG-1 file extension.
    if (SUCCEEDED(hr))
    {
        hr = RegisterByteStreamHandler(
          CLSID_MFFlvByteStreamHandler,       // CLSID
            sFileExtension,                             // Supported file extension
            sByteStreamHandlerDescription               // Description
            );
    }

    return hr;
}

STDAPI DllUnregisterServer()
{
    // Unregister the CLSIDs
  UnregisterObject(CLSID_MFFlvByteStreamHandler);

    // Unregister the bytestream handler for the file extension.
  UnregisterByteStreamHandler(CLSID_MFFlvByteStreamHandler, sFileExtension);

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

HRESULT RegisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension, const TCHAR *sDescription)
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
        hr = StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
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
        hr = RegSetValueEx(
            hSubKey,
            szCLSID,
            0,
            REG_SZ,
            (BYTE*)sDescription,
            static_cast<DWORD>((cchDescription + 1) * sizeof(TCHAR))
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
HRESULT CreateObjectKeyName(const GUID& guid, TCHAR *sName, DWORD cchMax)
{
    // convert CLSID uuid to string
    OLECHAR szCLSID[CHARS_IN_GUID];
    HRESULT hr = StringFromGUID2(guid, szCLSID, CHARS_IN_GUID);
    if (SUCCEEDED(hr))
    {
        // Create a string of the form "CLSID\{clsid}"
        hr = StringCchPrintf(sName, cchMax, TEXT("Software\\Classes\\CLSID\\%ls"), szCLSID);
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

    LONG lRet = RegCreateKeyEx(
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
    const TCHAR *pszDescription,
    const TCHAR *pszThreadingModel
    )
{
    HKEY hKey = NULL;
    HKEY hSubkey = NULL;

    TCHAR achTemp[MAX_PATH];

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
        (void)GetModuleFileName(hModule, achTemp, MAX_PATH);

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
    TCHAR achTemp[MAX_PATH];

    HRESULT hr = CreateObjectKeyName(guid, achTemp, MAX_PATH);

    if (SUCCEEDED(hr))
    {
        // Delete the key recursively.
      LONG lRes = RegDeleteTree(HKEY_CURRENT_USER, achTemp);

        hr = HRESULT_FROM_WIN32(lRes);
    }

    return hr;
}


