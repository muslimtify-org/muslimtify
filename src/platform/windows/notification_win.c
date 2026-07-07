#define UNICODE
#define _UNICODE
#define COBJMACROS

#include "audio.h"
#include "notification.h"
#include "toast_activator.h"
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

/* EventRegistrationToken (from eventtoken.h) */
typedef struct EventRegistrationToken {
  __int64 value;
} EventRegistrationToken;

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
  HRESULT(STDMETHODCALLTYPE *add_Dismissed)
  (IToastNotification *This, void *handler, EventRegistrationToken *token);
  void *remove_Dismissed;
  HRESULT(STDMETHODCALLTYPE *add_Activated)
  (IToastNotification *This, void *handler, EventRegistrationToken *token);
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

/* SetCurrentProcessExplicitAppUserModelID (shell32) -- declared manually to
   avoid including shell headers that clash with our hand-rolled WinRT types. */
DECLSPEC_IMPORT HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(PCWSTR AppID);

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

/* Convert UTF-16 to UTF-8. Caller must free() the result. */
static char *utf16_to_utf8(const wchar_t *wide) {
  int len;
  char *utf8;

  if (!wide)
    return NULL;
  len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
  if (len <= 0)
    return NULL;
  utf8 = (char *)malloc((size_t)len);
  if (!utf8)
    return NULL;
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, len, NULL, NULL);
  return utf8;
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

static BOOL resolve_adhan_path_from_base(const wchar_t *base_dir, wchar_t *buffer,
                                         size_t buffer_size) {
  static const wchar_t *const candidates[] = {
      L"..\\share\\muslimtify\\adhan.mp3",
      L"..\\assets\\adhan.mp3",
      L"assets\\adhan.mp3",
      L"..\\..\\share\\muslimtify\\adhan.mp3",
      L"..\\..\\assets\\adhan.mp3",
      L"..\\..\\..\\assets\\adhan.mp3",
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

  buffer[0] = L'\0';
  return FALSE;
}

static BOOL resolve_adhan_path(wchar_t *buffer, size_t buffer_size) {
  wchar_t base_dir[WINDOWS_PATH_MAX];

  if (!get_executable_dir(base_dir, sizeof(base_dir) / sizeof(base_dir[0])))
    return FALSE;

  return resolve_adhan_path_from_base(base_dir, buffer, buffer_size);
}

static wchar_t *build_toast_xml(const wchar_t *wtitle, const wchar_t *wmsg, const wchar_t *wicon,
                                const char *urgency, const char *sound_preset,
                                BOOL with_stop_action);

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

  xml = build_toast_xml(escaped_title, escaped_message, wicon, urgency, "default", FALSE);

fail:
  free(escaped_title);
  free(escaped_message);
  free(wicon);
  return xml;
}

wchar_t *notification_win_build_adhan_xml_for_test(const wchar_t *wtitle, const wchar_t *wmsg) {
  wchar_t *escaped_title = NULL;
  wchar_t *escaped_message = NULL;
  wchar_t *xml = NULL;

  if (!wtitle || !wmsg)
    return NULL;

  escaped_title = xml_escape(wtitle);
  escaped_message = xml_escape(wmsg);
  if (!escaped_title || !escaped_message)
    goto done;

  /* NULL preset => <audio silent="true"/>; TRUE => Stop action. Matches notify_adhan. */
  xml = build_toast_xml(escaped_title, escaped_message, NULL, "critical", NULL, TRUE);

done:
  free(escaped_title);
  free(escaped_message);
  return xml;
}

