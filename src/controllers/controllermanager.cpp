#include "controllers/controllermanager.h"

#include <QCoreApplication>
#include <QPointer>
#include <QSet>
#include <QThread>
#include <algorithm>

#include "controllers/controller.h"
#include "controllers/controllerhotplugwatcher.h"
#include "controllers/controllerlearningeventfilter.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "moc_controllermanager.cpp"
#include "preferences/usersettings.h"
#include "util/cmdlineargs.h"
#include "util/compatibility/qmutex.h"
#include "util/duration.h"
#include "util/thread_affinity.h"
#include "util/time.h"

#ifdef __PORTMIDI__
#include "controllers/midi/portmidienumerator.h"
#endif

#ifdef __HSS1394__
#include "controllers/midi/hss1394enumerator.h"
#endif

#ifdef __HID__
#include "controllers/hid/hidenumerator.h"
#endif

#ifdef __BULK__
#include "controllers/bulk/bulkenumerator.h"
#endif

// http://developer.qt.nokia.com/wiki/Threads_Events_QObjects

// Poll every 1ms (where possible) for good controller response
#ifdef __LINUX__
// Many Linux distros ship with the system tick set to 250Hz so 1ms timer
// reportedly causes CPU hosage. See https://github.com/mixxxdj/mixxx/issues/6383
// rryan 6/2012
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(5);
#else
const mixxx::Duration ControllerManager::kPollInterval = mixxx::Duration::fromMillis(1);
#endif

namespace {
/// Strip slashes and spaces from device name, so that it can be used as config
/// key or a filename.
QString sanitizeDeviceName(QString name) {
    return name.replace(" ", "_").replace("/", "_").replace("\\", "_");
}

QFileInfo findMappingFile(const QString& pathOrFilename, const QStringList& paths) {
    QFileInfo fileInfo(pathOrFilename);
    if (fileInfo.isAbsolute()) {
        return fileInfo;
    }

    for (const QString& path : paths) {
        fileInfo = QFileInfo(QDir(path).absoluteFilePath(pathOrFilename));
        if (fileInfo.exists()) {
            return fileInfo;
        }
    }

    return QFileInfo();
}

// Legacy code referred to mappings as "presets", so "[ControllerPreset]" must be
// kept for backwards compatibility.
const QString kSettingsGroup = QLatin1String("[ControllerPreset]");

/// How long after the last OS device event to look at what is plugged in. A USB
/// controller raises a burst of events (its USB, audio, MIDI and HID interfaces)
/// over a second or so, and its MIDI port can show up in WinMM after the last one.
constexpr mixxx::Duration kHotplugSettleTime = mixxx::Duration::fromMillis(1500);
/// Checks per burst, kHotplugSettleTime apart, before giving up on a device
/// that has not shown up yet.
constexpr int kHotplugChecks = 3;

bool isControllerEnabled(const UserSettingsPointer& pConfig, const QString& name) {
    return pConfig->getValue(ConfigKey(QStringLiteral("[Controller]"), sanitizeDeviceName(name)), 0);
}

} // anonymous namespace

QString firstAvailableFilename(QSet<QString>& filenames,
        const QString& originalFilename) {
    QString filename = originalFilename;
    int i = 1;
    while (filenames.contains(filename)) {
        i++;
        filename = QString("%1--%2").arg(originalFilename, QString::number(i));
    }
    filenames.insert(filename);
    return filename;
}

bool controllerCompare(Controller *a,Controller *b) {
    return a->getName() < b->getName();
}

