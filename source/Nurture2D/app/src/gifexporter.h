/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef GIFEXPORTER_H
#define GIFEXPORTER_H

#include <functional>
#include <QObject>
#include <QImage>
#include <QSize>
#include "pencilerror.h"
#include "gifexportdesc.h"

class Object;
class LayerCamera;

/**
 * Self-contained GIF encoder that renders animation frames using the existing
 * Object/LayerCamera pipeline and encodes them with the embedded gif.h library.
 *
 * Atomic write strategy: frames are written to outputPath + ".tmp"; on success
 * the file is renamed to outputPath; on failure or cancellation the .tmp file
 * is deleted so no partial file is left at the destination.
 */
class GifExporter : public QObject
{
    Q_OBJECT
public:
    explicit GifExporter(QObject* parent = nullptr);

    /**
     * Main entry point. Runs synchronously; call from a worker thread or
     * wrap in QtConcurrent for non-blocking UI.
     *
     * @param object     The animation object to render.
     * @param desc       Export parameters (path, size, frame range, fps, loop, transparency).
     * @param camera     The camera layer to use for view transforms.
     * @param progressCb Called with percent complete [0..100] after each frame.
     * @param cancelCb   Called each frame; return true to cancel the export.
     * @return Status::OK on success, Status::CANCELED if cancelled, Status::FAIL on error.
     */
    Status run(const Object* object,
               const GifExportDesc& desc,
               const LayerCamera* camera,
               std::function<void(int)> progressCb,
               std::function<bool()>    cancelCb);

    /** Request cancellation from another thread. */
    void cancel();

signals:
    void progressChanged(int percent);

private:
    /**
     * Renders a single animation frame to a QImage.
     *
     * @param object       The animation object.
     * @param camera       The camera layer.
     * @param frameNumber  1-based frame number to render.
     * @param size         Output image dimensions.
     * @param transparency If true, background is transparent (alpha=0).
     * @return The rendered frame as ARGB32 QImage.
     */
    QImage renderFrame(const Object* object,
                       const LayerCamera* camera,
                       int frameNumber,
                       const QSize& size,
                       bool transparency) const;

    std::atomic<bool> mCanceled { false };
};

#endif // GIFEXPORTER_H
