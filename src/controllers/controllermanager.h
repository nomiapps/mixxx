#pragma once

#include <QMap>
#include <QMutex>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <memory>
#include <vector>

#include "controllers/controllerenumerator.h"
#include "preferences/usersettings.h"
#include "util/duration.h"

// Forward declaration(s)
class Controller;
class ControllerHotplugWatcher;
class ControllerLearningEventFilter;
class MappingInfoEnumerator;
class LegacyControllerMapping;
class ControllerEnumerator;

/// Function to sort controllers by name
bool controllerCompare(Controller *a, Controller *b);

/// Manages enumeration/operation/deletion of hardware controllers.
class ControllerManager : public QObject {
    Q_OBJECT
  public:
    explicit ControllerManager(UserSettingsPointer pConfig);
    ~ControllerManager() override;

    static const mixxx::Duration kPollInterval;

    QList<Controller*> getControllers() const;
    QList<Controller*> getControllerList(bool outputDevices=true, bool inputDevices=true);
    ControllerLearningEventFilter* getControllerLearningEventFilter() const;
    QSharedPointer<MappingInfoEnumerator> getMainThreadUserMappingEnumerator() const {
        return m_pMainThreadUserMappingEnumerator;
    }
    QSharedPointer<MappingInfoEnumerator> getMainThreadSystemMappingEnumerator() const {
        return m_pMainThreadSystemMappingEnumerator;
    }
    QString getConfiguredMappingFileForDevice(const QString& name) const;

    /// Prevent other parts of Mixxx from having to manually connect to our slots.
    /// Requests made while one is already waiting are served by that one scan.
    void setUpDevices();

    static QList<QString> getMappingPaths(UserSettingsPointer pConfig);

  signals:
    void initialized();
    void devicesChanged();
    /// A scan for controllers has begun, and has finished with every enabled
    /// controller reopened. Emitted for every scan, whether or not it changed
    /// the list of devices.
    void scanStarted();
    void scanFinished();
    void requestSetUpDevices();
    void requestShutdown();
    void requestInitialize();
    void mappingApplied(bool applied);
    /// Controllers plugged in while Mixxx runs. `opened`: rescanned and running
    /// their mapping. `notOpened`: seen, but not enabled or without a mapping,
    /// or, when `rescanned` is false, left alone because a controller already
    /// in use would have been reopened by the rescan.
    void controllersPluggedIn(const QStringList& opened,
            const QStringList& notOpened,
            bool rescanned);

  public slots:
    void slotApplyMapping(Controller* pController,
            std::shared_ptr<LegacyControllerMapping> pMapping,
            bool bEnabled);

  private slots:
    /// Perform initialization that should be delayed until the ControllerManager
    /// thread is started.
    void slotInitialize();
    /// Open whatever controllers are selected in the preferences. This currently
    /// only runs on start-up but maybe should instead be signaled by the
    /// preferences dialog on apply, and only open/close changed devices
    void slotSetUpDevices();
    void slotShutdown();
    /// Calls poll() on all devices that have isPolling() true.
    void slotPollDevices();
    /// The OS reported a device change: start (or restart) the settle timer.
    void slotHotplugEvent();
    /// Compare what is plugged in now with the last scan, and rescan if a
    /// controller arrived.
    void slotCheckForNewDevices();
    /// Deletes the controllers replaced by the scans up to the given one.
    void slotDeleteRetiredControllers(quint64 scan);

  private:
    void updateControllerList();
    /// Has the controllers a scan replaced deleted once the main thread has
    /// handled the devicesChanged() of that scan.
    void deleteRetiredControllersAfterMainThread(QList<Controller*> retired);
    /// Union of every enumerator's presentDevices().
    QMap<QString, QString> presentDevices() const;
    void startPolling();
    void stopPolling();
    void pollIfAnyControllersOpen();
    void openController(Controller* pController);
    void closeController(Controller* pController);

    UserSettingsPointer m_pConfig;
    // WARNING: Do not parent m_pControllerLearningEventFilter to ControllerManager
    // because the CM is moved to its own thread and runs its own event loop.
    std::unique_ptr<ControllerLearningEventFilter> m_pControllerLearningEventFilter;
    QTimer m_pollTimer;
    mutable QMutex m_mutex;
    /// Guarded by m_mutex for main-thread reads.
    /// Written/iterated only on the ControllerManager thread
    std::vector<std::unique_ptr<ControllerEnumerator>> m_enumerators;
    /// Non-owning; Controllers are owned by their respective ControllerEnumerator.
    /// Guarded by m_mutex for main-thread reads.
    /// Written only on the ControllerManager thread
    QList<Controller*> m_controllers;
    /// The single shared background thread that drives the entire ControllerManager,
    /// all ControllerEnumerators, and all Controller instances.
    std::unique_ptr<QThread> m_pThread;
    /// Written once on the ControllerManager thread during slotInitialize(),
    /// before initialized() is emitted. Afterwards only read from the main thread
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadUserMappingEnumerator;
    QSharedPointer<MappingInfoEnumerator> m_pMainThreadSystemMappingEnumerator;
    /// Accessed only from the ControllerManager thread via slotPollDevices().
    bool m_skipPoll;

    // Hotplug. All of it lives on the ControllerManager thread.
    std::unique_ptr<ControllerHotplugWatcher> m_pHotplugWatcher;
    /// Collects a burst of OS device events (one USB device raises several) into
    /// one check, and retries while the device is still settling.
    QTimer m_hotplugTimer;
    int m_hotplugChecksLeft;
    /// False until the first slotSetUpDevices(): before it there is no baseline,
    /// and startup opens every controller anyway.
    bool m_hotplugArmed;
    /// presentDevices() as of the last scan, and the ids from it seen missing
    /// since. A device back after being missing was replugged: its Controller
    /// is dead and must be rebuilt.
    QMap<QString, QString> m_presentAtLastScan;
    QSet<QString> m_absentSinceLastScan;

    /// True from setUpDevices() until slotSetUpDevices() starts on it.
    std::atomic<bool> m_setUpRequested;
    /// The controllers each scan replaced, closed and waiting to be deleted.
    /// Only touched on the ControllerManager thread.
    struct RetiredControllers {
        quint64 scan;
        QList<Controller*> controllers;
    };
    std::vector<RetiredControllers> m_retiredControllers;
    quint64 m_scanCount;
};
