#define UNICODE
#define _UNICODE
#define COBJMACROS

#include "notification.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

/* -- WinRT type declarations ------------------------------------------------ */

/* HSTRING types (from winstring.h) */
typedef struct HSTRING__ {
  int unused;
} HSTRING__;
typedef HSTRING__ *HSTRING;
typedef struct HSTRING_HEADER {
  union {
    void *Reserved1;
#if defined(_WIN64)
    char Reserved2[24];
#else
    char Reserved2[20];
#endif
  } Reserved;
} HSTRING_HEADER;

STDAPI WindowsCreateStringReference(PCWSTR sourceString, UINT32 length,
                                    HSTRING_HEADER *hstringHeader, HSTRING *string);
STDAPI WindowsDeleteString(HSTRING string);

/* IInspectable (from inspectable.h) */
typedef interface IInspectable IInspectable;
typedef enum { BaseTrust = 0, PartialTrust = 1, FullTrust = 2 } TrustLevel;
typedef struct IInspectableVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IInspectable *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IInspectable *This);
  ULONG(STDMETHODCALLTYPE *Release)(IInspectable *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IInspectable *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IInspectable *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IInspectable *This, TrustLevel *trustLevel);
  END_INTERFACE
} IInspectableVtbl;
interface IInspectable {
  CONST_VTBL struct IInspectableVtbl *lpVtbl;
};

/* Forward declarations for vtable references */
typedef interface IXmlDocument IXmlDocument;
typedef interface IXmlDocumentIO IXmlDocumentIO;
typedef interface IToastNotification IToastNotification;
typedef interface IToastNotifier IToastNotifier;
typedef interface IToastNotificationFactory IToastNotificationFactory;
typedef interface IToastNotificationManagerStatics IToastNotificationManagerStatics;

/* IXmlDocument */
typedef struct IXmlDocumentVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IXmlDocument *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IXmlDocument *This);
  ULONG(STDMETHODCALLTYPE *Release)(IXmlDocument *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IXmlDocument *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IXmlDocument *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IXmlDocument *This, TrustLevel *trustLevel);
  /* IXmlDocument methods — we only need it as an opaque handle passed to
     CreateToastNotification, so the remaining vtable slots are void* padding */
  void *get_Doctype;
  void *get_Implementation;
  void *get_DocumentElement;
  void *CreateElement;
  void *CreateDocumentFragment;
  void *CreateTextNode;
  void *CreateComment;
  void *CreateProcessingInstruction;
  void *CreateAttribute;
  void *CreateEntityReference;
  void *GetElementsByTagName;
  void *CreateCDataSection;
  void *get_DocumentUri;
  void *CreateAttributeNS;
  void *CreateElementNS;
  void *GetElementById;
  void *ImportNode;
  END_INTERFACE
} IXmlDocumentVtbl;
interface IXmlDocument {
  CONST_VTBL struct IXmlDocumentVtbl *lpVtbl;
};

/* IXmlDocumentIO */
typedef struct IXmlDocumentIOVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IXmlDocumentIO *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IXmlDocumentIO *This);
  ULONG(STDMETHODCALLTYPE *Release)(IXmlDocumentIO *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IXmlDocumentIO *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IXmlDocumentIO *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IXmlDocumentIO *This, TrustLevel *trustLevel);
  HRESULT(STDMETHODCALLTYPE *LoadXml)(IXmlDocumentIO *This, HSTRING xml);
  void *LoadXmlWithSettings;
  void *SaveToFileAsync;
  END_INTERFACE
} IXmlDocumentIOVtbl;
interface IXmlDocumentIO {
  CONST_VTBL struct IXmlDocumentIOVtbl *lpVtbl;
};

/* IToastNotification */
typedef struct IToastNotificationVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IToastNotification *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IToastNotification *This);
  ULONG(STDMETHODCALLTYPE *Release)(IToastNotification *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IToastNotification *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IToastNotification *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IToastNotification *This, TrustLevel *trustLevel);
  void *get_Content;
  void *put_ExpirationTime;
  void *get_ExpirationTime;
  void *add_Dismissed;
  void *remove_Dismissed;
  void *add_Activated;
  void *remove_Activated;
  void *add_Failed;
  void *remove_Failed;
  END_INTERFACE
} IToastNotificationVtbl;
interface IToastNotification {
  CONST_VTBL struct IToastNotificationVtbl *lpVtbl;
};

