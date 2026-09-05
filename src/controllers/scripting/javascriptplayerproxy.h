#pragma once

#include <QStringList>

#include "control/controlproxy.h"
#include "mixer/basetrackplayer.h"

#define PROPERTY_IMPL_GETTER(NAMESPACE, TYPE, NAME, GETTER) \
    TYPE NAMESPACE::GETTER() const {                        \
        const TrackPointer pTrack = m_pCurrentTrack;        \
        if (pTrack == nullptr) {                            \
            return TYPE();                                  \
        }                                                   \
        return pTrack->GETTER();                            \
    }

class JavascriptPlayerProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString artist READ getArtist NOTIFY artistChanged)
    Q_PROPERTY(QString title READ getTitle NOTIFY titleChanged)
    Q_PROPERTY(QString album READ getAlbum NOTIFY albumChanged)
    Q_PROPERTY(QString albumArtist READ getAlbumArtist NOTIFY albumArtistChanged)
    Q_PROPERTY(QString genre READ getGenre STORED false NOTIFY genreChanged)
    Q_PROPERTY(QString composer READ getComposer NOTIFY composerChanged)
    Q_PROPERTY(QString grouping READ getGrouping NOTIFY groupingChanged)
    Q_PROPERTY(QString year READ getYear NOTIFY yearChanged)
    Q_PROPERTY(QString trackNumber READ getTrackNumber NOTIFY trackNumberChanged)
    Q_PROPERTY(QString trackTotal READ getTrackTotal NOTIFY trackTotalChanged)
    Q_PROPERTY(QString key READ getKeyText NOTIFY keyChanged)
#ifdef __STEM__
    Q_PROPERTY(QStringList stemLabels READ getStemLabels NOTIFY stemLabelsChanged)
    Q_PROPERTY(QStringList stemColors READ getStemColors NOTIFY stemColorsChanged)
#endif

  public:
    explicit JavascriptPlayerProxy(BaseTrackPlayer* pTrackPlayer, QObject* parent);

    QString getTitle() const;
    QString getArtist() const;
    QString getAlbum() const;
    QString getAlbumArtist() const;
    QString getGenre() const;
    QString getComposer() const;
    QString getGrouping() const;
    QString getYear() const;
    QString getTrackNumber() const;
    QString getTrackTotal() const;
    QString getKeyText() const;
#ifdef __STEM__
    /// Labels of the loaded track's stems in stem order, as written by the
    /// creator of the STEM file (e.g. "Drums", "Bass", "Synths", "Vox").
    /// Empty when no track is loaded or the track has no stems.
    QStringList getStemLabels() const;
    /// Colors of the loaded track's stems in stem order, as "#rrggbb".
    QStringList getStemColors() const;
#endif

  public slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);

  signals:
    void trackUnloaded();
    void albumChanged(const QString& newAlbum);
    void titleChanged(const QString& newTitle);
    void artistChanged(const QString& newArtist);
    void albumArtistChanged(const QString& newAlbumArtist);
    void genreChanged(const QString& newGenre);
    void composerChanged(const QString& newComposer);
    void groupingChanged(const QString& grouping);
    void yearChanged(const QString& newYear);
    void trackNumberChanged(const QString& newTrackNumber);
    void trackTotalChanged(const QString& newTrackTotal);
    void keyChanged(const QString& newKey);
#ifdef __STEM__
    void stemLabelsChanged(const QStringList& newStemLabels);
    void stemColorsChanged(const QStringList& newStemColors);
#endif

  private slots:
    // Track::keyChanged has no arguments,
    // so we bridge them with dedicated slots that re-read the value.
    void slotKeyChanged();
#ifdef __STEM__
    // Track::stemsUpdated has no arguments either.
    void slotStemsChanged();
#endif

  protected:
    void disconnectTrack();
    QPointer<BaseTrackPlayer> m_pTrackPlayer;
    TrackPointer m_pCurrentTrack;
    ControlProxy m_keyNotation;
};
