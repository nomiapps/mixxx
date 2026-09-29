#pragma once

#include <QObject>

/// Tells the ControllerManager that the OS saw a device arrive or leave, so it
/// can look for controllers plugged in while Mixxx runs. It only says that
/// something changed; ControllerManager decides whether that something is a
/// controller.
///
/// Windows only for now: a message-only window registered for device interface
/// notifications. It must be created, and destroyed, on a thread with a running
/// Qt event loop (the ControllerManager thread): Qt's Windows event dispatcher
/// is what delivers WM_DEVICECHANGE to it. Elsewhere it never fires, and
/// controllers are picked up by an explicit rescan as before.
class ControllerHotplugWatcher : public QObject {
    Q_OBJECT
  public:
    explicit ControllerHotplugWatcher(QObject* pParent = nullptr);
    ~ControllerHotplugWatcher() override;

    /// False when the OS gave no way to listen, or on a platform without one.
    bool isActive() const {
        return m_active;
    }

  signals:
    void devicesChanged();

  private:
    void emitDevicesChanged() {
        emit devicesChanged();
    }

    bool m_active;
#ifdef Q_OS_WIN
    void* m_pWindow;
    void* m_pNotification;

    friend struct ControllerHotplugWatcherWindowProc;
#endif
};
