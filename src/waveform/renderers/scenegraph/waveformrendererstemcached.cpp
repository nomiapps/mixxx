#include "waveform/renderers/scenegraph/waveformrendererstemcached.h"

#include <QMatrix4x4>
#include <QSGGeometry>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "control/controlproxy.h"
#include "engine/channels/enginedeck.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/logger.h"
#include "util/math.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace {

constexpr int kVerticesPerRectangle = 6;
// Tiles this far outside the visible range (in tiles) are dropped. Anything
// closer is kept, detached from the scene, for the next scroll or nudge back.
constexpr int kKeepMargin = 4;

// WaveformRendererStem draws a strip every other device pixel in the scene
// graph backend, to work around a partial-drawing issue there. The cached
// twin keeps the same grid so the two look identical.
constexpr float kPixelPerStrip = 2.f;

const mixxx::Logger kLogger("WaveformRendererStemCached");

} // namespace

namespace allshader {

bool WaveformRendererStemCached::CacheKey::operator==(const CacheKey& other) const {
    if (pWaveform != other.pWaveform || completion != other.completion ||
            devicePixelRatio != other.devicePixelRatio) {
        return false;
    }
    // The frames-per-column ratio is recomputed from doubles every frame;
    // treat anything within rounding as the same zoom level.
    if (std::fabs(framesPerColumn - other.framesPerColumn) >
            std::max(framesPerColumn, other.framesPerColumn) * 1e-9) {
        return false;
    }
    for (int stem = 0; stem < kStems; ++stem) {
        for (int layer = 0; layer < kLayers; ++layer) {
            for (int c = 0; c < 4; ++c) {
                if (color[stem][layer][c] != other.color[stem][layer][c]) {
                    return false;
                }
            }
        }
    }
    return true;
}

WaveformRendererStemCached::WaveformRendererStemCached(
        WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    m_stackOrder.resize(mixxx::kMaxSupportedStems);
    std::iota(m_stackOrder.begin(), m_stackOrder.end(), 0);
    for (int stem = 0; stem < kStems; ++stem) {
        // The clip is what keeps a loud stem inside its lane while split; it
        // is set to the whole widget when the stems are stacked.
        m_stemClip[stem] = new QSGClipNode();
        m_stemClip[stem]->setIsRectangular(true);
        // A rectangular clip is scissored from clipRect(), but the node still
        // carries geometry -- QmlWaveformDisplay's own clip node borrows the
        // background rectangle's for this. Ours owns a rectangle of its own.
        auto* pClipGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
        pClipGeometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        m_stemClip[stem]->setGeometry(pClipGeometry);
        m_stemClip[stem]->setFlag(QSGNode::OwnsGeometry, true);
        appendChildNode(m_stemClip[stem]);
        m_stemRoot[stem] = new QSGTransformNode();
        m_stemClip[stem]->appendChildNode(m_stemRoot[stem]);
        for (int layer = 0; layer < kLayers; ++layer) {
            m_layerRoot[stem][layer] = new QSGTransformNode();
            m_stemRoot[stem]->appendChildNode(m_layerRoot[stem][layer]);
        }
    }
}

WaveformRendererStemCached::~WaveformRendererStemCached() {
    // Attached tiles would be deleted by their layer root; detached ones are
    // ours. Detach everything first so there is exactly one owner.
    clearTiles();
}

void WaveformRendererStemCached::onSetup(const QDomNode&) {
}

bool WaveformRendererStemCached::init() {
    m_pStemGain.clear();
    m_pStemMute.clear();
    if (m_waveformRenderer->getGroup().isEmpty()) {
        return true;
    }
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; stemIdx++) {
        QString stemGroup = EngineDeck::getGroupForStem(m_waveformRenderer->getGroup(), stemIdx);
        m_pStemGain.emplace_back(
                std::make_unique<ControlProxy>(stemGroup, QStringLiteral("volume")));
        m_pStemMute.emplace_back(
                std::make_unique<ControlProxy>(stemGroup, QStringLiteral("mute")));
        auto bringToForeground = [this, stemIdx](double) {
            if (!m_reorderOnChange) {
                return;
            }
            m_stackOrder.removeAll(stemIdx);
            m_stackOrder.append(stemIdx);
            m_stackOrderDirty = true;
        };
        m_pStemGain.back()->connectValueChanged(this, bringToForeground);
        m_pStemMute.back()->connectValueChanged(this, bringToForeground);
    }
    return true;
}

