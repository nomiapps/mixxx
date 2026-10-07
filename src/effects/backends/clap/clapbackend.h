#pragma once

#include <QHash>
#include <QStringList>

#include "effects/backends/clap/clapmanifest.h"
#include "effects/backends/effectsbackend.h"
#include "effects/defs.h"

/// Offers the CLAP audio effect plugins installed on this machine as effects.
/// Refer to EffectsBackend for documentation
class CLAPBackend : public EffectsBackend {
  public:
    /// Scans the folders where CLAP plugins are installed.
    CLAPBackend();
    /// Scans the given folders, and the folders inside them.
    explicit CLAPBackend(const QStringList& searchPaths);
    ~CLAPBackend() override = default;

    /// The folders the CLAP specification names for this platform, then
    /// those listed in the CLAP_PATH environment variable.
    static QStringList defaultSearchPaths();

    EffectBackendType getType() const override {
        return EffectBackendType::CLAP;
    };

    const QList<QString> getEffectIds() const override;
    EffectManifestPointer getManifest(const QString& effectId) const override;
    const QList<EffectManifestPointer> getManifests() const override;
    std::unique_ptr<EffectProcessor> createProcessor(
            const EffectManifestPointer pManifest) const override;
    bool canInstantiateEffect(const QString& effectId) const override;

  private:
    void enumeratePlugins(const QStringList& searchPaths);
    void registerPlugins(const QString& libraryPath);

    QHash<QString, CLAPManifestPointer> m_registeredEffects;
};
