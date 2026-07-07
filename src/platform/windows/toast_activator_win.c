/*
 * Windows toast-activation COM server + registration.
 *
 * See include/toast_activator.h for the why. Flow: Windows launches this exe
 * with "-Embedding" when a toast action is clicked; run_toast_activator_server
 * serves an INotificationActivationCallback whose Activate() calls
 * notify_adhan_stop(). register_toast_activator writes the CLSID LocalServer32
 * key and a Start-Menu shortcut carrying the AUMID + ToastActivatorCLSID.
 */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <objbase.h>
#include <propidl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>

#include "notification.h"
#include "toast_activator.h"

/* -- Identity --------------------------------------------------------------- */

/* Must match MUSLIMTIFY_AUMID in notification_win.c. */
static const wchar_t MUSLIMTIFY_AUMID[] = L"Muslimtify";

/* Muslimtify's fixed ToastActivatorCLSID: {C0FFEE01-2B3C-4D5E-8F60-A1B2C3D4E5F6} */
static const CLSID CLSID_MuslimtifyToastActivator = {
    0xc0ffee01, 0x2b3c, 0x4d5e, {0x8f, 0x60, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6}};

/* IIDs we match in QueryInterface. Named with a trailing underscore so they do
   not clash with the libuuid externs of the same base name. */