ControllerManager::ControllerManager(UserSettingsPointer pConfig)
        : QObject(),
          m_pConfig(pConfig),
          // WARNING: Do not parent m_pControllerLearningEventFilter to
          // ControllerManager because the CM, together with all its children,
          // is moved to the ControllerManager thread (m_pThread) and
          // runs its own event loop there.
          m_pControllerLearningEventFilter(
                  std::make_unique<ControllerLearningEventFilter>()),
          m_pollTimer(this),
          m_pThread(std::make_unique<QThread>()),
          m_skipPoll(false),
          m_hotplugTimer(this),
          m_hotplugChecksLeft(0),
          m_hotplugArmed(false),
          m_setUpRequested(false),
          m_scanCount(0) {
    qRegisterMetaType<std::shared_ptr<LegacyControllerMapping>>(
            "std::shared_ptr<LegacyControllerMapping>");

    // Create controller mapping paths in the user's home directory.
    QString userMappings = userMappingsPath(m_pConfig);
    if (!QDir(userMappings).exists()) {
        qDebug() << "Creating user controller mappings directory:" << userMappings;
        QDir().mkpath(userMappings);
    }

    m_pollTimer.setInterval(kPollInterval.toIntegerMillis());
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllerManager::slotPollDevices);

    m_hotplugTimer.setSingleShot(true);
    m_hotplugTimer.setInterval(kHotplugSettleTime.toIntegerMillis());
    connect(&m_hotplugTimer,
            &QTimer::timeout,
            this,
            &ControllerManager::slotCheckForNewDevices);

    m_pThread->setObjectName("ControllerManager");

    // Move the entire ControllerManager object and all its children (including
    // the poll timer) onto the ControllerManager thread.
    moveToThread(m_pThread.get());

    // The ControllerManager thread is high-priority because controller input can
    // affect audio output directly (e.g. scratching).
    m_pThread->start(QThread::HighPriority);

    connect(this, &ControllerManager::requestInitialize, this, &ControllerManager::slotInitialize);
    connect(this,
            &ControllerManager::requestSetUpDevices,
            this,
            &ControllerManager::slotSetUpDevices);
    connect(this, &ControllerManager::requestShutdown, this, &ControllerManager::slotShutdown);

    // Signal that we should run slotInitialize once our event loop has started
    // up. invokeMethod with QueuedConnection will post an event to our event loop,
    // so slotInitialize will not run until after the ControllerManager thread is fully
    // started and ready to process events.
    QMetaObject::invokeMethod(this, &ControllerManager::slotInitialize, Qt::QueuedConnection);
}

ControllerManager::~ControllerManager() {
    // slotShutdown() closes controllers, deletes enumerators and calls
    // m_pThread->quit(). We must wait for the thread to finish before our
    // members (m_pollTimer, m_pControllerLearningEventFilter, …) are destroyed,
    // because they may still be accessed by the thread's event loop.
    emit requestShutdown(); // clazy:exclude=incorrect-emit
    m_pThread->wait();
    // m_pThread and m_pControllerLearningEventFilter are released by unique_ptr
    // after this point, in reverse declaration order.
}

ControllerLearningEventFilter* ControllerManager::getControllerLearningEventFilter() const {
    return m_pControllerLearningEventFilter.get();
}

void ControllerManager::slotInitialize() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    qDebug() << "ControllerManager:slotInitialize";

    // Initialize mapping info parsers. This object is only for use in the main
    // thread. Do not touch it from within ControllerManager.
    m_pMainThreadUserMappingEnumerator =
            QSharedPointer<MappingInfoEnumerator>::create(
                    userMappingsPath(m_pConfig));
    m_pMainThreadSystemMappingEnumerator =
            QSharedPointer<MappingInfoEnumerator>::create(
                    resourceMappingsPath(m_pConfig));

    // Instantiate all enumerators. Enumerators can take a long time to
    // construct since they interact with host MIDI APIs.
    {
        auto locker = lockMutex(&m_mutex);
#ifdef __PORTMIDI__
        m_enumerators.push_back(std::make_unique<PortMidiEnumerator>(m_pConfig));
#endif
#ifdef __HSS1394__
        m_enumerators.push_back(std::make_unique<Hss1394Enumerator>());
#endif
#ifdef __BULK__
        m_enumerators.push_back(std::make_unique<BulkEnumerator>());
#endif
#ifdef __HID__
        m_enumerators.push_back(std::make_unique<HidEnumerator>());
#endif
    } // Mutex locker released here

    // Created here, not in the constructor, so that it belongs to this thread:
    // its OS notifications are delivered by this thread's event loop.
    m_pHotplugWatcher = std::make_unique<ControllerHotplugWatcher>();
    connect(m_pHotplugWatcher.get(),
            &ControllerHotplugWatcher::devicesChanged,
            this,
            &ControllerManager::slotHotplugEvent);
    qDebug() << "Controller hotplug watcher active:" << m_pHotplugWatcher->isActive();

    emit initialized();
}

