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

#include "gifexporter.h"

#include <cmath>
#include <cstdio>
#include <atomic>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QTransform>

#include "object.h"
#include "layercamera.h"
#include "gif.h"

GifExporter::GifExporter(QObject* parent)
    : QObject(parent)
{
}

Status GifExporter::run(const Object* object,
                        const GifExportDesc& desc,
                        const LayerCamera* camera,
                        std::function<void(int)> progressCb,
                        std::function<bool()>    cancelCb)
{
    Q_ASSERT(object);
    Q_ASSERT(camera);

    mCanceled.store(false);

    // --- Defensive frame cap: at most fps * 10 frames (10-second limit) ---
    const int maxFrames = desc.fps * 10;
    const int startFrame = desc.startFrame;
    const int endFrame   = qMin(desc.endFrame, startFrame + maxFrames - 1);
    const int frameCount = endFrame - startFrame + 1;

    if (frameCount <= 0)
    {
        return Status(Status::INVALID_ARGUMENT,
                      tr("Invalid frame range: startFrame=%1, endFrame=%2")
                          .arg(startFrame).arg(endFrame));
    }

    const QSize  exportSize  = desc.size;
    const int    fps         = qMax(1, desc.fps);
    const bool   loop        = desc.loop;
    const bool   transparency = desc.transparency;

    // Per-frame delay in centiseconds (GIF unit)
    const uint32_t frameDelay = static_cast<uint32_t>(qRound(100.0 / fps));

    // Atomic write: encode to a .tmp file, rename on success
    const QString tmpPath = desc.outputPath + QStringLiteral(".tmp");

    // Clean up any stale .tmp from a previous failed run
    QFile::remove(tmpPath);

    // --- Open GIF writer ---
    // Pass loopCount directly to GifBegin:
    //   loop=true  → loopCount=0 (infinite)
    //   loop=false → loopCount=0xFFFF (sentinel: omit Netscape extension = play once)
    // This eliminates the fragile post-write byte-patching approach entirely.
    const uint16_t loopCount = loop ? 0 : 0xFFFF;

    GifWriter writer = {};
    if (!GifBegin(&writer, tmpPath.toLocal8Bit().constData(),
                  static_cast<uint32_t>(exportSize.width()),
                  static_cast<uint32_t>(exportSize.height()),
                  frameDelay,
                  8,        // bitDepth
                  false,    // dither
                  loopCount))
    {
        return Status(Status::FAIL,
                      tr("Failed to open temporary GIF file for writing: %1").arg(tmpPath));
    }

    // --- Frame encode loop ---
    bool encodeOk = true;
    for (int frame = startFrame; frame <= endFrame; ++frame)
    {
        // Check for cancellation
        if (mCanceled.load() || (cancelCb && cancelCb()))
        {
            GifEnd(&writer);
            QFile::remove(tmpPath);
            return Status::CANCELED;
        }

        // Render the frame
        QImage img = renderFrame(object, camera, frame, exportSize, transparency);
        if (img.isNull())
        {
            encodeOk = false;
            break;
        }

        // gif.h expects RGBA8 data.
        // Convert to Format_RGBA8888 so the byte order is R,G,B,A as gif.h expects.
        QImage rgbaImg = img.convertToFormat(QImage::Format_RGBA8888);

        // Transparency: gif.h uses index 0 (kGifTransIndex=0) as the transparent color.
        // When transparency is enabled, ensure fully-transparent pixels (alpha==0) have
        // RGB set to (0,0,0) so they map to palette index 0.
        if (transparency)
        {
            const int pixelCount = rgbaImg.width() * rgbaImg.height();
            uint8_t* pixels = rgbaImg.bits();
            for (int i = 0; i < pixelCount; ++i)
            {
                uint8_t* px = pixels + i * 4;
                if (px[3] == 0)
                {
                    px[0] = 0; // R
                    px[1] = 0; // G
                    px[2] = 0; // B
                }
            }
        }

        if (!GifWriteFrame(&writer,
                           rgbaImg.constBits(),
                           static_cast<uint32_t>(exportSize.width()),
                           static_cast<uint32_t>(exportSize.height()),
                           frameDelay))
        {
            encodeOk = false;
            break;
        }

        // Report progress
        const int percent = static_cast<int>(
            100.0 * (frame - startFrame + 1) / frameCount);
        if (progressCb)
            progressCb(percent);
        emit progressChanged(percent);
    }

    GifEnd(&writer);

    if (!encodeOk)
    {
        QFile::remove(tmpPath);
        return Status(Status::FAIL,
                      tr("An error occurred while encoding the GIF."));
    }

    // --- Atomic rename: .tmp → final destination ---
    // Remove any existing file at the destination first (QFile::rename fails if target exists)
    QFile::remove(desc.outputPath);
    if (!QFile::rename(tmpPath, desc.outputPath))
    {
        QFile::remove(tmpPath);
        return Status(Status::FAIL,
                      tr("Failed to move temporary GIF file to destination: %1").arg(desc.outputPath));
    }

    return Status::OK;
}

void GifExporter::cancel()
{
    mCanceled.store(true);
}

QImage GifExporter::renderFrame(const Object* object,
                                const LayerCamera* camera,
                                int frameNumber,
                                const QSize& size,
                                bool transparency) const
{
    // Create the base image with the correct background
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    QColor bgColor = Qt::white;
    if (transparency)
        bgColor.setAlpha(0);
    image.fill(bgColor);

    // Set up the painter with the camera transform, matching the pattern used
    // by MovieExporter::generateGif and Object::exportIm
    QPainter painter(&image);

    const QSize camSize = camera->getViewSize();
    QTransform centralizeCamera;
    centralizeCamera.translate(camSize.width() / 2.0, camSize.height() / 2.0);

    const QTransform view = camera->getViewAtFrame(frameNumber);
    painter.setWorldTransform(view * centralizeCamera);
    painter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));

    object->paintImage(painter, frameNumber, false /* no background */, true /* antialiasing */);
    painter.end();

    return image;
}


