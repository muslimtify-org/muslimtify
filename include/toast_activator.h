#ifndef TOAST_ACTIVATOR_H
#define TOAST_ACTIVATOR_H

/*
 * Windows toast-activation COM server.
 *
 * Unpackaged Win32 apps only receive toast button/dismiss activations if they
 * register a ToastActivatorCLSID (a COM LocalServer32) and associate it with the
 * app's AUMID. We use the shortcut-free registry path: an
 * HKCU\Software\Classes\AppUserModelId\<AUMID> key with a CustomActivator value.
 * While the app is running, notify_adhan calls toast_activator_register_running
 * so a click is delivered in-process (RPC thread) to Activate(), which calls
 * notify_adhan_stop(). When closed, Windows launches this exe with "-Embedding"
 * and run_toast_activator_server() serves the same callback.
 *
 * All symbols are Windows-only.
 */

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Run the COM local-server that receives a single toast activation, then exit.
 * Call this when the process was launched by COM with "-Embedding".
 * Returns 0 on success (activation handled or timed out cleanly), -1 on error.
 */
int run_toast_activator_server(void);

/*
 * Register the ToastActivatorCLSID (HKCU LocalServer32) and the AUMID ->
 * CustomActivator registry association so Windows routes toast clicks to this
 * exe. No Start-Menu shortcut. Idempotent. Returns 0 on success, -1 on failure.
 */
int register_toast_activator(void);

/*
 * Remove the CLSID and AppUserModelId registry registration. Returns 0.
 */
int unregister_toast_activator(void);

/*
 * Register / revoke the activation class object in the CURRENT running process,
 * so a toast click is delivered here (RPC thread) instead of launching a fresh
 * -Embedding instance. Requires COM already initialized on this thread.
 * *cookie receives the token to pass to revoke. Returns 0 on success.
 */
int toast_activator_register_running(unsigned long *cookie);
void toast_activator_revoke_running(unsigned long cookie);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* TOAST_ACTIVATOR_H */