void ControllerManager::slotShutdown() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    stopPolling();

    // Before the enumerators go, and on this thread, which owns its window.
    m_hotplugTimer.stop();
    m_pHotplugWatcher.reset();

    // Before the enumerators, whose libraries these controllers still use.
    for (const RetiredControllers& retired : std::as_const(m_retiredControllers)) {
        qDeleteAll(retired.controllers);
    }
    m_retiredControllers.clear();

    // Clear m_enumerators before deleting the enumerators to prevent other code
    // paths from accessing them during teardown.
    auto locker = lockMutex(&m_mutex);
    std::vector<std::unique_ptr<ControllerEnumerator>> enumerators =
            std::move(m_enumerators); // m_enumerators is guaranteed empty after move
    locker.unlock();

    // Delete enumerators (and their owned Controllers) by letting unique_ptrs
    // go out of scope here — no raw deletes needed.
    enumerators.clear();

    // Stop the event loop after the enumerators are torn down, since the
    // controller scripting engines live inside the enumerators.
    m_pThread->quit();
}

void ControllerManager::updateControllerList() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // NOTE: this runs on startup and again whenever a rescan is requested, e.g. from
    // the Controllers settings page after replugging a device. Every Controller is
    // replaced here, so anything holding one must let go of it in its handler
    // of devicesChanged(). DlgPrefControllers and QmlControllerManagerProxy both
    // do. The replaced controllers are closed at once but deleted only after the
    // main thread has run those handlers: until then its pointers stay valid.
    // A device plugged in while Mixxx runs triggers this through
    // slotCheckForNewDevices() where the OS says so (Windows), and otherwise
    // only through a rescan.
    auto locker = lockMutex(&m_mutex);
    if (m_enumerators.empty()) {
        qWarning() << "updateControllerList called but no enumerators have been added!";
        return;
    }
    // Take a snapshot of the enumerator pointers while holding the lock.
    std::vector<ControllerEnumerator*> enumerators;
    enumerators.reserve(m_enumerators.size());
    for (const auto& pEnumerator : m_enumerators) {
        enumerators.push_back(pEnumerator.get());
    }
    locker.unlock();

    QList<Controller*> newDeviceList;
    QList<Controller*> retired;
    for (ControllerEnumerator* pEnumerator : enumerators) {
        newDeviceList.append(pEnumerator->queryDevices());
        retired.append(pEnumerator->takeRetiredDevices());
    }

    locker.relock();
    const bool changed = newDeviceList != m_controllers;
    m_controllers = std::move(newDeviceList);
    locker.unlock();
    if (changed) {
        emit devicesChanged();
    }
    deleteRetiredControllersAfterMainThread(std::move(retired));
}

void ControllerManager::deleteRetiredControllersAfterMainThread(
        QList<Controller*> retired) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (retired.isEmpty()) {
        return;
    }
    const quint64 scan = ++m_scanCount;
    m_retiredControllers.push_back({scan, std::move(retired)});

    QCoreApplication* pApp = QCoreApplication::instance();
    if (!pApp || pApp->thread() == thread()) {
        // No other thread to wait for.
        slotDeleteRetiredControllers(scan);
        return;
    }
    // The main thread runs its events in the order they were posted, so this
    // reaches it after the devicesChanged() handlers above, and only then
    // comes back here to delete. ~ControllerManager runs on the main thread
    // too, so the pointer cannot go stale between the check and the call.
    QMetaObject::invokeMethod(
            pApp,
            [pThis = QPointer<ControllerManager>(this), scan]() {
                if (pThis) {
                    QMetaObject::invokeMethod(
                            pThis.data(),
                            [pThis, scan]() {
                                pThis->slotDeleteRetiredControllers(scan);
                            },
                            Qt::QueuedConnection);
                }
            },
            Qt::QueuedConnection);
}