void WaveformRendererStemCached::applyStackOrder() {
    // Which stem is drawn last is which stem is on top; while the stems are
    // stacked that is the only thing the stack order does, since every lane
    // shares one centre line.
    for (int stem = 0; stem < kStems; ++stem) {
        removeChildNode(m_stemClip[stem]);
    }
    for (int stemIdx : std::as_const(m_stackOrder)) {
        if (stemIdx >= 0 && stemIdx < kStems) {
            appendChildNode(m_stemClip[stemIdx]);
        }
    }
    m_stackOrderDirty = false;
}

void WaveformRendererStemCached::clearTiles() {
    for (auto& [index, tile] : m_tiles) {
        detach(&tile);
        for (int stem = 0; stem < kStems; ++stem) {
            for (int layer = 0; layer < kLayers; ++layer) {
                delete tile.transform[stem][layer];
            }
        }
    }
    m_tiles.clear();
}

void WaveformRendererStemCached::attach(Tile* pTile) {
    if (pTile->attached) {
        return;
    }
    for (int stem = 0; stem < kStems; ++stem) {
        for (int layer = 0; layer < kLayers; ++layer) {
            m_layerRoot[stem][layer]->appendChildNode(pTile->transform[stem][layer]);
        }
    }
    pTile->attached = true;
}

void WaveformRendererStemCached::detach(Tile* pTile) {
    if (!pTile->attached) {
        return;
    }
    for (int stem = 0; stem < kStems; ++stem) {
        for (int layer = 0; layer < kLayers; ++layer) {
            m_layerRoot[stem][layer]->removeChildNode(pTile->transform[stem][layer]);
        }
    }
    pTile->attached = false;
}

void WaveformRendererStemCached::hideAll() {
    for (auto& [index, tile] : m_tiles) {
        detach(&tile);
    }
}

void WaveformRendererStemCached::buildTile(
        Tile* pTile, int tileIndex, const WaveformData* pData, int dataSize) {
    mixxx::waveform::StemColumnMax columns[kTileColumns];
    mixxx::waveform::reduceStemColumns(pData,
            dataSize,
            tileIndex * kTileColumns,
            kTileColumns,
            m_key.framesPerColumn,
            columns);

    // Horizontal unit: logical pixels, tile-local, so a column is one strip
    // wide and the tile starts at 0. Vertical unit: raw waveform values,
    // drawn about y = 0; the stem's matrix maps that onto its lane.
    const float columnWidth = kPixelPerStrip / m_key.devicePixelRatio;
    const float halfColumn = 0.5f * columnWidth;
    for (int stem = 0; stem < kStems; ++stem) {
        for (int layer = 0; layer < kLayers; ++layer) {
            Geometry& geometry = pTile->geometry[stem][layer]->geometry();
            RGBAVertexUpdater updater{
                    geometry.vertexDataAs<Geometry::RGBAColoredPoint2D>()};
            const float r = m_key.color[stem][layer][0];
            const float g = m_key.color[stem][layer][1];
            const float b = m_key.color[stem][layer][2];
            const float a = m_key.color[stem][layer][3];
            for (int column = 0; column < kTileColumns; ++column) {
                const float x = static_cast<float>(column) * columnWidth;
                const float value = static_cast<float>(columns[column].value[stem]);
                // The slip waveform is drawn from the centre upwards only, as
                // the uncached renderer does.
                updater.addRectangle({x - halfColumn, -value},
                        {x + halfColumn, m_isSlipRenderer ? 0.f : value},
                        {r, g, b, a});
            }
            DEBUG_ASSERT(updater.index() == kTileColumns * kVerticesPerRectangle);
            pTile->geometry[stem][layer]->markDirtyGeometry();
            pTile->geometry[stem][layer]->markDirtyMaterial();
        }
    }
    ++m_tilesBuilt;
}