/* IToastNotifier */
typedef struct IToastNotifierVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IToastNotifier *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IToastNotifier *This);
  ULONG(STDMETHODCALLTYPE *Release)(IToastNotifier *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IToastNotifier *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IToastNotifier *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IToastNotifier *This, TrustLevel *trustLevel);
  HRESULT(STDMETHODCALLTYPE *Show)
  (IToastNotifier *This, IToastNotification *notification);
  void *Hide;
  void *get_Setting;
  void *AddToSchedule;
  void *RemoveFromSchedule;
  void *GetScheduledToastNotifications;
  END_INTERFACE
} IToastNotifierVtbl;
interface IToastNotifier {
  CONST_VTBL struct IToastNotifierVtbl *lpVtbl;
};

/* IToastNotificationFactory */
typedef struct IToastNotificationFactoryVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IToastNotificationFactory *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IToastNotificationFactory *This);
  ULONG(STDMETHODCALLTYPE *Release)(IToastNotificationFactory *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IToastNotificationFactory *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IToastNotificationFactory *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IToastNotificationFactory *This, TrustLevel *trustLevel);
  HRESULT(STDMETHODCALLTYPE *CreateToastNotification)
  (IToastNotificationFactory *This, IXmlDocument *content, IToastNotification **notification);
  END_INTERFACE
} IToastNotificationFactoryVtbl;
interface IToastNotificationFactory {
  CONST_VTBL struct IToastNotificationFactoryVtbl *lpVtbl;
};

/* IToastNotificationManagerStatics */
typedef struct IToastNotificationManagerStaticsVtbl {
  BEGIN_INTERFACE
  HRESULT(STDMETHODCALLTYPE *QueryInterface)
  (IToastNotificationManagerStatics *This, REFIID riid, void **ppvObject);
  ULONG(STDMETHODCALLTYPE *AddRef)(IToastNotificationManagerStatics *This);
  ULONG(STDMETHODCALLTYPE *Release)(IToastNotificationManagerStatics *This);
  HRESULT(STDMETHODCALLTYPE *GetIids)
  (IToastNotificationManagerStatics *This, ULONG *iidCount, IID **iids);
  HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)
  (IToastNotificationManagerStatics *This, HSTRING *className);
  HRESULT(STDMETHODCALLTYPE *GetTrustLevel)
  (IToastNotificationManagerStatics *This, TrustLevel *trustLevel);
  HRESULT(STDMETHODCALLTYPE *CreateToastNotifier)
  (IToastNotificationManagerStatics *This, IToastNotifier **notifier);
  HRESULT(STDMETHODCALLTYPE *CreateToastNotifierWithId)
  (IToastNotificationManagerStatics *This, HSTRING applicationId, IToastNotifier **notifier);
  void *GetTemplateContent;
  END_INTERFACE
} IToastNotificationManagerStaticsVtbl;
interface IToastNotificationManagerStatics {
  CONST_VTBL struct IToastNotificationManagerStaticsVtbl *lpVtbl;
};

/* -- GUIDs ------------------------------------------------------------------ */

static const IID IID_IToastNotificationManagerStatics = {
    0x50ac103f, 0xd235, 0x4598, {0xbb, 0xef, 0x98, 0xfe, 0x4d, 0x1a, 0x3a, 0xd4}};
static const IID IID_IToastNotificationFactory = {
    0x04124b20, 0x82c6, 0x4229, {0xb1, 0x09, 0xfd, 0x9e, 0xd4, 0x66, 0x2b, 0x53}};
static const IID IID_IXmlDocument = {
    0xf7f3a506, 0x1e87, 0x42d6, {0xbc, 0xfb, 0xb8, 0xc8, 0x09, 0xfa, 0x54, 0x94}};
static const IID IID_IXmlDocumentIO = {
    0x6cd0e74e, 0xee65, 0x4489, {0x9e, 0xbf, 0xca, 0x43, 0xe8, 0x7b, 0xa6, 0x37}};

