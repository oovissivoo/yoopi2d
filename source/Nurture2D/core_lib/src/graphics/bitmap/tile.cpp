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

#include "tile.h"

#include <QPainter>

Tile::Tile(const QPoint& pos, QSize size):
    mTilePixmap(size),
    mPosF(pos),
    mPos(pos),
    mBounds(pos, size),
    mSize(size)
{
    clear(); //Default tiles are transparent
    mTileImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
    mTileImage.fill(Qt::transparent);
    mImageDirty = false;
}

Tile::~Tile()
{
}

void Tile::load(const QImage& image, const QPoint& topLeft)
{
    QPainter painter(&mTilePixmap);

    painter.translate(-mPos);
    painter.drawImage(topLeft, image);
    painter.end();
    // keep image in sync
    syncImageFromPixmap();
}

void Tile::clear()
{
    mTilePixmap.fill(Qt::transparent);
    mTileImage = QImage(mSize, QImage::Format_ARGB32_Premultiplied);
    mTileImage.fill(Qt::transparent);
    mImageDirty = false;
}

void Tile::syncPixmapFromImage()
{
    if (!mImageDirty) return;
    mTilePixmap = QPixmap::fromImage(mTileImage);
    mImageDirty = false;
}

void Tile::syncImageFromPixmap()
{
    mTileImage = mTilePixmap.toImage();
    if (mTileImage.format() != QImage::Format_ARGB32_Premultiplied) {
        mTileImage = mTileImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    mImageDirty = false;
}

void Tile::setImageDirty(bool dirty)
{
    mImageDirty = dirty;
}