void ControllerManager::slotDeleteRetiredControllers(quint64 scan) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    auto it = m_retiredControllers.begin();
    while (it != m_retiredControllers.end()) {
        if (it->scan > scan) {
            ++it;
            continue;
        }
        qDeleteAll(it->controllers);
        it = m_retiredControllers.erase(it);
    }
}

void ControllerManager::setUpDevices() {
    if (!m_setUpRequested.exchange(true)) {
        emit requestSetUpDevices();
    }
}

QList<Controller*> ControllerManager::getControllers() const {
    const auto locker = lockMutex(&m_mutex);
    return m_controllers;
}

QMap<QString, QString> ControllerManager::presentDevices() const {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    auto locker = lockMutex(&m_mutex);
    std::vector<ControllerEnumerator*> enumerators;
    enumerators.reserve(m_enumerators.size());
    for (const auto& pEnumerator : m_enumerators) {
        enumerators.push_back(pEnumerator.get());
    }
    locker.unlock();

    QMap<QString, QString> devices;
    for (const ControllerEnumerator* pEnumerator : enumerators) {
        devices.insert(pEnumerator->presentDevices());
    }
    return devices;
}

void ControllerManager::slotHotplugEvent() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (!m_hotplugArmed) {
        return;
    }
    m_hotplugChecksLeft = kHotplugChecks;
    m_hotplugTimer.start();
}

void ControllerManager::slotCheckForNewDevices() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    const QMap<QString, QString> present = presentDevices();

    for (auto it = m_presentAtLastScan.cbegin(); it != m_presentAtLastScan.cend(); ++it) {
        if (!present.contains(it.key())) {
            m_absentSinceLastScan.insert(it.key());
        }
    }
    // Only arrivals count. A device that leaves is left alone: its Controller
    // stays, and gets rebuilt when the device comes back.
    QStringList arrived;
    for (auto it = present.cbegin(); it != present.cend(); ++it) {
        if ((!m_presentAtLastScan.contains(it.key()) ||
                    m_absentSinceLastScan.contains(it.key())) &&
                !arrived.contains(it.value())) {
            arrived.append(it.value());
        }
    }
    if (arrived.isEmpty()) {
        // One USB device raises several events, and its MIDI port can reach
        // WinMM after the last of them. Look again before giving up.
        if (--m_hotplugChecksLeft > 0) {
            m_hotplugTimer.start();
        }
        return;
    }
    m_hotplugChecksLeft = 0;

    // A rescan closes and reopens every controller, which restarts their
    // mappings. Worth it for a controller the user has set up, and harmless
    // when none is open. Otherwise, mid-set, say what was seen and leave the
    // rescan to the user.
    bool anyEnabled = false;
    for (const QString& name : std::as_const(arrived)) {
        anyEnabled = anyEnabled || isControllerEnabled(m_pConfig, name);
    }
    bool anyOpen = false;
    for (const Controller* pController : getControllers()) {
        anyOpen = anyOpen || pController->isOpen();
    }
    if (!anyEnabled && anyOpen) {
        qInfo() << "Controller hotplug:" << arrived
                << "plugged in; not rescanning while a controller is in use";
        m_presentAtLastScan = present;
        m_absentSinceLastScan.clear();
        emit controllersPluggedIn({}, arrived, false);
        return;
    }

    qInfo() << "Controller hotplug:" << arrived << "plugged in, rescanning";
    slotSetUpDevices();

    QStringList opened;
    QStringList notOpened;
    const QList<Controller*> controllers = getControllers();
    for (const QString& name : std::as_const(arrived)) {
        const bool isOpen = std::any_of(controllers.cbegin(),
                controllers.cend(),
                [&name](const Controller* pController) {
                    return pController->getName() == name && pController->isOpen();
                });
        (isOpen ? opened : notOpened).append(name);
    }
    emit controllersPluggedIn(opened, notOpened, true);
}

QList<Controller*> ControllerManager::getControllerList(bool bOutputDevices, bool bInputDevices) {
    qDebug() << "ControllerManager::getControllerList";

    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    // Create a list of controllers filtered to match the given input/output
    // options.
    QList<Controller*> filteredDeviceList;

    for (Controller* device : controllers) {
        if ((bOutputDevices == device->isOutputDevice()) ||
            (bInputDevices == device->isInputDevice())) {
            filteredDeviceList.push_back(device);
        }
    }
    return filteredDeviceList;
}