BOOL notification_win_resolve_adhan_path_for_test(const wchar_t *base_dir, wchar_t *buffer,
                                                  size_t buffer_size) {
  return resolve_adhan_path_from_base(base_dir, buffer, buffer_size);
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
                                const char *urgency, const char *sound_preset,
                                BOOL with_stop_action) {
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
  if (with_stop_action) {
    if (!append_wide_segment(&xml, &xml_len, &xml_cap,
                             L"<actions><action content=\"Stop\" arguments=\"stop\" "
                             L"activationType=\"foreground\"/></actions>")) {
      goto fail;
    }
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

/* Build a toast object from pre-built XML. Returns a live IToastNotification*
   the caller must Release, or NULL on failure. Does NOT show it. */
static IToastNotification *create_toast_from_xml(const wchar_t *xml) {
  HSTRING_HEADER hsh_xml_cls;
  HSTRING hs_xml_cls = NULL;
  IInspectable *inspectable = NULL;
  IXmlDocument *xml_doc = NULL;
  IXmlDocumentIO *xml_io = NULL;
  IToastNotification *toast = NULL;
  HSTRING_HEADER hsh_xml;
  HSTRING hs_xml = NULL;
  HRESULT hr;

  if (!SUCCEEDED(make_hstring_ref(RuntimeClass_XmlDocument, &hsh_xml_cls, &hs_xml_cls)))
    return NULL;
  if (!SUCCEEDED(RoActivateInstance(hs_xml_cls, &inspectable))) {
    fprintf(stderr, "muslimtify: failed to create XmlDocument\n");
    return NULL;
  }

  if (!SUCCEEDED(
          inspectable->lpVtbl->QueryInterface(inspectable, &IID_IXmlDocument, (void **)&xml_doc))) {
    inspectable->lpVtbl->Release(inspectable);
    return NULL;
  }
  inspectable->lpVtbl->Release(inspectable);

  if (!SUCCEEDED(xml_doc->lpVtbl->QueryInterface(xml_doc, &IID_IXmlDocumentIO, (void **)&xml_io))) {
    xml_doc->lpVtbl->Release(xml_doc);
    return NULL;
  }

  if (!SUCCEEDED(WindowsCreateStringReference(xml, (UINT32)wcslen(xml), &hsh_xml, &hs_xml))) {
    xml_io->lpVtbl->Release(xml_io);
    xml_doc->lpVtbl->Release(xml_doc);
    return NULL;
  }
  hr = xml_io->lpVtbl->LoadXml(xml_io, hs_xml);
  xml_io->lpVtbl->Release(xml_io);
  if (!SUCCEEDED(hr)) {
    fprintf(stderr, "muslimtify: LoadXml failed\n");
    xml_doc->lpVtbl->Release(xml_doc);
    return NULL;
  }

  if (!SUCCEEDED(
          g_state.factory->lpVtbl->CreateToastNotification(g_state.factory, xml_doc, &toast))) {
    fprintf(stderr, "muslimtify: CreateToastNotification failed\n");
    xml_doc->lpVtbl->Release(xml_doc);
    return NULL;
  }
  xml_doc->lpVtbl->Release(xml_doc);
  return toast;
}

/* Send a pre-built toast XML wide string through the WinRT pipeline */
static void send_toast_xml(const wchar_t *xml) {
  IToastNotification *toast = create_toast_from_xml(xml);
  HRESULT hr;

  if (!toast)
    return;

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
  wchar_t *xml = build_toast_xml(wtitle, wmsg, wicon, urgency, sound_preset, FALSE);
  free(wtitle);
  free(wmsg);
  free(wicon);

  if (!xml)
    return;

  send_toast_xml(xml);
  free(xml);
}

/* -- Adhan stop signal ------------------------------------------------------ */
/* Toast buttons cannot deliver a callback to an unpackaged Win32 app without a
   registered COM activator (ToastActivatorCLSID) + AUMID shortcut, so we do not
   rely on toast interaction to stop the adhan. Instead notify_adhan creates a
   named manual-reset event while the adhan plays and waits on it; a separate
   process (`muslimtify sound stop`) opens the event by name and signals it via
   notify_adhan_stop(). The event existing also means "an adhan is playing".
   Session-local namespace is correct: the interactive scheduled task and the
   user's shell share one logon session. */
static const wchar_t ADHAN_STOP_EVENT_NAME[] = L"Local\\MuslimtifyAdhanStop";
static HANDLE g_adhan_stop_event = NULL;

/* -- API implementation ----------------------------------------------------- */

int notify_init_once(const char *app_name) {
  (void)app_name; /* AUMID is used instead on Windows */

  if (g_state.initialized)
    return 1;

  if (!SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED))) {
    fprintf(stderr, "muslimtify: RoInitialize failed\n");
    return 0;
  }

  /* Attribute this process to our AUMID so Windows maps toast activations to the
     registered AppUserModelId / ToastActivatorCLSID. */
  SetCurrentProcessExplicitAppUserModelID(MUSLIMTIFY_AUMID);

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

void notify_adhan(const char *prayer_name, const char *time_str, const char *path) {
  if (!g_state.initialized)
    return;

  /* Resolve the audio file: caller path, else bundled adhan.mp3. */
  char *resolved_utf8 = NULL;
  const char *adhan_path = (path && path[0] != '\0') ? path : NULL;
  if (!adhan_path) {
    wchar_t wresolved[WINDOWS_PATH_MAX];
    if (resolve_adhan_path(wresolved, sizeof(wresolved) / sizeof(wresolved[0]))) {
      resolved_utf8 = utf16_to_utf8(wresolved);
      adhan_path = resolved_utf8;
    }
  }

  /* Build the toast: title/message, silent audio, no Stop button (unpackaged
     toasts can't route a button click back — stop is via notify_adhan_stop). */
  char title[128];
  char message[256];
  _snprintf_s(title, sizeof(title), _TRUNCATE, "Prayer Time: %s", prayer_name);
  _snprintf_s(message, sizeof(message), _TRUNCATE, "It's time for %s prayer\nTime: %s", prayer_name,
              time_str);

  wchar_t *wtitle_raw = utf8_to_utf16(title);
  wchar_t *wmsg_raw = utf8_to_utf16(message);
  wchar_t *wtitle = xml_escape(wtitle_raw);
  wchar_t *wmsg = xml_escape(wmsg_raw);
  free(wtitle_raw);
  free(wmsg_raw);

  IToastNotification *toast = NULL;
  if (wtitle && wmsg) {
    wchar_t icon_path[WINDOWS_PATH_MAX];
    wchar_t *wicon = NULL;
    if (resolve_toast_icon_path(icon_path, sizeof(icon_path) / sizeof(icon_path[0])))
      wicon = xml_escape(icon_path);

    wchar_t *xml = build_toast_xml(wtitle, wmsg, wicon, "critical", NULL, TRUE);
    free(wicon);
    if (xml) {
      toast = create_toast_from_xml(xml);
      free(xml);
    }
  }
  free(wtitle);
  free(wmsg);

  /* Create the named stop event: its existence marks "an adhan is playing", and
     `muslimtify sound stop` signals it via notify_adhan_stop(). */
  if (g_adhan_stop_event)
    CloseHandle(g_adhan_stop_event);
  g_adhan_stop_event = CreateEventW(NULL, TRUE, FALSE, ADHAN_STOP_EVENT_NAME); /* manual-reset */

  /* Serve toast activations in this running process so a Stop click routes here
     on an RPC thread (registry-AUMID path) and signals the event below. */
  unsigned long activator_cookie = 0;
  toast_activator_register_running(&activator_cookie);

  if (toast)
    g_state.notifier->lpVtbl->Show(g_state.notifier, toast);

  /* Play + block until the adhan ends, the Stop button, or `sound stop`. */
  if (adhan_path && audio_start(adhan_path) == 0) {
    while (audio_is_playing()) {
      if (g_adhan_stop_event && WaitForSingleObject(g_adhan_stop_event, 100) == WAIT_OBJECT_0)
        break;
      if (!g_adhan_stop_event)
        Sleep(100);
    }
    audio_stop();
  }

  toast_activator_revoke_running(activator_cookie);

  /* Close the event so it no longer advertises a playing adhan. */
  if (g_adhan_stop_event) {
    CloseHandle(g_adhan_stop_event);
    g_adhan_stop_event = NULL;
  }

  if (toast)
    toast->lpVtbl->Release(toast);
  free(resolved_utf8);
}

/* Signal an in-progress adhan (started by notify_adhan, possibly in another
   process such as the scheduled-task daemon) to stop. Returns 0 if a running
   adhan was signaled, -1 if none is playing. */
int notify_adhan_stop(void) {
  HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, ADHAN_STOP_EVENT_NAME);
  if (!ev)
    return -1; /* event absent => no adhan currently playing */
  SetEvent(ev);
  CloseHandle(ev);
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
