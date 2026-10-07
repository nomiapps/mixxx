#pragma once

#include <clap/clap.h>

#include <QByteArray>
#include <QLibrary>
#include <QString>
#include <memory>

/// One loaded .clap file. A .clap file is a shared library that exports a
/// `clap_entry` struct, through which the host reaches the plugins inside it.
/// The library stays loaded for as long as anything holds a pointer to it.
class CLAPLibrary {
  public:
    /// Returns a null pointer if the file is not a usable CLAP module.
    static std::shared_ptr<CLAPLibrary> load(const QString& path);
    ~CLAPLibrary();

    const clap_plugin_factory* factory() const {
        return m_pFactory;
    }
    const QString& path() const {
        return m_path;
    }

  private:
    explicit CLAPLibrary(const QString& path);

    const QString m_path;
    QLibrary m_library;
    const clap_plugin_entry* m_pEntry;
    const clap_plugin_factory* m_pFactory;
};

typedef std::shared_ptr<CLAPLibrary> CLAPLibraryPointer;

/// One instance of a plugin from a CLAPLibrary.
///
/// CLAP splits its calls between a main thread and an audio thread. Creating,
/// activating and destroying an instance belong to the main thread;
/// startProcessing() and everything done with plugin() while processing
/// belong to the audio thread.
class CLAPPluginInstance {
  public:
    /// Returns a null pointer if the plugin could not be created.
    static std::unique_ptr<CLAPPluginInstance> create(
            CLAPLibraryPointer pLibrary, const QByteArray& pluginId);
    ~CLAPPluginInstance();

    const clap_plugin* plugin() const {
        return m_pPlugin;
    }

    template<typename Extension>
    const Extension* extension(const char* extensionId) const {
        return static_cast<const Extension*>(
                m_pPlugin->get_extension(m_pPlugin, extensionId));
    }

    bool activate(double sampleRate, uint32_t maxFrames);
    bool isActive() const {
        return m_active;
    }
    /// Frames by which the output lags the input. Valid once activated.
    uint32_t latencyFrames() const;

    bool startProcessing();

  private:
    CLAPPluginInstance(CLAPLibraryPointer pLibrary, const clap_plugin* pPlugin);

    // Keeps the code behind m_pPlugin loaded.
    CLAPLibraryPointer m_pLibrary;
    const clap_plugin* m_pPlugin;
    bool m_active;
    bool m_processing;
};