/* -- Runtime class names ---------------------------------------------------- */

static const WCHAR RuntimeClass_ToastNotificationManager[] =
    L"Windows.UI.Notifications.ToastNotificationManager";
static const WCHAR RuntimeClass_ToastNotification[] = L"Windows.UI.Notifications.ToastNotification";
static const WCHAR RuntimeClass_XmlDocument[] = L"Windows.Data.Xml.Dom.XmlDocument";

/* -- RoAPI declarations ----------------------------------------------------- */

#ifndef ROAPI
#ifdef _ROAPI_
#define ROAPI
#else
#define ROAPI DECLSPEC_IMPORT
#endif
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;
ROAPI HRESULT WINAPI RoInitialize(RO_INIT_TYPE initType);
ROAPI HRESULT WINAPI RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void **factory);
ROAPI HRESULT WINAPI RoActivateInstance(HSTRING activatableClassId, IInspectable **instance);
ROAPI void WINAPI RoUninitialize(void);
#endif

/* -- AUMID for unpackaged app ----------------------------------------------- */

static const WCHAR MUSLIMTIFY_AUMID[] = L"Muslimtify";

/* -- File-static state ------------------------------------------------------ */

typedef struct {
  IToastNotificationFactory *factory;
  IToastNotifier *notifier;
  BOOL initialized;
} NotifyState;

static NotifyState g_state = {0};

#define WINDOWS_PATH_MAX 32768

/* -- Helpers ---------------------------------------------------------------- */

/* Convert UTF-8 string to UTF-16. Caller must free() the result. */
static wchar_t *utf8_to_utf16(const char *utf8) {
  if (!utf8)
    return NULL;
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (len <= 0)
    return NULL;
  wchar_t *wide = (wchar_t *)malloc(len * sizeof(wchar_t));
  if (!wide)
    return NULL;
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
  return wide;
}

/* XML-escape a UTF-16 string. Caller must free() the result. */
static wchar_t *xml_escape(const wchar_t *src) {
  if (!src)
    return NULL;
  /* Worst case: every char becomes "&quot;" or "&apos;" (6x expansion) */
  size_t src_len = wcslen(src);
  size_t max_len = src_len * 6 + 1;
  wchar_t *escaped = (wchar_t *)malloc(max_len * sizeof(wchar_t));
  if (!escaped)
    return NULL;
  wchar_t *dst = escaped;
  size_t remaining = max_len;

  for (size_t i = 0; i < src_len; i++) {
    const wchar_t *repl = NULL;
    size_t repl_len = 0;
    switch (src[i]) {
    case L'<':
      repl = L"&lt;";
      repl_len = 4;
      break;
    case L'>':
      repl = L"&gt;";
      repl_len = 4;
      break;
    case L'&':
      repl = L"&amp;";
      repl_len = 5;
      break;
    case L'"':
      repl = L"&quot;";
      repl_len = 6;
      break;
    case L'\'':
      repl = L"&apos;";
      repl_len = 6;
      break;
    default:
      if (remaining <= 1) {
        free(escaped);
        return NULL;
      }
      *dst++ = src[i];
      remaining--;
      continue;
    }

    if (remaining <= repl_len) {
      free(escaped);
      return NULL;
    }
    wmemcpy(dst, repl, repl_len);
    dst += repl_len;
    remaining -= repl_len;
  }
  *dst = L'\0';
  return escaped;
}

