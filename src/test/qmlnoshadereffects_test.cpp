#include <gtest/gtest.h>

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace {

const QString kNewUiQmlDir = QStringLiteral(RESOURCE_FOLDER "/qml");

/// What the New UI must not use, matched against a line with its comments
/// removed. Every one of these is, or makes, a ShaderEffect item.
const QStringList kForbiddenPatterns = {
        // DropShadow, InnerShadow, Glow, ColorOverlay, OpacityMask, GaussianBlur...
        QStringLiteral(R"(^\s*import\s+Qt5Compat\.GraphicalEffects\b)"),
        // MultiEffect
        QStringLiteral(R"(^\s*import\s+QtQuick\.Effects\b)"),
        QStringLiteral(R"(\bShaderEffect(Source)?\s*\{)"),
        QStringLiteral(R"(\blayer\.effect\s*:)"),
};

/// The file with its comments blanked out, line structure kept.
QStringList linesWithoutComments(const QString& text) {
    QString code = text;
    static const QRegularExpression blockComment(
            QStringLiteral(R"(/\*.*?\*/)"),
            QRegularExpression::DotMatchesEverythingOption);
    auto it = blockComment.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        for (qsizetype i = match.capturedStart(); i < match.capturedEnd(); ++i) {
            if (code[i] != QLatin1Char('\n')) {
                code[i] = QLatin1Char(' ');
            }
        }
    }
    QStringList lines = code.split(QLatin1Char('\n'));
    static const QRegularExpression lineComment(QStringLiteral(R"(//.*$)"));
    for (QString& line : lines) {
        line.remove(lineComment);
    }
    return lines;
}

/// The fork's New UI draws its chrome without shader effects. Qt rebuilds
/// every ShaderEffect on the GUI thread when the window moves to a screen with
/// another scale factor, with file checks per shader per item; with several
/// of them on every button, dragging the window between two monitors froze
/// Mixxx, and the cursor with it, for three to four seconds each time.
///
/// Draw flat instead: a face and a hairline from Theme, Rectangle.gradient,
/// IconImage for a tinted icon, `clip` for a moving edge, and
/// Mixxx.Library.roundedCoverArtUrl() for a cover shown round. This covers
/// res/qml only; res/skins/LateNightQML is upstream's skin.
TEST(QmlNoShaderEffectsTest, NewUiUsesNoShaderEffects) {
    QList<QRegularExpression> forbidden;
    for (const QString& pattern : kForbiddenPatterns) {
        forbidden.append(QRegularExpression(pattern));
    }

    QStringList offences;
    int filesRead = 0;
    QDirIterator it(kNewUiQmlDir,
            {QStringLiteral("*.qml")},
            QDir::Files,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(path);
        ++filesRead;
        const QStringList lines = linesWithoutComments(QString::fromUtf8(file.readAll()));
        for (qsizetype i = 0; i < lines.size(); ++i) {
            for (const QRegularExpression& expression : std::as_const(forbidden)) {
                if (expression.match(lines[i]).hasMatch()) {
                    offences.append(QStringLiteral("%1:%2: %3")
                                    .arg(path.mid(kNewUiQmlDir.size() + 1))
                                    .arg(i + 1)
                                    .arg(lines[i].trimmed()));
                }
            }
        }
    }

    // The scan found the tree at all.
    EXPECT_GT(filesRead, 50);
    EXPECT_TRUE(offences.isEmpty())
            << "Shader effects in the New UI QML (see the comment on this test):\n"
            << qPrintable(offences.join(QLatin1Char('\n')));
}

} // namespace
