/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "bitmapimage.h"

#include <cmath>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainterPath>
#include <QDataStream>
#include "util.h"
#include "qminiz.h"

#include "blitrect.h"
#include "tile.h"
#include "tiledbuffer.h"

static qreal bmCross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

static QString bmHybridSidecarPath(const QString& imageFilename)
{
    if (imageFilename.isEmpty()) {
        return QString();
    }
    QFileInfo info(imageFilename);
    if (!info.exists() && info.absolutePath().isEmpty()) {
        return QString();
    }
    return info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".hyb");
}

static bool bmSegmentIntersection(const QPointF& p1, const QPointF& p2, const QPointF& q1, const QPointF& q2, QPointF& out)
{
    QPointF r = p2 - p1;
    QPointF s = q2 - q1;
    qreal denom = bmCross(r, s);
    if (qFuzzyIsNull(denom)) {
        return false;
    }
    QPointF qp = q1 - p1;
    qreal t = bmCross(qp, s) / denom;
    qreal u = bmCross(qp, r) / denom;
    if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) {
        return false;
    }
    out = p1 + t * r;
    return true;
}

static bool bmHasNearbyPoint(const QVector<QPointF>& pts, const QPointF& p, qreal eps2)
{
    for (const QPointF& v : pts) {
        qreal dx = v.x() - p.x();
        qreal dy = v.y() - p.y();
        if (dx * dx + dy * dy <= eps2) {
            return true;
        }
    }
    return false;
}