/* Create HSTRING from static wide string (no allocation — reference only) */
static BOOL wide_file_exists(const wchar_t *path) {
  DWORD attrs;

  if (!path || path[0] == L'\0')
    return FALSE;

  attrs = GetFileAttributesW(path);
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static BOOL get_executable_dir(wchar_t *buffer, size_t buffer_size) {
  wchar_t exe_path[WINDOWS_PATH_MAX];
  DWORD len;
  int written;
  wchar_t *last_sep;

  if (!buffer || buffer_size == 0)
    return FALSE;

  len = GetModuleFileNameW(NULL, exe_path, WINDOWS_PATH_MAX);
  if (len == 0 || len >= WINDOWS_PATH_MAX)
    return FALSE;

  last_sep = wcsrchr(exe_path, L'\\');
  if (!last_sep)
    last_sep = wcsrchr(exe_path, L'/');
  if (!last_sep)
    return FALSE;

  *last_sep = L'\0';
  written = swprintf(buffer, buffer_size, L"%ls", exe_path);
  return written > 0 && (size_t)written < buffer_size;
}

static BOOL build_executable_relative_path(const wchar_t *base_dir, const wchar_t *relative,
                                           wchar_t *buffer, size_t buffer_size) {
  int written;

  if (!base_dir || !relative || !buffer || buffer_size == 0 || base_dir[0] == L'\0')
    return FALSE;

  written = swprintf(buffer, buffer_size, L"%ls\\%ls", base_dir, relative);
  return written > 0 && (size_t)written < buffer_size;
}

static BOOL resolve_toast_icon_path_from_base(const wchar_t *base_dir, wchar_t *buffer,
                                              size_t buffer_size) {
  static const wchar_t *const candidates[] = {
      L"..\\share\\icons\\hicolor\\128x128\\apps\\muslimtify.png",
      L"..\\share\\pixmaps\\muslimtify.png",
      L"..\\assets\\muslimtify.png",
      L"assets\\muslimtify.png",
      L"..\\..\\share\\icons\\hicolor\\128x128\\apps\\muslimtify.png",
      L"..\\..\\share\\pixmaps\\muslimtify.png",
      L"..\\..\\assets\\muslimtify.png",
      L"..\\..\\..\\share\\icons\\hicolor\\128x128\\apps\\muslimtify.png",
      L"..\\..\\..\\share\\pixmaps\\muslimtify.png",
      L"..\\..\\..\\assets\\muslimtify.png",
  };
  wchar_t candidate_path[WINDOWS_PATH_MAX];
  size_t i;

  if (!base_dir || base_dir[0] == L'\0' || !buffer || buffer_size == 0)
    return FALSE;

  buffer[0] = L'\0';

  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (!build_executable_relative_path(base_dir, candidates[i], candidate_path,
                                        sizeof(candidate_path) / sizeof(candidate_path[0]))) {
      continue;
    }
    if (wide_file_exists(candidate_path)) {
      if (swprintf(buffer, buffer_size, L"%ls", candidate_path) > 0)
        return TRUE;
      buffer[0] = L'\0';
      return FALSE;
    }
  }

  if (buffer_size > 0)
    buffer[0] = L'\0';
  return FALSE;
}

static BOOL resolve_toast_icon_path(wchar_t *buffer, size_t buffer_size) {
  wchar_t base_dir[WINDOWS_PATH_MAX];

  if (!get_executable_dir(base_dir, sizeof(base_dir) / sizeof(base_dir[0])))
    return FALSE;

  return resolve_toast_icon_path_from_base(base_dir, buffer, buffer_size);
}

static wchar_t *build_toast_xml(const wchar_t *wtitle, const wchar_t *wmsg, const wchar_t *wicon,
                                const char *urgency, const char *sound_preset);

#ifdef MUSLIMTIFY_NOTIFICATION_WIN_TEST
BOOL notification_win_resolve_toast_icon_path_for_test(const wchar_t *base_dir, wchar_t *buffer,
                                                       size_t buffer_size) {
  return resolve_toast_icon_path_from_base(base_dir, buffer, buffer_size);
}

wchar_t *notification_win_build_toast_xml_for_test(const wchar_t *base_dir, const wchar_t *wtitle,
                                                   const wchar_t *wmsg, const char *urgency) {
  wchar_t icon_path[WINDOWS_PATH_MAX];
  wchar_t *wicon = NULL;
  wchar_t *escaped_title = NULL;
  wchar_t *escaped_message = NULL;
  wchar_t *xml = NULL;

  if (!wtitle || !wmsg)
    return NULL;

  escaped_title = xml_escape(wtitle);
  escaped_message = xml_escape(wmsg);
  if (!escaped_title || !escaped_message)
    goto fail;

  if (base_dir && resolve_toast_icon_path_from_base(base_dir, icon_path,
                                                    sizeof(icon_path) / sizeof(icon_path[0]))) {
    wicon = xml_escape(icon_path);
    if (!wicon)
      goto fail;
  }

  xml = build_toast_xml(escaped_title, escaped_message, wicon, urgency, "default");

fail:
  free(escaped_title);
  free(escaped_message);
  free(wicon);
  return xml;
}
#endif

