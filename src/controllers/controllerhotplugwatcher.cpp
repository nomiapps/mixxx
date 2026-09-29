#include "controllers/controllerhotplugwatcher.h"

#include <QtDebug>

#include "moc_controllerhotplugwatcher.cpp"

#ifdef Q_OS_WIN
#include <windows.h>
// After windows.h
#include <dbt.h>

namespace {

const wchar_t kWindowClassName[] = L"MixxxControllerHotplugWatcher";

} // namespace

struct ControllerHotplugWatcherWindowProc {
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_DEVICECHANGE &&
                (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)) {
            auto* pWatcher = reinterpret_cast<ControllerHotplugWatcher*>(
                    GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (pWatcher) {
                pWatcher->emitDevicesChanged();
            }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
};

ControllerHotplugWatcher::ControllerHotplugWatcher(QObject* pParent)
        : QObject(pParent),
          m_active(false),
          m_pWindow(nullptr),
          m_pNotification(nullptr) {
    const HINSTANCE hInstance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &ControllerHotplugWatcherWindowProc::windowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        qWarning() << "ControllerHotplugWatcher: RegisterClassEx failed" << GetLastError();
        return;
    }
    // A message-only window: never shown, receives only what is sent to it.
    HWND hWnd = CreateWindowExW(0,
            kWindowClassName,
            L"",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            hInstance,
            nullptr);
    if (!hWnd) {
        qWarning() << "ControllerHotplugWatcher: CreateWindowEx failed" << GetLastError();
        return;
    }
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_pWindow = hWnd;

    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    m_pNotification = RegisterDeviceNotificationW(hWnd,
            &filter,
            DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!m_pNotification) {
        qWarning() << "ControllerHotplugWatcher: RegisterDeviceNotification failed"
                   << GetLastError();
        return;
    }
    m_active = true;
}

ControllerHotplugWatcher::~ControllerHotplugWatcher() {
    if (m_pNotification) {
        UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(m_pNotification));
    }
    if (m_pWindow) {
        HWND hWnd = static_cast<HWND>(m_pWindow);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        DestroyWindow(hWnd);
    }
}

#else

ControllerHotplugWatcher::ControllerHotplugWatcher(QObject* pParent)
        : QObject(pParent),
          m_active(false) {
}

ControllerHotplugWatcher::~ControllerHotplugWatcher() = default;

#endif