static bool bmEnsureFileExtractedFromWorkspace(const QString& absFilePath)
{
    if (absFilePath.isEmpty() || QFile::exists(absFilePath)) {
        return true;
    }

    QFileInfo fi(absFilePath);
    if (!fi.isAbsolute()) {
        return false;
    }

    // We expect files to live under: <AppLocalDataLocation>/workspaces/<hash>/root/...
    // Find the "root" directory purely from the path string (do not rely on directories existing).
    const QString cleanedAbs = QDir::cleanPath(fi.absoluteFilePath()).replace('\\', '/');
    QString rootPath;
    {
        const QString marker = QStringLiteral("/root/");
        const int idx = cleanedAbs.indexOf(marker);
        if (idx >= 0) {
            rootPath = cleanedAbs.left(idx + QStringLiteral("/root").size());
        } else if (cleanedAbs.endsWith(QStringLiteral("/root"))) {
            rootPath = cleanedAbs;
        }
    }

    if (rootPath.isEmpty())
    {
        // Fallback for non-standard paths: walk up to a directory named "root".
        QDir dir = fi.absoluteDir();
        while (!dir.isRoot())
        {
            if (dir.dirName() == QStringLiteral("root")) {
                rootPath = dir.absolutePath();
                break;
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    if (rootPath.isEmpty()) {
        return false;
    }

    const QString workspaceDir = QFileInfo(rootPath).dir().absolutePath();
    const QString sourcePathPath = QDir(workspaceDir).filePath(QStringLiteral("source_path"));

    QFile sp(sourcePathPath);
    if (!sp.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QString n2dPath = QString::fromUtf8(sp.readLine()).trimmed();
    sp.close();
    if (n2dPath.isEmpty()) {
        return false;
    }

    const QString relInZip = QDir(rootPath).relativeFilePath(absFilePath).replace('\\', '/');
    Status st = MiniZ::uncompressFile(n2dPath, relInZip, absFilePath);
    return st.ok() && QFile::exists(absFilePath);
}

bool BitmapImage::ensureFileExtractedFromWorkspace(const QString& absFilePath)
{
    return bmEnsureFileExtractedFromWorkspace(absFilePath);
}

BitmapImage::BitmapImage()
{
}

BitmapImage::BitmapImage(const BitmapImage& a) : KeyFrame(a)
{
    mBounds = a.mBounds;
    mMinBound = a.mMinBound;
    mEnableAutoCrop = a.mEnableAutoCrop;
    mOpacity = a.mOpacity;
    mImage = a.mImage;
    mHybridStrokes = a.mHybridStrokes;
}

BitmapImage::BitmapImage(const QRect& rectangle, const QColor& color)
{
    mBounds = rectangle;
    mImage = QImage(mBounds.size(), QImage::Format_ARGB32_Premultiplied);
    mImage.fill(color.rgba());
    mMinBound = false;
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QImage& image)
{
    mBounds = QRect(topLeft, image.size());
    mMinBound = true;
    mImage = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QString& path)
    : BitmapImage(topLeft, path, true)
{
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QString& path, bool loadNow)
{
    setFileName(path);
    mImage = QImage();

    mBounds = QRect(topLeft, QSize(-1, 0));
    mMinBound = true;
    setModified(false);

    if (loadNow)
    {
        loadFile();
    }
    else
    {
        if (!fileName().isEmpty())
        {
            QImageReader reader(fileName());
            const QSize sz = reader.size();
            if (sz.isValid()) {
                mBounds.setSize(sz);
            }
        }
    }
}

BitmapImage::~BitmapImage()
{
}

void BitmapImage::setImage(QImage* img)
{
    Q_ASSERT(img && img->format() == QImage::Format_ARGB32_Premultiplied);
    mImage = *img;
    mMinBound = false;
    invalidateLOD();

    modification();
}

BitmapImage& BitmapImage::operator=(const BitmapImage& a)
{
    if (this == &a)
    {
        return *this; // a self-assignment
    }

    KeyFrame::operator=(a);
    mBounds = a.mBounds;
    mMinBound = a.mMinBound;
    mOpacity = a.mOpacity;
    mImage = a.mImage;
    mHybridStrokes = a.mHybridStrokes;
    invalidateLOD();
    modification();
    return *this;
}

BitmapImage* BitmapImage::clone() const
{
    BitmapImage* b = new BitmapImage(*this);
    b->setFileName(""); // don't link to the file of the source bitmap image

    const bool validKeyFrame = !fileName().isEmpty();
    if (validKeyFrame && !isModified())
    {
        bmEnsureFileExtractedFromWorkspace(fileName());
        if (!QFile::exists(fileName())) {
            return b;
        }
        // This bitmapImage is temporarily unloaded.
        // since it's not in the memory, we need to copy the linked png file to prevent data loss.
        QFileInfo finfo(fileName());
        Q_ASSERT(finfo.isAbsolute());

        QString newFilePath;
        do
        {
            newFilePath = QString("%1/temp-%2.%3")
                .arg(finfo.canonicalPath())
                .arg(uniqueString(12))
                .arg(finfo.suffix());
        }
        while (QFile::exists(newFilePath));

        b->setFileName(newFilePath);
        bool ok = QFile::copy(fileName(), newFilePath);
        Q_ASSERT(ok);
        qDebug() << "COPY>" << fileName();
    }
    return b;
}

void BitmapImage::loadFile()
{
    if (!fileName().isEmpty() && !isLoaded())
    {
        bmEnsureFileExtractedFromWorkspace(fileName());
        mImage = QImage(fileName()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        mBounds.setSize(mImage.size());
        mMinBound = false;

        // Load hybrid stroke sidecar if present
        const QString sidecarPath = bmHybridSidecarPath(fileName());
        mHybridStrokes.clear();
        if (!sidecarPath.isEmpty()) {
            if (!QFile::exists(sidecarPath)) {
                bmEnsureFileExtractedFromWorkspace(sidecarPath);
            }
            QFile f(sidecarPath);
            if (f.exists() && f.open(QIODevice::ReadOnly)) {
                QDataStream in(&f);
                in.setVersion(QDataStream::Qt_5_6);
                quint32 magic = 0;
                quint32 version = 0;
                in >> magic >> version;
                const quint32 kMagic = 0x4E324448; // 'N2DH'
                if (magic == kMagic && (version == 1u)) {
                    quint32 strokeCount = 0;
                    in >> strokeCount;
                    mHybridStrokes.clear();
                    mHybridStrokes.reserve(static_cast<int>(strokeCount));
                    for (quint32 si = 0; si < strokeCount && in.status() == QDataStream::Ok; ++si) {
                        quint32 pointCount = 0;
                        in >> pointCount;
                        if (in.status() != QDataStream::Ok || pointCount < 2u) {
                            // Skip invalid stroke
                            continue;
                        }
                        QVector<QPointF> pts;
                        pts.reserve(static_cast<int>(pointCount));
                        for (quint32 pi = 0; pi < pointCount; ++pi) {
                            qreal x = 0.0, y = 0.0;
                            in >> x >> y;
                            if (in.status() != QDataStream::Ok) {
                                break;
                            }
                            pts.append(QPointF(x, y));
                        }
                        if (in.status() != QDataStream::Ok || pts.size() < 2) {
                            break;
                        }
                        qreal baseWidth = 0.0;
                        in >> baseWidth;
                        if (in.status() != QDataStream::Ok) {
                            break;
                        }
                        quint32 ipCount = 0;
                        in >> ipCount;
                        if (in.status() != QDataStream::Ok) {
                            break;
                        }
                        QVector<QPointF> ips;
                        ips.reserve(static_cast<int>(ipCount));
                        for (quint32 ii = 0; ii < ipCount; ++ii) {
                            qreal x = 0.0, y = 0.0;
                            in >> x >> y;
                            if (in.status() != QDataStream::Ok) {
                                break;
                            }
                            ips.append(QPointF(x, y));
                        }
                        if (in.status() != QDataStream::Ok) {
                            break;
                        }

                        HybridVectorStroke stroke;
                        stroke.points = pts;
                        stroke.baseWidth = baseWidth;
                        stroke.intersectionPoints = ips;

                        QRectF b(pts.first(), pts.first());
                        for (int i = 1; i < pts.size(); ++i) {
                            const QPointF& p = pts[i];
                            if (p.x() < b.left()) b.setLeft(p.x());
                            if (p.x() > b.right()) b.setRight(p.x());
                            if (p.y() < b.top()) b.setTop(p.y());
                            if (p.y() > b.bottom()) b.setBottom(p.y());
                        }
                        stroke.bounds = b.normalized();
                        mHybridStrokes.append(stroke);
                    }
                }
            }
        }
    }
}

void BitmapImage::unloadFile()
{
    if (isModified() == false)
    {
        mImage = QImage();
        mLODPyramid.clear();
        mLODValid = false;
        mHybridStrokes.clear();
    }
}

bool BitmapImage::isLoaded() const
{
    return mImage.width() == mBounds.width();
}

quint64 BitmapImage::memoryUsage()
{
    if (!mImage.isNull())
    {
        return imageSize(mImage);
    }
    return 0;
}

void BitmapImage::paintImage(QPainter& painter)
{
    painter.drawImage(mBounds.topLeft(), *image());
}

void BitmapImage::paintImage(QPainter& painter, QImage& image, QRect sourceRect, QRect destRect)
{
    painter.drawImage(QRect(mBounds.topLeft(), destRect.size()),
                      image,
                      sourceRect);
}

QImage* BitmapImage::image()
{
    loadFile();
    return &mImage;
}

BitmapImage BitmapImage::copy()
{
    return BitmapImage(mBounds.topLeft(), *image());
}

BitmapImage BitmapImage::copy(QRect rectangle)
{
    if (rectangle.isEmpty() || mBounds.isEmpty()) return BitmapImage();

    QRect intersection2 = rectangle.translated(-mBounds.topLeft());

    BitmapImage result(rectangle.topLeft(), image()->copy(intersection2));
    return result;
}

void BitmapImage::paste(BitmapImage* bitmapImage, QPainter::CompositionMode cm)
{
    if(bitmapImage->width() <= 0 || bitmapImage->height() <= 0)
    {
        return;
    }

    setCompositionModeBounds(bitmapImage, cm);

    QImage* image2 = bitmapImage->image();

    QPainter painter(image());
    painter.setCompositionMode(cm);
    painter.drawImage(bitmapImage->mBounds.topLeft() - mBounds.topLeft(), *image2);
    painter.end();

    modification();
}

void BitmapImage::paste(const TiledBuffer* tiledBuffer, QPainter::CompositionMode cm)
{
    if(tiledBuffer->bounds().width() <= 0 || tiledBuffer->bounds().height() <= 0)
    {
        return;
    }
    extend(tiledBuffer->bounds());

    QPainter painter(image());

    painter.setCompositionMode(cm);
    auto const tiles = tiledBuffer->tiles();
    for (const Tile* item : tiles) {
        const QPixmap& tilePixmap = item->pixmap();
        const QPoint& tilePos = item->pos();
        painter.drawPixmap(tilePos-mBounds.topLeft(), tilePixmap);
    }
    painter.end();

    modification();
}

void BitmapImage::moveTopLeft(QPoint point)
{
    mBounds.moveTopLeft(point);
    // Size is unchanged so there is no need to update mBounds
    modification();
}

void BitmapImage::transform(QRect newBoundaries, bool smoothTransform)
{
    mBounds = newBoundaries;
    newBoundaries.moveTopLeft(QPoint(0, 0));
    QImage newImage(mBounds.size(), QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&newImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(newImage.rect(), QColor(0, 0, 0, 0));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(newBoundaries, *image());
    painter.end();
    mImage = newImage;

    modification();
}

BitmapImage BitmapImage::transformed(QRect selection, QTransform transform, bool smoothTransform)
{
    Q_ASSERT(!selection.isEmpty());

    BitmapImage selectedPart = copy(selection);

    // Get the transformed image
    QImage transformedImage;
    if (smoothTransform)
    {
        transformedImage = selectedPart.image()->transformed(transform, Qt::SmoothTransformation);
    }
    else
    {
        transformedImage = selectedPart.image()->transformed(transform);
    }
    return BitmapImage(transform.mapRect(selection).normalized().topLeft(), transformedImage);
}

BitmapImage BitmapImage::transformed(QRect newBoundaries, bool smoothTransform)
{
    BitmapImage transformedImage(newBoundaries, QColor(0, 0, 0, 0));
    QPainter painter(transformedImage.image());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    newBoundaries.moveTopLeft(QPoint(0, 0));
    painter.drawImage(newBoundaries, *image());
    painter.end();
    return transformedImage;
}

/** Update image bounds.
 *
 *  @param[in] newBoundaries the new bounds
 *
 *  Sets this image's bounds to rectangle.
 *  Modifies mBounds and crops mImage.
 */
void BitmapImage::updateBounds(QRect newBoundaries)
{
    // Check to make sure changes actually need to be made
    if (mBounds == newBoundaries) return;

    QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
    newImage.fill(Qt::transparent);
    if (!newImage.isNull())
    {
        QPainter painter(&newImage);
        painter.drawImage(mBounds.topLeft() - newBoundaries.topLeft(), mImage);
        painter.end();
    }
    mImage = newImage;
    mBounds = newBoundaries;
    mMinBound = false;

    modification();
}

void BitmapImage::extend(const QPoint &p)
{
    if (!mBounds.contains(p))
    {
        extend(QRect(p, QSize(1, 1)));
    }
}

void BitmapImage::extend(QRect rectangle)
{
    if (rectangle.width() <= 0) rectangle.setWidth(1);
    if (rectangle.height() <= 0) rectangle.setHeight(1);
    if (mBounds.contains(rectangle))
    {
        // Do nothing
    }
    else
    {
        QRect newBoundaries = mBounds.united(rectangle).normalized();
        QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
        newImage.fill(Qt::transparent);
        if (newImage.isNull()) {
            // Allocation failed — keep existing image, don't corrupt state
            qWarning() << "BitmapImage::extend: QImage allocation failed for size" << newBoundaries.size();
            return;
        }
        if (!newImage.isNull())
        {
            QPainter painter(&newImage);
            if (!mImage.isNull()) {
                painter.drawImage(mBounds.topLeft() - newBoundaries.topLeft(), *image());
            }
            painter.end();
        }
        mImage = newImage;
        mBounds = newBoundaries;

        modification();
    }
}

/** Updates the bounds after a drawImage operation with the composition mode cm.
 *
 *  @param[in] source The source image used for the drawImage call.
 *  @param[in] cm The composition mode that will be used for the draw image
 *
 *  @see BitmapImage::setCompositionModeBounds(BitmapImage, QPainter::CompositionMode)
 */
void BitmapImage::setCompositionModeBounds(BitmapImage *source, QPainter::CompositionMode cm)
{
    if (source)
    {
        setCompositionModeBounds(source->mBounds, source->mMinBound, cm);
    }
}

/** Updates the bounds after a draw operation with the composition mode cm.
 *
 * @param[in] sourceBounds The bounds of the source used for drawcall.
 * @param[in] isSourceMinBounds Is sourceBounds the minimal bounds for the source image
 * @param[in] cm The composition mode that will be used for the draw image
 *
 * For a call to draw image of a QPainter (initialized with mImage) with an argument
 * of source, this function intelligently calculates the bounds. It will attempt to
 * preserve minimum bounds based on the composition mode.
 *
 * This works baed on the principle that some minimal bounds can be determined
 * solely by the minimal bounds of this and source, depending on the value of cm.
 * Some composition modes only expand, or have no affect on the bounds.
 *
 * @warning The draw operation described by the arguments of this
 *          function needs to be called after this function is run,
 *          or the bounds will be out of sync. If mBounds is null,
 *          no draw operation needs to be performed.
 */
void BitmapImage::setCompositionModeBounds(QRect sourceBounds, bool isSourceMinBounds, QPainter::CompositionMode cm)
{
    QRect newBoundaries;
    switch(cm)
    {
    case QPainter::CompositionMode_Destination:
    case QPainter::CompositionMode_SourceAtop:
        // The Destination and SourceAtop modes
        // do not change the bounds from destination.
        newBoundaries = mBounds;
        // mMinBound remains the same
        break;
    case QPainter::CompositionMode_SourceIn:
    case QPainter::CompositionMode_DestinationIn:
    case QPainter::CompositionMode_Clear:
    case QPainter::CompositionMode_DestinationOut:
        // The bounds of the result of SourceIn, DestinationIn, Clear, and DestinationOut
        // modes are no larger than the destination bounds
        newBoundaries = mBounds;
        mMinBound = false;
        break;
    default:
        // If it's not one of the above cases, create a union of the two bounds.
        // This contains the minimum bounds, if both the destination and source
        // use their respective minimum bounds.
        newBoundaries = mBounds.united(sourceBounds);
        mMinBound = mMinBound && isSourceMinBounds;
    }

    updateBounds(newBoundaries);
}

/** Removes any transparent borders by reducing the boundaries.
 *
 *  This function reduces the bounds of an image until the top and
 *  bottom rows, and the left and right columns of pixels each
 *  contain at least one pixel with a non-zero alpha value
 *  (i.e. non-transparent pixel). Both mBounds and
 *  the size of #mImage are updated.
 *
 *  @pre mBounds.size() == mImage->size()
 *  @post Either the first and last rows and columns all contain a
 *        pixel with alpha > 0 or mBounds.isEmpty() == true
 *  @post isMinimallyBounded() == true
 */
void BitmapImage::autoCrop()
{
    if (!mEnableAutoCrop) return;
    if (mBounds.isEmpty()) return; // Exit if current bounds are null
    if (mImage.isNull()) return;

    Q_ASSERT(mBounds.size() == mImage.size());

    // Exit if already min bounded
    if (mMinBound) return;

    // Get image properties
    const int width = mImage.width();

    // Relative top and bottom row indices (inclusive)
    int relTop = 0;
    int relBottom = mBounds.height() - 1;

    // Check top row
    bool isEmpty = true; // Used to track if a non-transparent pixel has been found
    while (isEmpty && relTop <= relBottom) // Loop through rows
    {
        // Point cursor to the first pixel in the current top row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relTop));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if (qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relTop,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            cursor++;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, increase relTop
            // to remove the empty row from the top of the bounding box
            ++relTop;
        }
    }

    // Check bottom row
    isEmpty = true; // Reset isEmpty
    while (isEmpty && relBottom >= relTop) // Loop through rows
    {
        // Point cursor to the first pixel in the current bottom row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relBottom));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if(qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relBottom,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            ++cursor;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, decrease relBottom
            // to remove the empty row from the bottom of the bounding box
            --relBottom;
        }
    }

    // Relative left and right column indices (inclusive)
    int relLeft = 0;
    int relRight = mBounds.width()-1;

    // Check left row
    isEmpty = (relBottom >= relTop); // Check left only when
    while (isEmpty && relBottom >= relTop && relLeft <= relRight) // Loop through columns
    {
        // Point cursor to the pixel at row relTop and column relLeft
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relTop)) + relLeft;
        // Loop through pixels in column
        // Note: we only need to loop from relTop to relBottom (inclusive)
        //       not the full image height, because rows 0 to relTop-1 and
        //       relBottom+1 to mBounds.height() have already been
        //       confirmed to contain only transparent pixels
        for (int row = relTop; row <= relBottom; row++)
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if(qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in column relLeft,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to next pixel in the column
            // Increment by width because the data is in row-major order
            cursor += width;
        }
        if (isEmpty)
        {
            // If the column we just checked was empty, increase relLeft
            // to remove the empty column from the left of the bounding box
            ++relLeft;
        }
    }

    // Check right row
    isEmpty = (relBottom >= relTop); // Reset isEmpty
    while (isEmpty && relRight >= relLeft) // Loop through columns
    {
        // Point cursor to the pixel at row relTop and column relRight
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relTop)) + relRight;
        // Loop through pixels in column
        // Note: we only need to loop from relTop to relBottom (inclusive)
        //       not the full image height, because rows 0 to relTop-1 and
        //       relBottom+1 to mBounds.height()-1 have already been
        //       confirmed to contain only transparent pixels
        for (int row = relTop; row <= relBottom; row++)
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if(qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in column relRight,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to next pixel in the column
            // Increment by width because the data is in row-major order
            cursor += width;
        }
        if (isEmpty)
        {
            // If the column we just checked was empty, increase relRight
            // to remove the empty column from the left of the bounding box
            --relRight;
        }
    }

    //qDebug() << "Original" << mBounds;
    //qDebug() << "Autocrop" << relLeft << relTop << relRight - mBounds.width() + 1 << relBottom - mBounds.height() + 1;
    // Update mBounds and mImage if necessary
    updateBounds(mBounds.adjusted(relLeft, relTop, relRight - mBounds.width() + 1, relBottom - mBounds.height() + 1));

    //qDebug() << "New bounds" << mBounds;

    mMinBound = true;
}