static BOOL append_wide_segment(wchar_t **buffer, size_t *len, size_t *cap,
                                const wchar_t *segment) {
  size_t add_len;
  size_t required;
  size_t new_cap;
  wchar_t *tmp;

  if (!buffer || !len || !cap || !segment)
    return FALSE;

  add_len = wcslen(segment);
  required = *len + add_len + 1;
  if (required > *cap) {
    new_cap = (*cap == 0) ? 256 : *cap;
    while (new_cap < required) {
      size_t next_cap = new_cap * 2;
      if (next_cap <= new_cap)
        return FALSE;
      new_cap = next_cap;
    }

    tmp = (wchar_t *)realloc(*buffer, new_cap * sizeof(wchar_t));
    if (!tmp)
      return FALSE;
    *buffer = tmp;
    *cap = new_cap;
  }

  wmemcpy(*buffer + *len, segment, add_len);
  *len += add_len;
  (*buffer)[*len] = L'\0';
  return TRUE;
}

static const wchar_t *urgency_to_toast_open(const char *urgency) {
  if (urgency && strcmp(urgency, "critical") == 0)
    return L"<toast scenario=\"urgent\" duration=\"long\">";
  if (urgency && strcmp(urgency, "low") == 0)
    return L"<toast scenario=\"reminder\" duration=\"long\">";
  return L"<toast duration=\"long\">";
}

// Append an <audio> element for the given preset.
// NULL preset → <audio silent="true"/>
// "alarm"     → looping alarm sound (caps at ~25s — WinRT toast duration limit)
// "reminder"  → short reminder ding
// "default"   → omit <audio> entirely so the system plays the default sound
// unknown     → treat as "default"
static BOOL append_audio_element(wchar_t **xml, size_t *len, size_t *cap,
                                 const char *sound_preset) {
  if (sound_preset == NULL) {
    return append_wide_segment(xml, len, cap, L"<audio silent=\"true\"/>");
  }
  if (strcmp(sound_preset, "alarm") == 0) {
    return append_wide_segment(
        xml, len, cap,
        L"<audio src=\"ms-winsoundevent:Notification.Looping.Alarm\" loop=\"true\"/>");
  }
  if (strcmp(sound_preset, "reminder") == 0) {
    return append_wide_segment(xml, len, cap,
                               L"<audio src=\"ms-winsoundevent:Notification.Reminder\"/>");
  }
  // "default" or unknown → no <audio> element; Windows plays its default sound
  return TRUE;
}

static wchar_t *build_toast_xml(const wchar_t *wtitle, const wchar_t *wmsg, const wchar_t *wicon,
                                const char *urgency, const char *sound_preset) {
  wchar_t *xml = NULL;
  size_t xml_len = 0;
  size_t xml_cap = 0;

  if (!append_wide_segment(&xml, &xml_len, &xml_cap, urgency_to_toast_open(urgency))) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap,
                           L"<visual><binding template=\"ToastGeneric\">")) {
    goto fail;
  }
  if (wicon) {
    if (!append_wide_segment(&xml, &xml_len, &xml_cap,
                             L"<image placement=\"appLogoOverride\" src=\"")) {
      goto fail;
    }
    if (!append_wide_segment(&xml, &xml_len, &xml_cap, wicon)) {
      goto fail;
    }
    if (!append_wide_segment(&xml, &xml_len, &xml_cap, L"\"/>")) {
      goto fail;
    }
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, L"<text>")) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, wtitle)) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, L"</text><text>")) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, wmsg)) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, L"</text></binding></visual>")) {
    goto fail;
  }
  if (!append_audio_element(&xml, &xml_len, &xml_cap, sound_preset)) {
    goto fail;
  }
  if (!append_wide_segment(&xml, &xml_len, &xml_cap, L"</toast>")) {
    goto fail;
  }

  return xml;

