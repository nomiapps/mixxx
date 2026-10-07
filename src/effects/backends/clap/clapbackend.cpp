#include "effects/backends/clap/clapbackend.h"

#include <QDir>
#include <QDirIterator>
#include <QProcessEnvironment>

#include "effects/backends/clap/clapeffectprocessor.h"
#include "util/assert.h"

namespace {

const QString kLibraryFilter = QStringLiteral("*.clap");
const QString kSearchPathVariable = QStringLiteral("CLAP_PATH");

bool isAudioEffect(const clap_plugin_descriptor* pDescriptor) {
    if (!pDescriptor->features) {
        return false;
    }
    for (const char* const* ppFeature = pDescriptor->features; *ppFeature; ++ppFeature) {
        if (qstrcmp(*ppFeature, CLAP_PLUGIN_FEATURE_AUDIO_EFFECT) == 0) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

CLAPBackend::CLAPBackend()
        : CLAPBackend(defaultSearchPaths()) {
}

CLAPBackend::CLAPBackend(const QStringList& searchPaths) {
    enumeratePlugins(searchPaths);
}

// static
QStringList CLAPBackend::defaultSearchPaths() {
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QStringList paths;
#if defined(Q_OS_WIN)
    const QString commonFiles = environment.value(QStringLiteral("COMMONPROGRAMFILES"));
    if (!commonFiles.isEmpty()) {
        paths.append(QDir(commonFiles).filePath(QStringLiteral("CLAP")));
    }
    const QString localAppData = environment.value(QStringLiteral("LOCALAPPDATA"));
    if (!localAppData.isEmpty()) {
        paths.append(QDir(localAppData).filePath(QStringLiteral("Programs/Common/CLAP")));
    }
#elif defined(Q_OS_MACOS)
    // On macOS a .clap is a bundle, which this backend does not open yet.
#else
    paths.append(QDir::home().filePath(QStringLiteral(".clap")));
    paths.append(QStringLiteral("/usr/lib/clap"));
#endif
    paths.append(environment.value(kSearchPathVariable)
                    .split(QDir::listSeparator(), Qt::SkipEmptyParts));
    return paths;
}

void CLAPBackend::enumeratePlugins(const QStringList& searchPaths) {
    for (const QString& searchPath : searchPaths) {
        QDirIterator it(searchPath,
                {kLibraryFilter},
                QDir::Files,
                QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext()) {
            registerPlugins(it.next());
        }
    }
}

void CLAPBackend::registerPlugins(const QString& libraryPath) {
    const CLAPLibraryPointer pLibrary = CLAPLibrary::load(libraryPath);
    if (!pLibrary) {
        return;
    }
    const clap_plugin_factory* pFactory = pLibrary->factory();
    const uint32_t count = pFactory->get_plugin_count(pFactory);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_plugin_descriptor* pDescriptor = pFactory->get_plugin_descriptor(pFactory, i);
        if (!pDescriptor || !pDescriptor->id || !isAudioEffect(pDescriptor)) {
            continue;
        }
        const QString id = QString::fromUtf8(pDescriptor->id);
        if (m_registeredEffects.contains(id)) {
            // The same plugin installed in two folders: the first one wins.
            continue;
        }
        auto pManifest = CLAPManifestPointer::create(pLibrary, pDescriptor);
        if (!pManifest->isValid()) {
            qInfo() << "CLAPBackend: skipping" << pManifest->name()
                    << "from" << libraryPath
                    << (pManifest->status() == CLAPManifest::Status::IoNotStereo
                                       ? "(its main input and output are not both stereo)"
                                       : "(could not be created)");
            continue;
        }
        qInfo() << "CLAPBackend: found" << pManifest->name() << "in" << libraryPath;
        m_registeredEffects.insert(id, pManifest);
    }
}

const QList<QString> CLAPBackend::getEffectIds() const {
    return m_registeredEffects.keys();
}

bool CLAPBackend::canInstantiateEffect(const QString& effectId) const {
    return m_registeredEffects.contains(effectId);
}

EffectManifestPointer CLAPBackend::getManifest(const QString& effectId) const {
    return m_registeredEffects.value(effectId);
}

const QList<EffectManifestPointer> CLAPBackend::getManifests() const {
    QList<EffectManifestPointer> list;
    for (const auto& pManifest : m_registeredEffects) {
        list.append(pManifest);
    }
    return list;
}

std::unique_ptr<EffectProcessor> CLAPBackend::createProcessor(
        const EffectManifestPointer pManifest) const {
    CLAPManifestPointer pCLAPManifest = m_registeredEffects.value(pManifest->id());
    VERIFY_OR_DEBUG_ASSERT(pCLAPManifest) {
        return nullptr;
    }
    return std::make_unique<CLAPEffectProcessor>(pCLAPManifest);
}