QRgb BitmapImage::pixel(int x, int y)
{
    return pixel(QPoint(x, y));
}

QRgb BitmapImage::pixel(QPoint p)
{
    QRgb result = qRgba(0, 0, 0, 0); // black
    if (mBounds.contains(p))
        result = image()->pixel(p - mBounds.topLeft());
    return result;
}

void BitmapImage::setPixel(int x, int y, QRgb color)
{
    setPixel(QPoint(x, y), color);
}

void BitmapImage::setPixel(QPoint p, QRgb color)
{
    setCompositionModeBounds(QRect(p, QSize(1,1)), true, QPainter::CompositionMode_SourceOver);
    if (mBounds.contains(p))
    {
        image()->setPixel(p - mBounds.topLeft(), color);
    }
    modification();
}

void BitmapImage::fillNonAlphaPixels(const QRgb color)
{
    if (mBounds.isEmpty()) { return; }

    BitmapImage fill(bounds(), color);
    paste(&fill, QPainter::CompositionMode_SourceIn);
}

void BitmapImage::drawLine(QPointF P1, QPointF P2, QPen pen, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = 2 + pen.width();
    setCompositionModeBounds(QRect(P1.toPoint(), P2.toPoint()).normalized().adjusted(-width, -width, width, width), true, cm);
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.drawLine(P1 - mBounds.topLeft(), P2 - mBounds.topLeft());
        painter.end();
    }
    modification();
}