fail:
  free(xml);
  return NULL;
}

static HRESULT make_hstring_ref(const WCHAR *str, HSTRING_HEADER *header, HSTRING *hstr) {
  return WindowsCreateStringReference(str, (UINT32)wcslen(str), header, hstr);
}

/* Send a pre-built toast XML wide string through the WinRT pipeline */
static void send_toast_xml(const wchar_t *xml) {
  /* Create XmlDocument */
  HSTRING_HEADER hsh_xml_cls;
  HSTRING hs_xml_cls = NULL;
  IInspectable *inspectable = NULL;
  if (!SUCCEEDED(make_hstring_ref(RuntimeClass_XmlDocument, &hsh_xml_cls, &hs_xml_cls)))
    return;
  if (!SUCCEEDED(RoActivateInstance(hs_xml_cls, &inspectable))) {
    fprintf(stderr, "muslimtify: failed to create XmlDocument\n");
    return;
  }

  /* Query IXmlDocument */
  IXmlDocument *xml_doc = NULL;
  if (!SUCCEEDED(
          inspectable->lpVtbl->QueryInterface(inspectable, &IID_IXmlDocument, (void **)&xml_doc))) {
    inspectable->lpVtbl->Release(inspectable);
    return;
  }
  inspectable->lpVtbl->Release(inspectable);

  /* Query IXmlDocumentIO and load XML */
  IXmlDocumentIO *xml_io = NULL;
  if (!SUCCEEDED(xml_doc->lpVtbl->QueryInterface(xml_doc, &IID_IXmlDocumentIO, (void **)&xml_io))) {
    xml_doc->lpVtbl->Release(xml_doc);
    return;
  }

  HSTRING_HEADER hsh_xml;
  HSTRING hs_xml = NULL;
  if (!SUCCEEDED(WindowsCreateStringReference(xml, (UINT32)wcslen(xml), &hsh_xml, &hs_xml))) {
    xml_io->lpVtbl->Release(xml_io);
    xml_doc->lpVtbl->Release(xml_doc);
    return;
  }
  HRESULT hr = xml_io->lpVtbl->LoadXml(xml_io, hs_xml);
  xml_io->lpVtbl->Release(xml_io);
  if (!SUCCEEDED(hr)) {
    fprintf(stderr, "muslimtify: LoadXml failed\n");
    xml_doc->lpVtbl->Release(xml_doc);
    return;
  }

  /* Create and show toast */
  IToastNotification *toast = NULL;
  if (!SUCCEEDED(
          g_state.factory->lpVtbl->CreateToastNotification(g_state.factory, xml_doc, &toast))) {
    fprintf(stderr, "muslimtify: CreateToastNotification failed\n");
    xml_doc->lpVtbl->Release(xml_doc);
    return;
  }
  xml_doc->lpVtbl->Release(xml_doc);

  hr = g_state.notifier->lpVtbl->Show(g_state.notifier, toast);
  if (!SUCCEEDED(hr)) {
    fprintf(stderr, "muslimtify: toast Show failed\n");
  }
  toast->lpVtbl->Release(toast);
}

/* Build toast XML from title/message with optional reminder scenario, icon, and sound */
static void send_notification(const char *title, const char *message, const char *urgency,
                              const char *sound_preset) {
  if (!g_state.initialized)
    return;
  if (!title || !message)
    return;

  /* Convert and escape strings */
  wchar_t *wtitle_raw = utf8_to_utf16(title);
  wchar_t *wmsg_raw = utf8_to_utf16(message);
  wchar_t *wtitle = xml_escape(wtitle_raw);
  wchar_t *wmsg = xml_escape(wmsg_raw);
  free(wtitle_raw);
  free(wmsg_raw);
  if (!wtitle || !wmsg) {
    free(wtitle);
    free(wmsg);
    return;
  }

  wchar_t icon_path[WINDOWS_PATH_MAX];
  wchar_t *wicon = NULL;
  if (resolve_toast_icon_path(icon_path, sizeof(icon_path) / sizeof(icon_path[0]))) {
    wicon = xml_escape(icon_path);
  }

  /* Build toast XML */
  wchar_t *xml = build_toast_xml(wtitle, wmsg, wicon, urgency, sound_preset);
  free(wtitle);
  free(wmsg);
  free(wicon);

  if (!xml)
    return;

  send_toast_xml(xml);
  free(xml);
}