QString ControllerManager::getConfiguredMappingFileForDevice(const QString& name) const {
    // Thread-Safe : ConfigObject<ValueType>::getValueString/get is protected by QWriteLocker
    return m_pConfig->getValueString(ConfigKey(kSettingsGroup, sanitizeDeviceName(name)));
}

void ControllerManager::slotSetUpDevices() {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    qDebug() << "ControllerManager: Setting up devices";
    // From here on a new request means a new scan.
    m_setUpRequested = false;
    emit scanStarted();

    // The baseline hotplug compares against. Taken before the scan, so a device
    // that arrives during it still counts as new afterwards.
    m_presentAtLastScan = presentDevices();
    m_absentSinceLastScan.clear();
    m_hotplugArmed = true;

    updateControllerList();
    const QList<Controller*> deviceList = getControllerList(false, true);
    QStringList mappingPaths(getMappingPaths(m_pConfig));

    for (Controller* pController : deviceList) {
        QString name = pController->getName();

        if (pController->isOpen()) {
            pController->close();
        }

        // The filename for this device name.
        QString deviceName = sanitizeDeviceName(name);

        // Check if device is enabled
        if (!m_pConfig->getValue(ConfigKey("[Controller]", deviceName), 0)) {
            continue;
        }

        // Check if device has a configured mapping
        QString mappingFilePath = getConfiguredMappingFileForDevice(deviceName);
        if (mappingFilePath.isEmpty()) {
            continue;
        }

        qDebug() << "Searching for controller mapping" << mappingFilePath
                 << "in paths:" << mappingPaths.join(",");
        QFileInfo mappingFile = findMappingFile(mappingFilePath, mappingPaths);
        if (!mappingFile.exists()) {
            qDebug() << "Could not find" << mappingFilePath << "in any mapping path.";
            continue;
        }

        std::shared_ptr<LegacyControllerMapping> pMapping =
                LegacyControllerMappingFileHandler::loadMapping(
                        mappingFile, resourceMappingsPath(m_pConfig));

        if (!pMapping) {
            continue;
        }
        pMapping->loadSettings(m_pConfig, pController->getName());

        // This runs on the main thread but LegacyControllerMapping is not thread safe, so clone it.
        pController->setMapping(std::move(pMapping));

        // If we are in safe mode, skip opening controllers.
        if (CmdlineArgs::Instance().getSafeMode()) {
            qDebug() << "We are in safe mode -- skipping opening controller.";
            continue;
        }

        qDebug() << "Opening controller:" << name;

        int value = pController->open(m_pConfig->getResourcePath());
        if (value != 0) {
            qWarning() << "There was a problem opening" << name;
            continue;
        }
    }

    pollIfAnyControllersOpen();
    emit scanFinished();
}

void ControllerManager::pollIfAnyControllersOpen() {
    // Not Thread-Safe because calls startPolling()/stopPolling()
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    auto locker = lockMutex(&m_mutex);
    QList<Controller*> controllers = m_controllers;
    locker.unlock();

    bool shouldPoll = false;
    for (Controller* pController : controllers) {
        if (pController->isOpen() && pController->isPolling()) {
            shouldPoll = true;
        }
    }
    if (shouldPoll) {
        startPolling();
    } else {
        stopPolling();
    }
}

void ControllerManager::startPolling() {
    // Not Thread-Safe because QTimer::start() must be called from the thread that owns the timer.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // Start the polling timer.
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
        qDebug() << "Controller polling started.";
    }
}

void ControllerManager::stopPolling() {
    // Not Thread-Safe because QTimer::stop() must be called from the thread that owns the timer.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    m_pollTimer.stop();
    qDebug() << "Controller polling stopped.";
}

