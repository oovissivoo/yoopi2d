#ifndef GIFEXPORTDESC_H
#define GIFEXPORTDESC_H

#include <QString>
#include <QSize>

/**
 * Plain data struct carrying all parameters needed to perform a GIF export.
 * Populated by CosmicExportGifPopup and consumed by GifExporter.
 */
struct GifExportDesc
{
    QString outputPath;       ///< Absolute path; must end in ".gif"
    QSize   size;             ///< Export dimensions in pixels
    int     startFrame  = 1;  ///< Inclusive, 1-based
    int     endFrame    = 1;  ///< Inclusive, already clamped to fps*10 cap
    int     fps         = 12; ///< Frames per second from PlaybackManager
    bool    loop        = true;  ///< true → loop count 0 (infinite), false → loop count 1
    bool    transparency = false; ///< true → map alpha=0 pixels to GIF transparency index
};

#endif // GIFEXPORTDESC_H
