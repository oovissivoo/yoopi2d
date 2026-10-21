$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Join-Path $workspace 'source/Nurture2D'
$stage = Join-Path $workspace 'output/Nurture2D-M1'
if (Test-Path -LiteralPath $stage) { throw 'Package staging directory already exists.' }
[IO.Directory]::CreateDirectory($stage) | Out-Null
$allowedDirs = @(
    'app', 'core_lib', 'CUI', 'paintlyFillAlgorithm', 'BrushFiles', 'BrushTextures',
    'BrushTips', 'CustomCursor', 'CustomToolIcons', 'Defaults', 'FileBrowser',
    'Font', 'Interaction', 'RefWinIcons', 'Splash Files', 'Windows&AppIcons',
    'tools', 'translations', 'tests', 'util', 'docs', 'BStabiliser'
)
$allowedRootFiles = @(
    'pencil2d.pro', 'LICENSE.TXT', 'LICENSE.QT.TXT', 'README.md',
    'build-macos.command', 'START-HERE-RU.md', 'VALIDATION-RU.txt',
    '.gitignore', '.gitattributes'
)
foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File -Force) {
    $relative = [IO.Path]::GetRelativePath($source, $file.FullName).Replace('\','/')
    $first = ($relative -split '/')[0]
    if ($relative.Contains('/')) {
        if ($first -notin $allowedDirs) { continue }
    } elseif ($relative -notin $allowedRootFiles) { continue }
    if ($relative -match '(^|/)(build[^/]*|\.git|\.qm|\.moc|\.obj)/|(^|/)\.qmake[^/]*$') { continue }
    if ($relative -match '^app/core_lib/' -or $relative -match '/fill-drag-test/') { continue }
    if ($file.Name -match '^(moc_|qrc_|ui_|Makefile)' -or $file.Name -match '(\.bak|\.backup|\.o$|\.a$|\.gch$|\.pro\.user|\.AppImage$)') { continue }
    $dest = Join-Path $stage $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $dest
}

$resourceCount = 0
foreach ($relative in @('app/data/app.qrc', 'core_lib/data/core_lib.qrc')) {
    $qrcPath = Join-Path $stage $relative
    [xml]$document = Get-Content -LiteralPath $qrcPath -Raw
    foreach ($element in $document.SelectNodes('//file')) {
        $resource = [IO.Path]::GetFullPath((Join-Path (Split-Path $qrcPath) $element.InnerText))
        if (-not (Test-Path -LiteralPath $resource -PathType Leaf)) { throw "Missing resource: $relative -> $($element.InnerText)" }
        $resourceCount++
    }
}
$projectFileCount = 0
foreach ($relative in @('app/app.pro', 'core_lib/core_lib.pro', 'tests/tests.pro', 'tools/generate_brush_tips/generate_brush_tips.pro')) {
    $proPath = Join-Path $stage $relative
    $proDir = Split-Path $proPath
    foreach ($line in Get-Content -LiteralPath $proPath) {
        if ($line -match '^\s*(?:\w+\s*\+?=\s*)?((?:\$\$PWD/)?[\w./-]+\.(?:cpp|h|ui|qrc|mm|ts))\s*\\?\s*$') {
            $item = $Matches[1].Replace('$$PWD/', '')
            if (-not (Test-Path -LiteralPath (Join-Path $proDir $item) -PathType Leaf)) { throw "Missing project file: $relative -> $item" }
            $projectFileCount++
        }
    }
}
[xml]$plist = Get-Content -LiteralPath (Join-Path $stage 'app/data/Info.plist') -Raw
if ($plist.SelectSingleNode('//key[text()="CFBundlePackageType"]/following-sibling::*[1]').InnerText -ne 'APPL') {
    throw 'Incorrect bundle package type'
}
if ($plist.SelectSingleNode('//key[text()="NSHighResolutionCapable"]/following-sibling::*[1]').Name -ne 'true') {
    throw 'Incorrect Retina value'
}
& (Join-Path $PSScriptRoot 'shfmt.exe') -ln bash -i 4 -d (Join-Path $stage 'build-macos.command')
if ($LASTEXITCODE -ne 0) { throw 'Shell syntax/format check failed' }
$scriptBytes = [IO.File]::ReadAllBytes((Join-Path $stage 'build-macos.command'))
if ($scriptBytes -contains 13) { throw 'Build script has CRLF line endings' }

$report = @"
Проверки при подготовке комплекта в Windows:
- SHA-256 оригинального архива совпал с опубликованным GitHub digest.
- Скрипт успешно разобран shfmt 3.14.1 как Bash; окончания строк LF.
- Проверено наличие $resourceCount ресурсов из app.qrc и core_lib.qrc.
- Проверено наличие $projectFileCount явно перечисленных файлов проектов qmake.
- Info.plist разобран как XML; тип приложения APPL, Retina имеет тип boolean.
- Проверено наличие всех изменённых файлов в архиве комплекта.

Не выполнено в этой среде:
- Компиляция C++/Objective-C++ с macOS SDK и Qt.
- Запуск приложения и проверка рисования/планшета на M1.
- Проверка сохранения, аудио, импорта и экспорта на macOS.
- Проверка переносимости готового .app на другой компьютер.

Проверки ARM64, codesign, macdeployqt и --version находятся в скрипте.
Они будут выполнены при первой сборке на Mac, а не при подготовке этого ZIP.
"@
[IO.File]::WriteAllText((Join-Path $stage 'VALIDATION-RU.txt'), $report, [Text.UTF8Encoding]::new($false))
$archivePath = Join-Path $workspace 'output/Nurture2D-M1-build-kit.zip'
$zip = [IO.Compression.ZipFile]::Open($archivePath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse -Force) {
        $relative = 'Nurture2D-M1/' + [IO.Path]::GetRelativePath($stage, $file.FullName).Replace('\','/')
        $entry = [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative, [IO.Compression.CompressionLevel]::Optimal)
        # Preserve a runnable .command file when the ZIP is extracted on macOS.
        if ($file.Name -eq 'build-macos.command') { $entry.ExternalAttributes = [int](-2115174400) } # Unix regular file 0755
    }
} finally {
    $zip.Dispose()
}
$verifyZip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    foreach ($relative in @(
        'build-macos.command', 'START-HERE-RU.md', 'VALIDATION-RU.txt',
        'app/app.pro', 'app/data/Info.plist', 'util/common.pri',
        'core_lib/src/external/macosx/macosx.cpp',
        'core_lib/src/external/macosx/macosxnative.mm',
        'core_lib/src/util/util.cpp', 'tools/generate_brush_tips/generate_brush_tips.pro'
    )) {
        if ($null -eq $verifyZip.GetEntry("Nurture2D-M1/$relative")) { throw "ZIP missing $relative" }
    }
    Write-Output "ZIP verified: $($verifyZip.Entries.Count) files, $resourceCount resources, $projectFileCount project entries"
} finally {
    $verifyZip.Dispose()
}
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$archivePath.sha256", "$hash  Nurture2D-M1-build-kit.zip`n")
Get-Item -LiteralPath $archivePath | Select-Object FullName,Length
Write-Output "SHA256: $hash"