/* -- API implementation ----------------------------------------------------- */

int notify_init_once(const char *app_name) {
  (void)app_name; /* AUMID is used instead on Windows */

  if (g_state.initialized)
    return 1;

  if (!SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED))) {
    fprintf(stderr, "muslimtify: RoInitialize failed\n");
    return 0;
  }

  /* Get ToastNotificationManager factory */
  HSTRING_HEADER hsh_mgr;
  HSTRING hs_mgr = NULL;
  IToastNotificationManagerStatics *mgr = NULL;

  if (!SUCCEEDED(make_hstring_ref(RuntimeClass_ToastNotificationManager, &hsh_mgr, &hs_mgr))) {
    fprintf(stderr, "muslimtify: failed to create manager HSTRING\n");
    goto fail;
  }
  if (!SUCCEEDED(
          RoGetActivationFactory(hs_mgr, &IID_IToastNotificationManagerStatics, (void **)&mgr))) {
    fprintf(stderr, "muslimtify: failed to get ToastNotificationManager\n");
    goto fail;
  }

  /* Create notifier with AUMID */
  HSTRING_HEADER hsh_aumid;
  HSTRING hs_aumid = NULL;
  if (!SUCCEEDED(make_hstring_ref(MUSLIMTIFY_AUMID, &hsh_aumid, &hs_aumid))) {
    mgr->lpVtbl->Release(mgr);
    goto fail;
  }
  if (!SUCCEEDED(mgr->lpVtbl->CreateToastNotifierWithId(mgr, hs_aumid, &g_state.notifier))) {
    fprintf(stderr, "muslimtify: failed to create ToastNotifier\n");
    mgr->lpVtbl->Release(mgr);
    goto fail;
  }
  mgr->lpVtbl->Release(mgr);

  /* Get ToastNotification factory */
  HSTRING_HEADER hsh_notif;
  HSTRING hs_notif = NULL;
  if (!SUCCEEDED(make_hstring_ref(RuntimeClass_ToastNotification, &hsh_notif, &hs_notif))) {
    goto fail;
  }
  if (!SUCCEEDED(RoGetActivationFactory(hs_notif, &IID_IToastNotificationFactory,
                                        (void **)&g_state.factory))) {
    fprintf(stderr, "muslimtify: failed to get ToastNotificationFactory\n");
    goto fail;
  }

  g_state.initialized = TRUE;
  return 1;

fail:
  if (g_state.notifier) {
    g_state.notifier->lpVtbl->Release(g_state.notifier);
    g_state.notifier = NULL;
  }
  RoUninitialize();
  return 0;
}

void notify_send(const char *title, const char *message) {
  send_notification(title, message, "normal", "default");
}

void notify_prayer(const char *prayer_name, const char *time_str, int minutes_before,
                   const char *urgency_str, const char *sound_preset) {
  char title[128];
  char message[256];

  if (minutes_before == 0) {
    _snprintf_s(title, sizeof(title), _TRUNCATE, "Prayer Time: %s", prayer_name);
    _snprintf_s(message, sizeof(message), _TRUNCATE, "It's time for %s prayer\nTime: %s",
                prayer_name, time_str);
  } else {
    _snprintf_s(title, sizeof(title), _TRUNCATE, "Prayer Reminder: %s", prayer_name);
    _snprintf_s(message, sizeof(message), _TRUNCATE, "%s prayer in %d minutes\nTime: %s",
                prayer_name, minutes_before, time_str);
  }

  send_notification(title, message, urgency_str, sound_preset);
}

void notify_cleanup(void) {
  if (!g_state.initialized)
    return;

  if (g_state.factory) {
    g_state.factory->lpVtbl->Release(g_state.factory);
    g_state.factory = NULL;
  }
  if (g_state.notifier) {
    g_state.notifier->lpVtbl->Release(g_state.notifier);
    g_state.notifier = NULL;
  }

  RoUninitialize();
  g_state.initialized = FALSE;
}
