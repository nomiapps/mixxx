#include "effects/backends/clap/clapplugin.h"

#include <QDebug>

#include "util/versionstore.h"

namespace {

// Mixxx offers plugins no host extensions: it drives them through their
// parameters and audio ports only, and never shows a plugin's own window.
const void* CLAP_ABI hostGetExtension(const clap_host*, const char*) {
    return nullptr;
}

void CLAP_ABI hostRequestIgnored(const clap_host*) {
}

const clap_host* mixxxHost() {
    static const QByteArray version = VersionStore::version().toUtf8();
    static const clap_host host = {
            CLAP_VERSION,
            nullptr,
            "Mixxx",
            "Mixxx",
            "https://mixxx.org",
            version.constData(),
            hostGetExtension,
            hostRequestIgnored, // request_restart
            hostRequestIgnored, // request_process
            hostRequestIgnored, // request_callback
    };
    return &host;
}

} // anonymous namespace

CLAPLibrary::CLAPLibrary(const QString& path)
        : m_path(path),
          m_library(path),
          m_pEntry(nullptr),
          m_pFactory(nullptr) {
}

// static
std::shared_ptr<CLAPLibrary> CLAPLibrary::load(const QString& path) {
    std::shared_ptr<CLAPLibrary> pLibrary(new CLAPLibrary(path));
    if (!pLibrary->m_library.load()) {
        qWarning() << "CLAPLibrary: could not load" << path
                   << pLibrary->m_library.errorString();
        return nullptr;
    }
    const auto* pEntry = reinterpret_cast<const clap_plugin_entry*>(
            pLibrary->m_library.resolve("clap_entry"));
    if (!pEntry || !pEntry->init || !pEntry->deinit || !pEntry->get_factory) {
        qWarning() << "CLAPLibrary:" << path << "has no clap_entry";
        return nullptr;
    }
    if (!clap_version_is_compatible(pEntry->clap_version)) {
        qWarning() << "CLAPLibrary:" << path << "uses an incompatible CLAP version";
        return nullptr;
    }
    if (!pEntry->init(path.toUtf8().constData())) {
        qWarning() << "CLAPLibrary:" << path << "failed to initialize";
        return nullptr;
    }
    // From here on the destructor calls deinit().
    pLibrary->m_pEntry = pEntry;
    pLibrary->m_pFactory = static_cast<const clap_plugin_factory*>(
            pEntry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!pLibrary->m_pFactory) {
        qWarning() << "CLAPLibrary:" << path << "has no plugin factory";
        return nullptr;
    }
    return pLibrary;
}

CLAPLibrary::~CLAPLibrary() {
    if (m_pEntry) {
        m_pEntry->deinit();
    }
    // The library itself is left loaded: ~QLibrary does not unload it.
}

CLAPPluginInstance::CLAPPluginInstance(
        CLAPLibraryPointer pLibrary, const clap_plugin* pPlugin)
        : m_pLibrary(std::move(pLibrary)),
          m_pPlugin(pPlugin),
          m_active(false),
          m_processing(false) {
}

// static
std::unique_ptr<CLAPPluginInstance> CLAPPluginInstance::create(
        CLAPLibraryPointer pLibrary, const QByteArray& pluginId) {
    if (!pLibrary) {
        return nullptr;
    }
    const clap_plugin* pPlugin = pLibrary->factory()->create_plugin(
            pLibrary->factory(), mixxxHost(), pluginId.constData());
    if (!pPlugin) {
        qWarning() << "CLAPPluginInstance: could not create" << pluginId;
        return nullptr;
    }
    if (!pPlugin->init(pPlugin)) {
        qWarning() << "CLAPPluginInstance: could not initialize" << pluginId;
        pPlugin->destroy(pPlugin);
        return nullptr;
    }
    return std::unique_ptr<CLAPPluginInstance>(
            new CLAPPluginInstance(std::move(pLibrary), pPlugin));
}

CLAPPluginInstance::~CLAPPluginInstance() {
    // Mixxx deletes an effect's state on the main thread, after the engine
    // has let go of it, so nothing is processing any more.
    if (m_processing) {
        m_pPlugin->stop_processing(m_pPlugin);
    }
    if (m_active) {
        m_pPlugin->deactivate(m_pPlugin);
    }
    m_pPlugin->destroy(m_pPlugin);
}

bool CLAPPluginInstance::activate(double sampleRate, uint32_t maxFrames) {
    if (!m_active) {
        m_active = m_pPlugin->activate(m_pPlugin, sampleRate, 1, maxFrames);
    }
    return m_active;
}

uint32_t CLAPPluginInstance::latencyFrames() const {
    const auto* pLatency = extension<clap_plugin_latency>(CLAP_EXT_LATENCY);
    if (!m_active || !pLatency || !pLatency->get) {
        return 0;
    }
    return pLatency->get(m_pPlugin);
}

bool CLAPPluginInstance::startProcessing() {
    if (m_active && !m_processing) {
        m_processing = m_pPlugin->start_processing(m_pPlugin);
    }
    return m_processing;
}