static const IID IID_IUnknown_ = {
    0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_IClassFactory_ = {
    0x00000001, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
/* INotificationActivationCallback (from notificationactivationcallback.h) */
static const IID IID_INotificationActivationCallback_ = {
    0x53e31837, 0x6600, 0x4a81, {0x93, 0x95, 0x75, 0xcf, 0xfe, 0x74, 0x6f, 0x94}};

static void dbg(const char *msg) { OutputDebugStringA(msg); }

/* -- INotificationActivationCallback ---------------------------------------- */

typedef struct {
  LPCWSTR Key;
  LPCWSTR Value;
} NOTIF_INPUT_DATA;

typedef struct ActivationCallback ActivationCallback;
typedef struct ActivationCallbackVtbl {
  HRESULT(STDMETHODCALLTYPE *QueryInterface)(ActivationCallback *, REFIID, void **);
  ULONG(STDMETHODCALLTYPE *AddRef)(ActivationCallback *);
  ULONG(STDMETHODCALLTYPE *Release)(ActivationCallback *);
  HRESULT(STDMETHODCALLTYPE *Activate)
  (ActivationCallback *, LPCWSTR appUserModelId, LPCWSTR invokedArgs,
   const NOTIF_INPUT_DATA *data, ULONG count);
} ActivationCallbackVtbl;
struct ActivationCallback {
  const ActivationCallbackVtbl *lpVtbl;
};

static HRESULT STDMETHODCALLTYPE cb_QI(ActivationCallback *This, REFIID riid, void **ppv) {
  if (!ppv)
    return E_POINTER;
  if (IsEqualGUID(riid, &IID_IUnknown_) ||
      IsEqualGUID(riid, &IID_INotificationActivationCallback_)) {
    *ppv = This;
    return S_OK;
  }
  *ppv = NULL;
  return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cb_AddRef(ActivationCallback *This) {
  (void)This;
  return 2;
}
static ULONG STDMETHODCALLTYPE cb_Release(ActivationCallback *This) {
  (void)This;
  return 1;
}
static HRESULT STDMETHODCALLTYPE cb_Activate(ActivationCallback *This, LPCWSTR aumid,
                                             LPCWSTR invokedArgs, const NOTIF_INPUT_DATA *data,
                                             ULONG count) {
  (void)This;
  (void)aumid;
  (void)data;
  (void)count;
  dbg("[muslimtify] toast Activate() called\n");
  /* Our only action is Stop; stop regardless of the exact argument. */
  if (!invokedArgs || wcsstr(invokedArgs, L"stop") != NULL || invokedArgs[0] == L'\0')
    notify_adhan_stop();
  PostQuitMessage(0); /* end the server message loop */
  return S_OK;
}

static const ActivationCallbackVtbl g_cb_vtbl = {cb_QI, cb_AddRef, cb_Release, cb_Activate};
static ActivationCallback g_cb = {&g_cb_vtbl};

/* -- IClassFactory ---------------------------------------------------------- */

typedef struct ClassFactory ClassFactory;
typedef struct ClassFactoryVtbl {
  HRESULT(STDMETHODCALLTYPE *QueryInterface)(ClassFactory *, REFIID, void **);
  ULONG(STDMETHODCALLTYPE *AddRef)(ClassFactory *);
  ULONG(STDMETHODCALLTYPE *Release)(ClassFactory *);
  HRESULT(STDMETHODCALLTYPE *CreateInstance)(ClassFactory *, IUnknown *, REFIID, void **);
  HRESULT(STDMETHODCALLTYPE *LockServer)(ClassFactory *, BOOL);
} ClassFactoryVtbl;
struct ClassFactory {
  const ClassFactoryVtbl *lpVtbl;
};

static HRESULT STDMETHODCALLTYPE cf_QI(ClassFactory *This, REFIID riid, void **ppv) {
  if (!ppv)
    return E_POINTER;
  if (IsEqualGUID(riid, &IID_IUnknown_) || IsEqualGUID(riid, &IID_IClassFactory_)) {
    *ppv = This;
    return S_OK;
  }
  *ppv = NULL;
  return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cf_AddRef(ClassFactory *This) {
  (void)This;
  return 2;
}
static ULONG STDMETHODCALLTYPE cf_Release(ClassFactory *This) {
  (void)This;
  return 1;
}
static HRESULT STDMETHODCALLTYPE cf_CreateInstance(ClassFactory *This, IUnknown *outer, REFIID riid,
                                                   void **ppv) {
  (void)This;
  if (outer)
    return CLASS_E_NOAGGREGATION;
  return g_cb.lpVtbl->QueryInterface(&g_cb, riid, ppv);
}
static HRESULT STDMETHODCALLTYPE cf_LockServer(ClassFactory *This, BOOL lock) {
  (void)This;
  (void)lock;
  return S_OK;
}

static const ClassFactoryVtbl g_cf_vtbl = {cf_QI, cf_AddRef, cf_Release, cf_CreateInstance,
                                           cf_LockServer};
static ClassFactory g_cf = {&g_cf_vtbl};

/* -- Server ----------------------------------------------------------------- */

int run_toast_activator_server(void) {
  DWORD cookie = 0;
  MSG msg;
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr))
    return -1;

  hr = CoRegisterClassObject(&CLSID_MuslimtifyToastActivator, (IUnknown *)&g_cf,
                             CLSCTX_LOCAL_SERVER, REGCLS_SINGLEUSE, &cookie);
  if (FAILED(hr)) {
    dbg("[muslimtify] CoRegisterClassObject failed\n");
    CoUninitialize();
    return -1;
  }

  /* Pump until Activate posts WM_QUIT, or a safety timeout fires. */
  SetTimer(NULL, 1, 20000, NULL);
  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    if (msg.message == WM_TIMER)
      break;
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  CoRevokeClassObject(cookie);
  CoUninitialize();
  return 0;
}

/* -- Registration ----------------------------------------------------------- */

static int register_clsid_localserver(const wchar_t *exe) {
  wchar_t clsid_str[64];
  wchar_t keypath[320];
  wchar_t cmd[MAX_PATH + 8];
  HKEY hk = NULL;
  LONG rc;

  if (StringFromGUID2(&CLSID_MuslimtifyToastActivator, clsid_str,
                      (int)(sizeof(clsid_str) / sizeof(clsid_str[0]))) == 0)
    return -1;

  swprintf(keypath, sizeof(keypath) / sizeof(keypath[0]),
           L"Software\\Classes\\CLSID\\%ls\\LocalServer32", clsid_str);

  rc = RegCreateKeyExW(HKEY_CURRENT_USER, keypath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                       &hk, NULL);
  if (rc != ERROR_SUCCESS)
    return -1;

  swprintf(cmd, sizeof(cmd) / sizeof(cmd[0]), L"\"%ls\"", exe);
  rc = RegSetValueExW(hk, NULL, 0, REG_SZ, (const BYTE *)cmd,
                      (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
  RegCloseKey(hk);
  return (rc == ERROR_SUCCESS) ? 0 : -1;
}

/* Register the AUMID -> ToastActivatorCLSID association purely via the registry
   (the modern shortcut-free path used by the CommunityToolkit):
     HKCU\Software\Classes\AppUserModelId\<AUMID>
        CustomActivator = "{CLSID}"
        DisplayName     = "Muslimtify"                                        */
static int register_aumid(void) {
  wchar_t clsid_str[64];
  wchar_t keypath[320];
  static const wchar_t display_name[] = L"Muslimtify";
  HKEY hk = NULL;
  LONG rc;

  if (StringFromGUID2(&CLSID_MuslimtifyToastActivator, clsid_str,
                      (int)(sizeof(clsid_str) / sizeof(clsid_str[0]))) == 0)
    return -1;

  swprintf(keypath, sizeof(keypath) / sizeof(keypath[0]),
           L"Software\\Classes\\AppUserModelId\\%ls", MUSLIMTIFY_AUMID);
  rc = RegCreateKeyExW(HKEY_CURRENT_USER, keypath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                       &hk, NULL);
  if (rc != ERROR_SUCCESS)
    return -1;

  RegSetValueExW(hk, L"CustomActivator", 0, REG_SZ, (const BYTE *)clsid_str,
                 (DWORD)((wcslen(clsid_str) + 1) * sizeof(wchar_t)));
  RegSetValueExW(hk, L"DisplayName", 0, REG_SZ, (const BYTE *)display_name,
                 (DWORD)sizeof(display_name));
  RegCloseKey(hk);
  return 0;
}

/* Register the class object in the running process, so a toast click is
   delivered here on an RPC thread while notify_adhan blocks. COM must already
   be initialized on this thread (RoInitialize in notify_init_once). */
int toast_activator_register_running(unsigned long *cookie) {
  DWORD c = 0;
  HRESULT hr;
  if (cookie)
    *cookie = 0;
  hr = CoRegisterClassObject(&CLSID_MuslimtifyToastActivator, (IUnknown *)&g_cf,
                             CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c);
  if (FAILED(hr)) {
    dbg("[muslimtify] running CoRegisterClassObject failed\n");
    return -1;
  }
  if (cookie)
    *cookie = (unsigned long)c;
  return 0;
}

void toast_activator_revoke_running(unsigned long cookie) {
  if (cookie)
    CoRevokeClassObject((DWORD)cookie);
}

int register_toast_activator(void) {
  wchar_t exe[MAX_PATH];
  DWORD len = GetModuleFileNameW(NULL, exe, MAX_PATH);
  if (len == 0 || len >= MAX_PATH)
    return -1;

  if (register_clsid_localserver(exe) != 0)
    return -1;
  if (register_aumid() != 0)
    return -1;
  return 0;
}

int unregister_toast_activator(void) {
  wchar_t clsid_str[64];
  wchar_t keypath[320];

  if (StringFromGUID2(&CLSID_MuslimtifyToastActivator, clsid_str,
                      (int)(sizeof(clsid_str) / sizeof(clsid_str[0]))) != 0) {
    swprintf(keypath, sizeof(keypath) / sizeof(keypath[0]), L"Software\\Classes\\CLSID\\%ls",
             clsid_str);
    RegDeleteTreeW(HKEY_CURRENT_USER, keypath);
  }
  swprintf(keypath, sizeof(keypath) / sizeof(keypath[0]),
           L"Software\\Classes\\AppUserModelId\\%ls", MUSLIMTIFY_AUMID);
  RegDeleteTreeW(HKEY_CURRENT_USER, keypath);
  return 0;
}
