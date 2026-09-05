# Is build\mixxx.exe new enough to run against the source tree as it stands?
#
# Most of res/qml is loaded from disk at runtime, so editing it needs no rebuild.
# A minority is COMPILED INTO the binary by qt_add_qml_module, and so is all of
# src/. When one of those changes and the binary is not rebuilt, nothing warns
# you: the QML imports a module the exe does not contain, or calls a Q_INVOKABLE
# that is not there, and it fails at the moment you touch that control.
#
# This bit twice on 2026-09-05:
#   - res/qml/Mixxx/Controls moved to res/qml/Edge/Controls as a new QML module.
#     The on-disk QML imported Edge.Controls; the running binary had no such
#     module, so six files' worth of controls would have failed to resolve.
#   - A merge brought in QmlPlayerManagerProxy::sampleLoopToSampler while the
#     binary predated it. Deck/Loop.qml's Sample button called a method the exe
#     did not have.
#
# The compiled-in QML list is PARSED OUT OF CMakeLists.txt rather than hardcoded,
# because that list changes (it did today) and a hardcoded copy would go stale
# exactly when it matters.
#
# Exit 0 = fresh (or no binary yet, which is a different problem). Exit 1 = stale.

[CmdletBinding()]
param(
    [string]$Root = 'C:\StreamDeck\src\mixxx-edge',
    [switch]$Quiet
)

$exe = Join-Path $Root 'build\mixxx.exe'
if (-not (Test-Path $exe)) {
    if (-not $Quiet) { Write-Host "no binary yet: $exe" -ForegroundColor Yellow }
    exit 0
}
$exeTime = (Get-Item $exe).LastWriteTime

# --- compiled-in QML: every QML_FILES entry inside a qt_add_qml_module block ---
$cmake = Join-Path $Root 'CMakeLists.txt'
$compiledQml = @()
if (Test-Path $cmake) {
    $inModule = $false; $inFiles = $false
    foreach ($line in Get-Content $cmake) {
        if ($line -match '^\s*qt_add_qml_module\(') { $inModule = $true; $inFiles = $false; continue }
        if ($inModule) {
            if ($line -match '^\s*QML_FILES\s*$') { $inFiles = $true; continue }
            if ($line -match '^\s*\)\s*$') { $inModule = $false; $inFiles = $false; continue }
            if ($inFiles -and $line -match '^\s*(\S+\.(qml|mjs))\s*$') {
                $compiledQml += (Join-Path $Root ($matches[1] -replace '/', '\'))
            }
        }
    }
}

# --- everything else that lands in the binary ---
$watch = @()
$watch += $compiledQml
foreach ($d in @('src')) {
    $p = Join-Path $Root $d
    if (Test-Path $p) { $watch += (Get-ChildItem $p -Recurse -File -Include *.cpp,*.h,*.mm | ForEach-Object { $_.FullName }) }
}
if (Test-Path $cmake) { $watch += $cmake }

$newer = @()
foreach ($f in $watch) {
    if (-not (Test-Path $f)) { continue }
    $t = (Get-Item $f).LastWriteTime
    if ($t -gt $exeTime) { $newer += [pscustomobject]@{ File = $f.Replace("$Root\", ''); Modified = $t } }
}

if ($newer.Count -eq 0) {
    if (-not $Quiet) {
        Write-Host "build is fresh - mixxx.exe $($exeTime.ToString('HH:mm:ss')) is newer than all $($watch.Count) compiled-in sources" -ForegroundColor Green
    }
    exit 0
}

Write-Host ""
Write-Host "STALE BINARY: build\mixxx.exe is older than $($newer.Count) compiled-in source(s)." -ForegroundColor Red
Write-Host "  binary:  $($exeTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor Red
Write-Host ""
Write-Host "  These are compiled INTO the exe, so editing them without rebuilding fails" -ForegroundColor Yellow
Write-Host "  silently at runtime - an unresolved import or a missing Q_INVOKABLE:" -ForegroundColor Yellow
Write-Host ""
$newer | Sort-Object Modified -Descending | Select-Object -First 12 |
    ForEach-Object { Write-Host ("    {0}  {1}" -f $_.Modified.ToString('HH:mm:ss'), $_.File) }
if ($newer.Count -gt 12) { Write-Host "    ... and $($newer.Count - 12) more" }
Write-Host ""
Write-Host "  Rebuild:  build-edge.bat" -ForegroundColor Cyan
Write-Host ""
exit 1