void ControllerManager::slotPollDevices() {
    // Not Thread-Safe because it accesses m_controllers and m_skipPoll without a mutex.
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    // Note: this function is called from a high priority thread which
    // may stall the GUI or may reduce the available CPU time for other
    // High Priority threads like caching reader or broadcasting more
    // then desired, if it is called endless loop like.
    //
    // This especially happens if a controller like the 3x Speed
    // Stanton SCS.1D emits more massages than Mixxx is able to handle
    // or a controller like Hercules RMX2 goes wild. In such a case the
    // receive buffer is stacked up every call to insane values > 500 messages.
    //
    // To avoid this we pick here a strategies similar like the audio
    // thread. In case pollDevice() takes longer than a call cycle
    // we are cooperative a skip the next cycle to free at least some
    // CPU time
    //
    // Some random test data from a i5-3317U CPU @ 1.70GHz Running
    // Ubuntu Trusty:
    // * Idle poll: ~5 µs.
    // * 5 messages burst (full midi bandwidth): ~872 µs.

    if (m_skipPoll) {
        // skip poll in overload situation
        m_skipPoll = false;
        //qDebug() << "ControllerManager::pollDevices() skip";
        return;
    }

    mixxx::Duration start = mixxx::Time::elapsed();
    for (Controller* pDevice : std::as_const(m_controllers)) {
        if (pDevice->isOpen() && pDevice->isPolling()) {
            pDevice->poll();
        }
    }

    mixxx::Duration duration = mixxx::Time::elapsed() - start;
    if (duration > kPollInterval) {
        m_skipPoll = true;
    }
    //qDebug() << "ControllerManager::pollDevices()" << duration << start;
}

void ControllerManager::openController(Controller* pController) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (!pController) {
        return;
    }
    if (pController->isOpen()) {
        pController->close();
    }
    int result = pController->open(m_pConfig->getResourcePath());
    pollIfAnyControllersOpen();

    // If successfully opened the device, apply the mapping and save the
    // preference setting.
    if (result == 0) {
        // Update configuration to reflect controller is enabled.
        m_pConfig->setValue(
                ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 1);
    }
}

void ControllerManager::closeController(Controller* pController) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    if (!pController) {
        return;
    }
    pController->close();
    pollIfAnyControllersOpen();
    // Update configuration to reflect controller is disabled.
    m_pConfig->setValue(
            ConfigKey("[Controller]", sanitizeDeviceName(pController->getName())), 0);
}

// This needs to be called in a Qt::BlockingQueuedConnection so that the
// signaling thread can't alter the LegacyControllerMapping during applying
void ControllerManager::slotApplyMapping(Controller* pController,
        std::shared_ptr<LegacyControllerMapping> pMapping,
        bool bEnabled) {
    DEBUG_ASSERT_THIS_QOBJECT_THREAD_AFFINITY();
    VERIFY_OR_DEBUG_ASSERT(pController) {
        qWarning() << "slotApplyMapping got invalid controller!";
        return;
    }
    // A request that was on its way while a rescan replaced the controller.
    if (!getControllers().contains(pController)) {
        qWarning() << "slotApplyMapping: the controller was replaced by a rescan";
        return;
    }

    closeController(pController);
    ConfigKey key(kSettingsGroup, sanitizeDeviceName(pController->getName()));
    if (!pMapping) {
        // Unset the controller mapping for this controller
        pController->setMapping(nullptr);
        m_pConfig->remove(key);
        emit mappingApplied(false);
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(!pMapping->isDirty()) {
        qWarning() << "Mapping is dirty, changes might be lost on restart!";
    }

    // Save the file path/name in the config so it can be auto-loaded at
    // startup next time
    m_pConfig->set(key, pMapping->filePath());

    pController->setMapping(std::move(pMapping));

    if (bEnabled) {
        emit mappingApplied(pController->isMappable());
    } else {
        emit mappingApplied(false);
        return;
    }

    // Note: openController() may call ControllerRenderingEngine::setup()
    // Which has a blocking invokeMethod() call for QOffscreenSurface::create()
    // That why we need to return from this blocking call first.
    QMetaObject::invokeMethod(
            this,
            [this, pController]() { openController(pController); },
            Qt::QueuedConnection);
}

// static
QList<QString> ControllerManager::getMappingPaths(UserSettingsPointer pConfig) {
    QList<QString> scriptPaths;
    scriptPaths.append(userMappingsPath(pConfig));
    scriptPaths.append(resourceMappingsPath(pConfig));
    return scriptPaths;
}