WaveformRendererStemCached::Tile* WaveformRendererStemCached::tileAt(
        int tileIndex, const WaveformData* pData, int dataSize) {
    auto it = m_tiles.find(tileIndex);
    if (it != m_tiles.end()) {
        return &it->second;
    }
    Tile& tile = m_tiles[tileIndex];
    for (int stem = 0; stem < kStems; ++stem) {
        for (int layer = 0; layer < kLayers; ++layer) {
            tile.transform[stem][layer] = new QSGTransformNode();
            auto pGeometry = std::make_unique<GeometryNode>();
            pGeometry->initForRectangles<RGBAMaterial>(kTileColumns);
            tile.geometry[stem][layer] = pGeometry.release();
            // The transform node owns the geometry node (OwnedByParent), and
            // the layer root owns the transform node only while attached.
            tile.transform[stem][layer]->appendChildNode(tile.geometry[stem][layer]);
        }
    }
    buildTile(&tile, tileIndex, pData, dataSize);
    return &tile;
}

void WaveformRendererStemCached::update() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        hideAll();
        return;
    }
    const auto stemInfo = pTrack->getStemInfo();
    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (stemInfo.isEmpty() || waveform.isNull() || !waveform->hasStem()) {
        hideAll();
        return;
    }
    const int dataSize = waveform->getDataSize();
    const WaveformData* pData = dataSize > 1 ? waveform->data() : nullptr;
    if (!pData) {
        hideAll();
        return;
    }

    if (m_stackOrderDirty) {
        applyStackOrder();
    }

    const auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                               : ::WaveformRendererAbstract::Play;
    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const float length = static_cast<float>(m_waveformRenderer->getLength());
    const int pixelLength = static_cast<int>(m_waveformRenderer->getLength() * devicePixelRatio);
    const int stripLength = static_cast<int>(static_cast<float>(pixelLength) / kPixelPerStrip);
    if (stripLength <= 0) {
        hideAll();
        return;
    }

    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition(positionType) * visualFramesSize;
    const double framesPerColumn =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(stripLength);
    if (!(framesPerColumn > 0.0)) {
        hideAll();
        return;
    }

    CacheKey key;
    key.pWaveform = waveform.data();
    // While the analyzer is still filling the waveform, the data behind the
    // tiles changes under them: rebuild every frame until it is complete,
    // which is what the uncached renderer always does.
    key.completion = std::min(waveform->getCompletion(), dataSize);
    key.framesPerColumn = framesPerColumn;
    key.devicePixelRatio = devicePixelRatio;
    for (int stem = 0; stem < kStems; ++stem) {
        const QColor stemColor = stem < stemInfo.size() ? stemInfo[stem].getColor() : QColor();
        for (int layer = 0; layer < kLayers; ++layer) {
            key.color[stem][layer][0] = static_cast<float>(stemColor.redF());
            key.color[stem][layer][1] = static_cast<float>(stemColor.greenF());
            key.color[stem][layer][2] = static_cast<float>(stemColor.blueF());
            key.color[stem][layer][3] = static_cast<float>(stemColor.alphaF()) *
                    (layer ? m_opacity : m_outlineOpacity);
        }
    }
    if (!(key == m_key)) {
        clearTiles();
        m_key = key;
        if (key.completion >= dataSize) {
            // Once per track/zoom/colour change; silent while the analyzer is
            // still filling the waveform (that would be every frame).
            kLogger.debug() << "stem tile cache reset:" << m_waveformRenderer->getGroup()
                            << "framesPerColumn" << framesPerColumn
                            << "dpr" << devicePixelRatio;
        }
    }

    // Per-frame values that live in matrices rather than vertices.
    float allGain = 1.f;
    getGains(&allGain, nullptr, nullptr, nullptr);
    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    // A single-stem view is laid out like an unsplit waveform: its one stem
    // gets the whole breadth rather than a quarter of it.
    const bool splitLanes = m_splitStemTracks && m_stemIndex < 0;
    const float stemBreadth = splitLanes ? breadth / 4.0f : 0.f;
    const float halfBreadth = (splitLanes ? stemBreadth : breadth) / 2.0f;
    const float heightFactor = allGain * halfBreadth / m_maxValue;
    const uint selectedStems = m_waveformRenderer->getSelectedStems();

    for (int stem = 0; stem < kStems; ++stem) {
        const float laneCentre = (splitLanes ? stem * stemBreadth : 0.f) + halfBreadth;
        const QRectF clipRect = splitLanes
                ? QRectF(0., stem * stemBreadth, length, stemBreadth)
                : QRectF(0., 0., length, breadth);
        if (clipRect != m_clipRect[stem]) {
            m_clipRect[stem] = clipRect;
            m_stemClip[stem]->setClipRect(clipRect);
            QSGGeometry::updateRectGeometry(m_stemClip[stem]->geometry(), clipRect);
            m_stemClip[stem]->markDirty(QSGNode::DirtyGeometry);
        }

        // A single-stem view hides the others outright, outline included.
        const bool hidden = m_stemIndex >= 0 && stem != m_stemIndex;
        QMatrix4x4 matrix;
        matrix.translate(0.f, laneCentre);
        matrix.scale(1.f, hidden ? 0.f : heightFactor);
        m_stemRoot[stem]->setMatrix(matrix);

        // The outline layer shows the signal before the stem's own gain; the
        // body layer is what mute, volume and the selected-stem mask act on.
        float bodyFactor = 1.f;
        if (selectedStems) {
            bodyFactor = (selectedStems & 1 << stem) ? 1.f : 0.f;
        } else if (!m_pStemMute.empty() && m_pStemMute[stem]->toBool()) {
            bodyFactor = 0.f;
        } else if (!m_pStemGain.empty()) {
            bodyFactor = static_cast<float>(m_pStemGain[stem]->get());
        }
        QMatrix4x4 bodyMatrix;
        bodyMatrix.scale(1.f, bodyFactor);
        m_layerRoot[stem][0]->setMatrix(QMatrix4x4());
        m_layerRoot[stem][1]->setMatrix(bodyMatrix);
    }

    // The first visible column on the grid, as WaveformRendererStem rounds
    // it, and the tiles that cover the visible columns.
    const float columnWidth = kPixelPerStrip / devicePixelRatio;
    const int firstColumn = static_cast<int>(std::lround(firstVisualFrame / framesPerColumn));
    const int lastColumn = firstColumn + stripLength - 1;
    const int firstTile = static_cast<int>(
            std::floor(static_cast<double>(firstColumn) / kTileColumns));
    const int lastTile = static_cast<int>(
            std::floor(static_cast<double>(lastColumn) / kTileColumns));

    for (int tileIndex = firstTile; tileIndex <= lastTile; ++tileIndex) {
        Tile* pTile = tileAt(tileIndex, pData, dataSize);
        const float offset =
                static_cast<float>(tileIndex * kTileColumns - firstColumn) * columnWidth;
        QMatrix4x4 matrix;
        matrix.translate(offset, 0.f);
        for (int stem = 0; stem < kStems; ++stem) {
            for (int layer = 0; layer < kLayers; ++layer) {
                pTile->transform[stem][layer]->setMatrix(matrix);
            }
        }
        attach(pTile);
    }

    // Park tiles that scrolled out, drop the ones that are far gone.
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        const int tileIndex = it->first;
        if (tileIndex >= firstTile && tileIndex <= lastTile) {
            ++it;
            continue;
        }
        detach(&it->second);
        if (tileIndex < firstTile - kKeepMargin || tileIndex > lastTile + kKeepMargin) {
            for (int stem = 0; stem < kStems; ++stem) {
                for (int layer = 0; layer < kLayers; ++layer) {
                    delete it->second.transform[stem][layer];
                }
            }
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace allshader