void BitmapImage::drawRect(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - mBounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - mBounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);

        // Adjust the brush rectangle to be bigger than the bounds itself,
        // otherwise there will be artifacts shown in some cases when smudging
        painter.drawRect(rectangle.translated(-mBounds.topLeft()).adjusted(-1, -1, 1, 1));
        painter.end();
    }
    modification();
}

void BitmapImage::drawEllipse(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - mBounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - mBounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());

        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setCompositionMode(cm);
        painter.drawEllipse(rectangle.translated(-mBounds.topLeft()));
        painter.end();
    }
    modification();
}

void BitmapImage::drawPath(QPainterPath path, QPen pen, QBrush brush,
                           QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    // qreal inc = 1.0 + width / 20.0;

    setCompositionModeBounds(path.controlPointRect().adjusted(-width, -width, width, width).toRect(), true, cm);

    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setTransform(QTransform().translate(-mBounds.left(), -mBounds.top()));
        painter.setWorldMatrixEnabled(true);
        if (path.length() > 0)
        {
            /*
            for (int pt = 0; pt < path.elementCount() - 1; pt++)
            {
                qreal dx = path.elementAt(pt + 1).x - path.elementAt(pt).x;
                qreal dy = path.elementAt(pt + 1).y - path.elementAt(pt).y;
                qreal m = sqrt(dx*dx + dy*dy);
                qreal factorx = dx / m;
                qreal factory = dy / m;
                for (float h = 0.f; h < m; h += inc)
                {
                    qreal x = path.elementAt(pt).x + factorx * h;
                    qreal y = path.elementAt(pt).y + factory * h;
                    painter.drawPoint(QPointF(x, y));
                }
            }
            */
            painter.drawPath( path );
        }
        else
        {
            // forces drawing when points are coincident (mousedown)
            painter.drawPoint(static_cast<int>(path.elementAt(0).x), static_cast<int>(path.elementAt(0).y));
        }
        painter.end();
    }
    modification();
}

BitmapImage* BitmapImage::scanToTransparent(BitmapImage *img, const int threshold, const bool redEnabled, const bool greenEnabled, const bool blueEnabled)
{
    Q_ASSERT(img != nullptr);

    QRgb rgba = img->constScanLine(img->left(), img->top());
    if (qAlpha(rgba) == 0)
        return img;

    for (int x = img->left(); x <= img->right(); x++)
    {
        for (int y = img->top(); y <= img->bottom(); y++)
        {
            rgba = img->constScanLine(x, y);

            if (qAlpha(rgba) == 0)
                break;

            const int grayValue = qGray(rgba);
            const int redValue = qRed(rgba);
            const int greenValue = qGreen(rgba);
            const int blueValue = qBlue(rgba);
            if (grayValue >= threshold)
            {   // IF Threshold or above
                img->scanLine(x, y, transp);
            }
            else if (redValue > greenValue + COLORDIFF &&
                     redValue > blueValue + COLORDIFF &&
                     redValue > grayValue + GRAYSCALEDIFF)
            {   // IF Red line
                if (redEnabled)
                {
                    img->scanLine(x, y, redline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (greenValue > redValue + COLORDIFF &&
                     greenValue > blueValue + COLORDIFF &&
                     greenValue > grayValue + GRAYSCALEDIFF)
            {   // IF Green line
                if (greenEnabled)
                {
                    img->scanLine(x, y, greenline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (blueValue > redValue + COLORDIFF &&
                     blueValue > greenValue + COLORDIFF &&
                     blueValue > grayValue + GRAYSCALEDIFF)
            {   // IF Blue line
                if (blueEnabled)
                {
                    img->scanLine(x, y, blueline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else
            {   // okay, so it is in grayscale graduation area
                if (grayValue >= LOW_THRESHOLD)
                {
                    const qreal factor = static_cast<qreal>(threshold - grayValue) / static_cast<qreal>(threshold - LOW_THRESHOLD);
                    img->scanLine(x , y, qRgba(0, 0, 0, static_cast<int>(threshold * factor)));
                }
                else // grayValue < LOW_THRESHOLD
                {
                    img->scanLine(x , y, blackline);
                }
            }
        }
    }
    img->modification();
    return img;
}

Status BitmapImage::writeFile(const QString& filename)
{
    if (!mImage.isNull())
    {
        bool b = mImage.save(filename);
        if (!b) {
            return Status::FAIL;
        }

        // Persist hybrid strokes to sidecar file
        const QString sidecarPath = bmHybridSidecarPath(filename);
        if (!sidecarPath.isEmpty()) {
            if (mHybridStrokes.isEmpty()) {
                QFile::remove(sidecarPath);
            } else {
                QFile f(sidecarPath);
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    QDataStream out(&f);
                    out.setVersion(QDataStream::Qt_5_6);
                    const quint32 kMagic = 0x4E324448; // 'N2DH'
                    const quint32 kVersion = 1u;
                    out << kMagic << kVersion;
                    out << static_cast<quint32>(mHybridStrokes.size());
                    for (const HybridVectorStroke& stroke : mHybridStrokes) {
                        out << static_cast<quint32>(stroke.points.size());
                        for (const QPointF& p : stroke.points) {
                            out << static_cast<qreal>(p.x()) << static_cast<qreal>(p.y());
                        }
                        out << static_cast<qreal>(stroke.baseWidth);
                        out << static_cast<quint32>(stroke.intersectionPoints.size());
                        for (const QPointF& ip : stroke.intersectionPoints) {
                            out << static_cast<qreal>(ip.x()) << static_cast<qreal>(ip.y());
                        }
                    }
                } else {
                    qWarning() << "BitmapImage: failed to write hybrid sidecar" << sidecarPath;
                }
            }
        }

        return Status::OK;
    }

    if (!fileName().isEmpty() && !isModified())
    {
        bmEnsureFileExtractedFromWorkspace(fileName());
        if (QFile::exists(fileName()))
        {
            QDir outDir(QFileInfo(filename).absolutePath());
            if (!outDir.exists()) {
                outDir.mkpath(QStringLiteral("."));
            }

            QFile::remove(filename);
            if (!QFile::copy(fileName(), filename)) {
                return Status::FAIL;
            }

            const QString srcSidecarPath = bmHybridSidecarPath(fileName());
            const QString dstSidecarPath = bmHybridSidecarPath(filename);
            if (!dstSidecarPath.isEmpty()) {
                if (!srcSidecarPath.isEmpty()) {
                    if (!QFile::exists(srcSidecarPath)) {
                        bmEnsureFileExtractedFromWorkspace(srcSidecarPath);
                    }
                    if (QFile::exists(srcSidecarPath)) {
                        QFile::remove(dstSidecarPath);
                        QFile::copy(srcSidecarPath, dstSidecarPath);
                    } else {
                        QFile::remove(dstSidecarPath);
                    }
                } else {
                    QFile::remove(dstSidecarPath);
                }
            }

            return Status::OK;
        }
    }

    if (bounds().isEmpty())
    {
        QFile f(filename);
        if(f.exists())
        {
            bool b = f.remove();
            if (!b) {
                return Status::FAIL;
            }
        }

        // Remove any stale hybrid sidecar as well
        const QString sidecarPath = bmHybridSidecarPath(filename);
        if (!sidecarPath.isEmpty()) {
            QFile::remove(sidecarPath);
        }

        // The frame is likely empty, act like there's no file name
        // so we don't end up writing to it later.
        if (this->fileName() == filename) {
            setFileName("");
        }
    }
    return Status::SAFE;
}

void BitmapImage::clear()
{
    mImage = QImage(); // null image
    mBounds = QRect(0, 0, 0, 0);
    mMinBound = true;
    mHybridStrokes.clear();
    invalidateLOD();
    modification();
}

QRgb BitmapImage::constScanLine(int x, int y) const
{
    QRgb result = QRgb();
    if (mBounds.contains(x, y)) {
        result = *(reinterpret_cast<const QRgb*>(mImage.constScanLine(y - mBounds.top())) + x - mBounds.left());
    }
    return result;
}

void BitmapImage::scanLine(int x, int y, QRgb color)
{
    if (!mBounds.contains(x, y)) {
        return;
    }
    // Make sure color is premultiplied before calling
    *(reinterpret_cast<QRgb*>(image()->scanLine(y - mBounds.top())) + x - mBounds.left()) = color;
}

void BitmapImage::clear(QRect rectangle)
{
    QRect clearRectangle = mBounds.intersected(rectangle);
    clearRectangle.moveTopLeft(clearRectangle.topLeft() - mBounds.topLeft());

    setCompositionModeBounds(clearRectangle, true, QPainter::CompositionMode_Clear);

    QPainter painter(image());
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(clearRectangle, QColor(0, 0, 0, 0));
    painter.end();

    modification();
}

bool BitmapImage::floodFill(BitmapImage** replaceImage,
                            const BitmapImage* targetImage,
                            const QRect& cameraRect,
                            const QPoint& point,
                            const QRgb& fillColor,
                            int tolerance,
                            const int expandValue)
{
    // Fill region must be 1 pixel larger than the target image to fill regions on the edge connected only by transparent pixels
    const QRect& fillBounds = targetImage->mBounds.adjusted(-1, -1, 1, 1);
    // Respect camera/canvas bounds: if a cameraRect is provided, restrict the search to its intersection.
    QRect limited = (cameraRect.isValid() && !cameraRect.isEmpty()) ? cameraRect.intersected(fillBounds) : fillBounds;
    if (!limited.isValid() || limited.isEmpty()) {
        limited = fillBounds;
    }
    QRect maxBounds = limited.adjusted(-expandValue, -expandValue, expandValue, expandValue);
    // If the seed is outside the constrained area, abort to avoid filling entire canvas
    if (!maxBounds.contains(point)) {
        return false;
    }
    const int maxWidth = maxBounds.width(), left = maxBounds.left(), top = maxBounds.top();

    // Square tolerance for use with compareColor
    tolerance = static_cast<int>(qPow(tolerance, 2));

    QRect newBounds;
    bool *filledPixels = floodFillPoints(targetImage, maxBounds, point, tolerance, newBounds);

    QRect translatedSearchBounds = newBounds.translated(-maxBounds.topLeft());

    // The scanned bounds should take the expansion into account
    const QRect& expandRect = newBounds.adjusted(-expandValue, -expandValue, expandValue, expandValue);
    if (expandValue > 0) {
        newBounds = expandRect;
    }
    if (!maxBounds.contains(newBounds)) {
        newBounds = maxBounds;
    }
    translatedSearchBounds = newBounds.translated(-maxBounds.topLeft());

    if (expandValue > 0) {
        expandFill(filledPixels, translatedSearchBounds, maxBounds, expandValue);
    }

    *replaceImage = new BitmapImage(newBounds, Qt::transparent);

    // Fill all the found pixels
    for (int y = translatedSearchBounds.top(); y <= translatedSearchBounds.bottom(); y++)
    {
        for (int x = translatedSearchBounds.left(); x <= translatedSearchBounds.right(); x++)
        {
            const int index = y * maxWidth + x;
            if (!filledPixels[index])
            {
                continue;
            }
            (*replaceImage)->scanLine(x + left, y + top, fillColor);
        }
    }

    delete[] filledPixels;

    return true;
}

// Flood filling based on this scanline algorithm
// ----- http://lodev.org/cgtutor/floodfill.html
bool* BitmapImage::floodFillPoints(const BitmapImage* targetImage,
                                   const QRect& searchBounds,
                                   QPoint point,
                                   const int tolerance,
                                   QRect& newBounds)
{
    QRgb oldColor = targetImage->constScanLine(point.x(), point.y());
    oldColor = qRgba(qRed(oldColor), qGreen(oldColor), qBlue(oldColor), qAlpha(oldColor));

    // Preparations
    QList<QPoint> queue; // queue all the pixels of the filled area (as they are found)

    QPoint tempPoint;
    QScopedPointer< QHash<QRgb, bool> > cache(new QHash<QRgb, bool>());

    int xTemp = 0;
    bool spanLeft = false;
    bool spanRight = false;

    queue.append(point);
    // Preparations END

    bool *filledPixels = new bool[searchBounds.height()*searchBounds.width()]{};

    BlitRect blitBounds(point);
    while (!queue.empty())
    {
        tempPoint = queue.takeFirst();

        point.setX(tempPoint.x());
        point.setY(tempPoint.y());

        xTemp = point.x();

        int xCoord = xTemp - searchBounds.left();
        int yCoord = point.y() - searchBounds.top();

        if (filledPixels[yCoord*searchBounds.width()+xCoord]) continue;

        while (xTemp >= searchBounds.left() &&
               compareColor(targetImage->constScanLine(xTemp, point.y()), oldColor, tolerance, cache.data())) xTemp--;
        xTemp++;

        spanLeft = spanRight = false;
        while (xTemp <= searchBounds.right() &&
               compareColor(targetImage->constScanLine(xTemp, point.y()), oldColor, tolerance, cache.data()))
        {

            QPoint floodPoint = QPoint(xTemp, point.y());
            if (!blitBounds.contains(floodPoint)) {
                blitBounds.extend(floodPoint);
            }

            xCoord = xTemp - searchBounds.left();
            // This pixel is what we're going to fill later
            filledPixels[yCoord*searchBounds.width()+xCoord] = true;

            if (!spanLeft && (point.y() > searchBounds.top()) &&
                compareColor(targetImage->constScanLine(xTemp, point.y() - 1), oldColor, tolerance, cache.data())) {
                queue.append(QPoint(xTemp, point.y() - 1));
                spanLeft = true;
            }
            else if (spanLeft && (point.y() > searchBounds.top()) &&
                     !compareColor(targetImage->constScanLine(xTemp, point.y() - 1), oldColor, tolerance, cache.data())) {
                spanLeft = false;
            }

            if (!spanRight && point.y() < searchBounds.bottom() &&
                compareColor(targetImage->constScanLine(xTemp, point.y() + 1), oldColor, tolerance, cache.data())) {
                queue.append(QPoint(xTemp, point.y() + 1));
                spanRight = true;
            }
            else if (spanRight && point.y() < searchBounds.bottom() &&
                     !compareColor(targetImage->constScanLine(xTemp, point.y() + 1), oldColor, tolerance, cache.data())) {
                spanRight = false;
            }

            Q_ASSERT(queue.count() < (searchBounds.width() * searchBounds.height()));
            xTemp++;
        }
    }

    newBounds = blitBounds;

    return filledPixels;
}

/** Finds all pixels closest to the input color and applies the input color to the image
 *
 * An example:
 *
 * 0 is where the color was found
 * 1 is the distance from the nearest pixel of that color
 *
 * 211112
 * 100001
 * 100001
 * 211112
 *
 * @param bitmapImage: Image to search
 * @param searchColor: Color to find
 */
void BitmapImage::expandFill(bool* fillPixels, const QRect& searchBounds, const QRect& maxBounds, int expand) {

    const int maxWidth = maxBounds.width();
    const int length = maxBounds.height() * maxBounds.width();

    int* manhattanPoints = new int[length]{};

    // Fill points with max length, this is important because otherwise the filled pixels will include a border of the expanded area
    std::fill_n(manhattanPoints, length, searchBounds.width()+searchBounds.height());

    for (int y = searchBounds.top(); y <= searchBounds.bottom(); y++)
    {
        for (int x = searchBounds.left(); x <= searchBounds.right(); x++)
        {
            const int index = y*maxWidth+x;

            if (fillPixels[index]) {
                manhattanPoints[index] = 0;
                continue;
            }

            if (y > searchBounds.top()) {
                // the value will be the num of pixels away from y - 1 of the next position
                manhattanPoints[index] = qMin(manhattanPoints[index],
                                             manhattanPoints[(y - 1) * maxWidth+x] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
            if (x > searchBounds.left()) {
                // the value will be the num of pixels away from x - 1 of the next position
                manhattanPoints[index] = qMin(manhattanPoints[index],
                                             manhattanPoints[y*maxWidth+(x - 1)] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
        }
    }

    // traverse from bottom right to top left
    for (int y = searchBounds.bottom(); y >= searchBounds.top(); y--)
    {
        for (int x = searchBounds.right(); x >= searchBounds.left(); x--)
        {
            const int index = y*maxWidth+x;

            if (y + 1 < searchBounds.bottom()) {
                manhattanPoints[index] = qMin(manhattanPoints[index], manhattanPoints[(y + 1)*maxWidth+x] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
            if (x + 1 < searchBounds.right()) {
                manhattanPoints[index] = qMin(manhattanPoints[index], manhattanPoints[y*maxWidth+(x + 1)] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
        }
    }

    delete[] manhattanPoints;
}

// LOD (Level of Detail) implementation for performance optimization
void BitmapImage::invalidateLOD()
{
    mLODPyramid.clear();
    mLODValid = false;
}

void BitmapImage::buildLODPyramid() const
{
    if (mLODValid || mImage.isNull()) return;
    
    mLODPyramid.clear();
    
    // Build pyramid: level 0 = full res, level 1 = 0.5x, level 2 = 0.25x, etc.
    // Stop when either dimension is < 64 pixels
    QImage current = mImage;
    int level = 0;
    
    while (current.width() >= 64 && current.height() >= 64 && level < 4)
    {
        level++;
        // Halve the size using smooth scaling for quality
        QSize halfSize(qMax(1, current.width() / 2), qMax(1, current.height() / 2));
        QImage scaled = current.scaled(halfSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        mLODPyramid[level] = scaled;
        current = scaled;
    }
    
    mLODValid = true;
}

const QImage* BitmapImage::getLODImage(qreal scale) const
{
    // For scales >= 0.5, use full resolution
    if (scale >= 0.5 || mImage.isNull()) {
        return &mImage;
    }
    
    // Build LOD pyramid if needed
    if (!mLODValid) {
        buildLODPyramid();
    }
    
    // Select appropriate LOD level
    // level 1 = 0.25-0.5x, level 2 = 0.125-0.25x, etc.
    int targetLevel = 1;
    if (scale >= 0.25) targetLevel = 1;
    else if (scale >= 0.125) targetLevel = 2;
    else if (scale >= 0.0625) targetLevel = 3;
    else targetLevel = 4;
    
    // Find best available level (fallback to closest if target not available)
    for (int level = targetLevel; level >= 1; level--) {
        if (mLODPyramid.contains(level)) {
            return &mLODPyramid[level];
        }
    }
    
    // Fallback to full resolution
    return &mImage;
}

void BitmapImage::clearHybridStrokes()
{
    if (mHybridStrokes.isEmpty()) {
        return;
    }
    mHybridStrokes.clear();
    modification();
}

void BitmapImage::addHybridStroke(const QVector<QPointF>& points, qreal baseWidth)
{
    if (points.size() < 2) {
        return;
    }

    HybridVectorStroke stroke;
    stroke.points = points;
    stroke.baseWidth = baseWidth;

    QRectF bounds(points[0], points[0]);
    for (int i = 1; i < points.size(); ++i) {
        const QPointF& p = points[i];
        if (p.x() < bounds.left()) bounds.setLeft(p.x());
        if (p.x() > bounds.right()) bounds.setRight(p.x());
        if (p.y() < bounds.top()) bounds.setTop(p.y());
        if (p.y() > bounds.bottom()) bounds.setBottom(p.y());
    }
    stroke.bounds = bounds.normalized();

    const qreal eps2 = 0.5 * 0.5;

    for (int i = 0; i + 1 < points.size(); ++i) {
        const QPointF& a1 = points[i];
        const QPointF& a2 = points[i + 1];
        for (int j = i + 2; j + 1 < points.size(); ++j) {
            QPointF ip;
            if (bmSegmentIntersection(a1, a2, points[j], points[j + 1], ip)) {
                if (!bmHasNearbyPoint(stroke.intersectionPoints, ip, eps2)) {
                    stroke.intersectionPoints.append(ip);
                }
            }
        }
    }

    for (HybridVectorStroke& existing : mHybridStrokes) {
        if (!existing.bounds.intersects(stroke.bounds)) {
            continue;
        }
        const QVector<QPointF>& ep = existing.points;
        for (int i = 0; i + 1 < points.size(); ++i) {
            const QPointF& a1 = points[i];
            const QPointF& a2 = points[i + 1];
            for (int j = 0; j + 1 < ep.size(); ++j) {
                QPointF ip;
                if (bmSegmentIntersection(a1, a2, ep[j], ep[j + 1], ip)) {
                    if (!bmHasNearbyPoint(stroke.intersectionPoints, ip, eps2)) {
                        stroke.intersectionPoints.append(ip);
                    }
                    if (!bmHasNearbyPoint(existing.intersectionPoints, ip, eps2)) {
                        existing.intersectionPoints.append(ip);
                    }
                }
            }
        }
    }

    mHybridStrokes.append(stroke);
    modification();
}

void BitmapImage::pruneHybridStrokesInRect(const QRectF& canvasRect)
{
    if (mHybridStrokes.isEmpty()) {
        return;
    }
    if (canvasRect.isEmpty()) {
        return;
    }

    const QRectF region = canvasRect.normalized();
    bool modifiedStrokes = false;

    auto it = mHybridStrokes.begin();
    while (it != mHybridStrokes.end()) {
        if (it->bounds.intersects(region)) {
            it = mHybridStrokes.erase(it);
            modifiedStrokes = true;
        } else {
            ++it;
        }
    }

    if (modifiedStrokes) {
        modification();
    }
}

void BitmapImage::eraseHybridStrokesWithBrush(const QVector<QPointF>& eraseSamples, const QVector<qreal>& sampleRadii)
{
    if (mHybridStrokes.isEmpty()) {
        return;
    }
    if (eraseSamples.isEmpty()) {
        return;
    }

    const int sampleCount = eraseSamples.size();
    bool haveRadii = (sampleRadii.size() == sampleCount);
    qreal maxRadius = 0.0;
    QRectF eraseBounds(eraseSamples.first(), eraseSamples.first());
    for (int i = 0; i < sampleCount; ++i) {
        const QPointF& p = eraseSamples[i];
        if (p.x() < eraseBounds.left()) eraseBounds.setLeft(p.x());
        if (p.x() > eraseBounds.right()) eraseBounds.setRight(p.x());
        if (p.y() < eraseBounds.top()) eraseBounds.setTop(p.y());
        if (p.y() > eraseBounds.bottom()) eraseBounds.setBottom(p.y());
        qreal r = 0.0;
        if (haveRadii) {
            r = sampleRadii[i];
        } else if (!sampleRadii.isEmpty()) {
            r = sampleRadii.first();
        }
        if (r > maxRadius) {
            maxRadius = r;
        }
    }
    if (maxRadius <= 0.0) {
        return;
    }
    eraseBounds = eraseBounds.adjusted(-maxRadius, -maxRadius, maxRadius, maxRadius);

    auto pointErased = [&](const QPointF& pt) -> bool {
        for (int i = 0; i < sampleCount; ++i) {
            qreal radius = 0.0;
            if (haveRadii) {
                radius = sampleRadii[i];
            } else if (!sampleRadii.isEmpty()) {
                radius = sampleRadii.first();
            }
            if (radius <= 0.0) {
                continue;
            }
            const QPointF& sp = eraseSamples[i];
            qreal dx = pt.x() - sp.x();
            qreal dy = pt.y() - sp.y();
            qreal r2 = radius * radius;
            if (dx * dx + dy * dy <= r2) {
                return true;
            }
        }
        return false;
    };

    QVector<HybridVectorStroke> newStrokes;
    newStrokes.reserve(mHybridStrokes.size());

    // Use a maximum segment length tied to the brush radius so that any
    // erase dab intersecting a stroke has at least one nearby sample point
    // on the hybrid polyline, even if the original stroke points were
    // sparse. This avoids the need to "hit" existing control points
    // exactly for erasure to register.
    const qreal maxSegLen = qMax<qreal>(1.0, maxRadius * 0.5);

    for (const HybridVectorStroke& stroke : mHybridStrokes) {
        if (stroke.points.size() < 2) {
            continue;
        }
        if (!stroke.bounds.intersects(eraseBounds)) {
            newStrokes.append(stroke);
            continue;
        }

        // Densify the stroke so that segment lengths are bounded by
        // maxSegLen. This gives us additional intermediate samples where
        // the eraser can intersect the stroke between original points.
        QVector<QPointF> pts;
        pts.reserve(stroke.points.size() * 2);
        pts.append(stroke.points.first());
        for (int i = 0; i + 1 < stroke.points.size(); ++i) {
            const QPointF& a = stroke.points[i];
            const QPointF& b = stroke.points[i + 1];
            const qreal segLen = QLineF(a, b).length();
            int steps = 1;
            if (segLen > maxSegLen) {
                steps = qMax(1, int(std::ceil(segLen / maxSegLen)));
            }

            for (int s = 1; s <= steps; ++s) {
                const qreal t = qreal(s) / qreal(steps);
                QPointF p(a.x() + (b.x() - a.x()) * t,
                          a.y() + (b.y() - a.y()) * t);
                pts.append(p);
            }
        }

        QVector<QPointF> currentSegment;
        currentSegment.reserve(pts.size());

        for (int i = 0; i < pts.size(); ++i) {
            const QPointF& p = pts[i];
            if (pointErased(p)) {
                if (currentSegment.size() >= 2) {
                    HybridVectorStroke ns;
                    ns.points = currentSegment;
                    ns.baseWidth = stroke.baseWidth;
                    QRectF b(ns.points.first(), ns.points.first());
                    for (int k = 1; k < ns.points.size(); ++k) {
                        const QPointF& cp = ns.points[k];
                        if (cp.x() < b.left()) b.setLeft(cp.x());
                        if (cp.x() > b.right()) b.setRight(cp.x());
                        if (cp.y() < b.top()) b.setTop(cp.y());
                        if (cp.y() > b.bottom()) b.setBottom(cp.y());
                    }
                    ns.bounds = b.normalized();
                    newStrokes.append(ns);
                }
                currentSegment.clear();
            } else {
                currentSegment.append(p);
            }
        }

        if (currentSegment.size() >= 2) {
            HybridVectorStroke ns;
            ns.points = currentSegment;
            ns.baseWidth = stroke.baseWidth;
            QRectF b(ns.points.first(), ns.points.first());
            for (int k = 1; k < ns.points.size(); ++k) {
                const QPointF& cp = ns.points[k];
                if (cp.x() < b.left()) b.setLeft(cp.x());
                if (cp.x() > b.right()) b.setRight(cp.x());
                if (cp.y() < b.top()) b.setTop(cp.y());
                if (cp.y() > b.bottom()) b.setBottom(cp.y());
            }
            ns.bounds = b.normalized();
            newStrokes.append(ns);
        }
    }

    mHybridStrokes = newStrokes;
    modification();
}

void BitmapImage::collectHybridStrokesWithMask(const QImage& mask, const QPointF& origin, QVector<HybridVectorStroke>* outInsideSegments) const
{
    if (!outInsideSegments) {
        return;
    }
    if (mHybridStrokes.isEmpty()) {
        return;
    }
    if (mask.isNull() || mask.width() <= 0 || mask.height() <= 0) {
        return;
    }

    const QRectF maskRect(origin, mask.size());

    auto pointInsideMask = [&](const QPointF& pt) -> bool {
        if (!maskRect.contains(pt)) {
            return false;
        }
        const QPointF lp = pt - origin;
        const int x = static_cast<int>(std::floor(lp.x()));
        const int y = static_cast<int>(std::floor(lp.y()));
        if (x < 0 || y < 0 || x >= mask.width() || y >= mask.height()) {
            return false;
        }
        const QRgb a = mask.pixel(x, y);
        return qAlpha(a) != 0;
    };

    auto interpolateBoundaryPoint = [&](const QPointF& a, const QPointF& b, bool aInside) -> QPointF {
        Q_UNUSED(aInside);
        QPointF p0 = a;
        QPointF p1 = b;
        bool s0 = pointInsideMask(p0);
        for (int iter = 0; iter < 8; ++iter) {
            QPointF mid((p0.x() + p1.x()) * 0.5, (p0.y() + p1.y()) * 0.5);
            bool sm = pointInsideMask(mid);
            if (sm == s0) {
                p0 = mid;
            } else {
                p1 = mid;
            }
        }
        return p1;
    };

    for (const HybridVectorStroke& stroke : mHybridStrokes) {
        if (stroke.points.size() < 2) {
            continue;
        }

        if (!stroke.bounds.intersects(maskRect)) {
            continue;
        }

        const QVector<QPointF>& pts = stroke.points;
        QVector<QPointF> insideSegment;
        insideSegment.reserve(pts.size());

        auto flushInside = [&](QVector<QPointF>& seg) {
            if (seg.size() < 2) {
                seg.clear();
                return;
            }
            HybridVectorStroke ns;
            ns.points = seg;
            ns.baseWidth = stroke.baseWidth;
            QRectF b(ns.points.first(), ns.points.first());
            for (int k = 1; k < ns.points.size(); ++k) {
                const QPointF& cp = ns.points[k];
                if (cp.x() < b.left()) b.setLeft(cp.x());
                if (cp.x() > b.right()) b.setRight(cp.x());
                if (cp.y() < b.top()) b.setTop(cp.y());
                if (cp.y() > b.bottom()) b.setBottom(cp.y());
            }
            ns.bounds = b.normalized();
            outInsideSegments->append(ns);
            seg.clear();
        };

        QPointF prevPt = pts[0];
        bool prevInside = pointInsideMask(prevPt);

        for (int i = 1; i < pts.size(); ++i) {
            const QPointF& currPt = pts[i];
            const bool currInside = pointInsideMask(currPt);

            if (prevInside && currInside) {
                insideSegment.append(prevPt);
            } else if (prevInside && !currInside) {
                insideSegment.append(prevPt);
                QPointF edge = interpolateBoundaryPoint(prevPt, currPt, true);
                insideSegment.append(edge);
                flushInside(insideSegment);
            } else if (!prevInside && currInside) {
                QPointF edge = interpolateBoundaryPoint(prevPt, currPt, false);
                insideSegment.append(edge);
            }

            prevPt = currPt;
            prevInside = currInside;
        }

        if (prevInside) {
            insideSegment.append(prevPt);
        }
        flushInside(insideSegment);
    }
}

void BitmapImage::clipHybridStrokesWithMask(const QImage& mask, const QPointF& origin, QVector<HybridVectorStroke>* outInsideSegments)
{
    if (mHybridStrokes.isEmpty()) {
        return;
    }
    if (mask.isNull() || mask.width() <= 0 || mask.height() <= 0) {
        return;
    }

    const QRectF maskRect(origin, mask.size());

    auto pointInsideMask = [&](const QPointF& pt) -> bool {
        if (!maskRect.contains(pt)) {
            return false;
        }
        const QPointF lp = pt - origin;
        const int x = static_cast<int>(std::floor(lp.x()));
        const int y = static_cast<int>(std::floor(lp.y()));
        if (x < 0 || y < 0 || x >= mask.width() || y >= mask.height()) {
            return false;
        }
        const QRgb a = mask.pixel(x, y);
        return qAlpha(a) != 0;
    };

    QVector<HybridVectorStroke> newStrokes;
    newStrokes.reserve(mHybridStrokes.size());

    auto interpolateBoundaryPoint = [&](const QPointF& a, const QPointF& b, bool aInside) -> QPointF {
        // Binary search along the segment [a,b] to find an approximate
        // transition point where the mask changes from inside to outside
        // (or vice versa). This keeps the remaining stroke aligned with the
        // lasso edge instead of snapping back to the last untouched sample.
        QPointF p0 = a;
        QPointF p1 = b;
        bool s0 = aInside;
        for (int iter = 0; iter < 8; ++iter) {
            QPointF mid = QPointF((p0.x() + p1.x()) * 0.5, (p0.y() + p1.y()) * 0.5);
            bool sm = pointInsideMask(mid);
            if (sm == s0) {
                p0 = mid;
            } else {
                p1 = mid;
            }
        }
        return p1;
    };

    for (const HybridVectorStroke& stroke : mHybridStrokes) {
        if (stroke.points.size() < 2) {
            continue;
        }

        if (!stroke.bounds.intersects(maskRect)) {
            newStrokes.append(stroke);
            continue;
        }

        const QVector<QPointF>& pts = stroke.points;
        QVector<QPointF> outsideSegment;
        QVector<QPointF> insideSegment;
        outsideSegment.reserve(pts.size());
        insideSegment.reserve(pts.size());

        auto flushSegment = [&](QVector<QPointF>& seg, bool toInside) {
            if (seg.size() < 2) {
                seg.clear();
                return;
            }
            HybridVectorStroke ns;
            ns.points = seg;
            ns.baseWidth = stroke.baseWidth;
            QRectF b(ns.points.first(), ns.points.first());
            for (int k = 1; k < ns.points.size(); ++k) {
                const QPointF& cp = ns.points[k];
                if (cp.x() < b.left()) b.setLeft(cp.x());
                if (cp.x() > b.right()) b.setRight(cp.x());
                if (cp.y() < b.top()) b.setTop(cp.y());
                if (cp.y() > b.bottom()) b.setBottom(cp.y());
            }
            ns.bounds = b.normalized();
            if (toInside) {
                if (outInsideSegments) {
                    outInsideSegments->append(ns);
                }
            } else {
                newStrokes.append(ns);
            }
            seg.clear();
        };

        // Walk segments and insert a boundary point whenever a segment
        // crosses the mask edge so outside/inside segments align tightly
        // with the lasso.
        QPointF prevPt = pts[0];
        bool prevInside = pointInsideMask(prevPt);

        for (int i = 1; i < pts.size(); ++i) {
            const QPointF& currPt = pts[i];
            const bool currInside = pointInsideMask(currPt);

            if (prevInside == currInside) {
                // Same region: grow the active segment with prevPt
                if (prevInside) {
                    insideSegment.append(prevPt);
                } else {
                    outsideSegment.append(prevPt);
                }
            } else {
                // Crossing the mask boundary between prevPt and currPt
                QPointF edge = interpolateBoundaryPoint(prevPt, currPt, prevInside);

                if (prevInside) {
                    // Leaving inside region: close inside at edge, then
                    // start outside from the same edge.
                    insideSegment.append(prevPt);
                    insideSegment.append(edge);
                    flushSegment(insideSegment, true);
                    outsideSegment.append(edge);
                } else {
                    // Leaving outside region: close outside at edge, then
                    // start inside from the same edge.
                    outsideSegment.append(prevPt);
                    outsideSegment.append(edge);
                    flushSegment(outsideSegment, false);
                    insideSegment.append(edge);
                }
            }

            prevPt = currPt;
            prevInside = currInside;
        }

        // Append the final point to its region and flush remaining segments
        if (prevInside) {
            insideSegment.append(prevPt);
        } else {
            outsideSegment.append(prevPt);
        }

        flushSegment(insideSegment, true);
        flushSegment(outsideSegment, false);
    }

    mHybridStrokes = newStrokes;
    modification();
}
