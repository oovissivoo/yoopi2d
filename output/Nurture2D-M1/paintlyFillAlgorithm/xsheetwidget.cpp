#include "xsheetwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QDebug>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QMainWindow>
#include <QLabel>

XsheetWidget::XsheetWidget(QWidget *parent)
    : QWidget(parent)
    , currentFrame(1)
    , currentLayer("A")
    , visibleFrames(20)  // Show 20 frames by default
    , scrollOffset(1)
    , isDragging(false)
    , isScrubbing(false)
    , lastDragPos(0, 0)
    , pressPos(0, 0)
    , mouseHasMoved(false)
    , actionCenter(nullptr)
    , actionCenterLayout(nullptr)
    , addLayerButton(nullptr)
    , addKeyframeButton(nullptr)
    , removeKeyframeButton(nullptr)
    , frameInputField(nullptr)
    , frameInputRow(-1)
    , frameInputCol(-1)
    , isDraggingLayer(false)
    , dragLayerIndex(-1)
    , dragLayerTargetIndex(-1)
    , dragOffset(0, 0)
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    
    // Set up UI components
    setupUI();
    
    // Add default layer "A" with a keyframe on frame 1
    addLayer("A");
    setCurrentLayer("A");
    addKeyframe("A", 1);
    
    updateLayout();
}

XsheetWidget::~XsheetWidget()
{
}

void XsheetWidget::setupUI()
{
    // Create the Action Center with controls
    setupActionCenter();
    
    // Create the frame input field for selecting keyframes
    frameInputField = new QLineEdit(this);
    frameInputField->setFixedWidth(LAYER_WIDTH - 10);
    frameInputField->setPlaceholderText("Frame #");
    frameInputField->hide();
    
    // Connect signals
    connect(frameInputField, &QLineEdit::editingFinished, this, &XsheetWidget::onFrameInputFinished);
}

void XsheetWidget::setupActionCenter()
{
    // Create the Action Center widget at the top
    actionCenter = new QWidget(this);
    actionCenter->setGeometry(0, 0, width(), ACTION_CENTER_HEIGHT);
    actionCenterLayout = new QHBoxLayout(actionCenter);
    actionCenterLayout->setContentsMargins(5, 2, 5, 2);
    actionCenterLayout->setSpacing(5);
    
    // Add a title label
    QLabel *titleLabel = new QLabel("Animation Controls", actionCenter);
    titleLabel->setStyleSheet("font-weight: bold;");
    actionCenterLayout->addWidget(titleLabel);
    
    actionCenterLayout->addStretch();
    
    // Add Layer button
    addLayerButton = new QPushButton("Add Layer", actionCenter);
    actionCenterLayout->addWidget(addLayerButton);
    connect(addLayerButton, &QPushButton::clicked, this, &XsheetWidget::onAddLayerClicked);
    
    // Add Keyframe button
    addKeyframeButton = new QPushButton("Add Keyframe", actionCenter);
    actionCenterLayout->addWidget(addKeyframeButton);
    connect(addKeyframeButton, &QPushButton::clicked, this, &XsheetWidget::onAddKeyframeClicked);
    
    // Remove Keyframe button
    removeKeyframeButton = new QPushButton("Remove Keyframe", actionCenter);
    actionCenterLayout->addWidget(removeKeyframeButton);
    connect(removeKeyframeButton, &QPushButton::clicked, this, &XsheetWidget::onRemoveKeyframeClicked);
}

void XsheetWidget::onAddLayerClicked()
{
    // Generate a new layer name (next letter in alphabet)
    char nextLetter = 'A';
    if (!layers.isEmpty()) {
        QChar lastChar = layers.last().name[0];
        nextLetter = lastChar.toLatin1() + 1;
        if (nextLetter > 'Z') nextLetter = 'A'; // wrap around
    }
    
    QString newLayerName = QString(nextLetter);
    
    // Make sure this name is unique
    int suffix = 1;
    while (true) {
        bool nameExists = false;
        for (const Layer &layer : layers) {
            if (layer.name == newLayerName) {
                nameExists = true;
                break;
            }
        }
        
        if (!nameExists) break;
        
        // Add a numeric suffix to make the name unique
        newLayerName = QString(nextLetter) + QString::number(suffix);
        suffix++;
    }
    
    addLayer(newLayerName);
    
    // Always make the newly created layer current
    setCurrentLayer(newLayerName);
}

void XsheetWidget::onAddKeyframeClicked()
{
    // Add a keyframe at the current frame in the currently selected layer only
    if (!currentLayer.isEmpty() && currentFrame > 0) {
        // Get the layer index
        int layerIndex = getLayerIndex(currentLayer);
        if (layerIndex >= 0) {
            // Check if a keyframe already exists at this position
            if (layers[layerIndex].keyframes.contains(currentFrame)) {
                // Do nothing if there's already a keyframe here
                qDebug() << "Keyframe already exists at frame" << currentFrame << "for layer" << currentLayer;
                return;
            }
            
            // Only add a keyframe if one doesn't already exist
            addKeyframe(currentLayer, currentFrame);
            
            // Make sure we emit the signal to update the UI
            emit keyframeAdded(currentLayer, currentFrame);
        }
    }
}

void XsheetWidget::onRemoveKeyframeClicked()
{
    // Remove keyframe at the current frame in the current layer
    removeKeyframe(currentLayer, currentFrame);
}

void XsheetWidget::showFrameInputField(int frame, int layerIndex)
{
    if (layerIndex >= 0 && layerIndex < layers.size() && frame > 0) {
        frameInputRow = frame;
        frameInputCol = layerIndex;
        
        QString layerName = layers[layerIndex].name;
        
        // Calculate position for the input field
        QRect cellRect = getFrameRect(frame, layerIndex);
        frameInputField->setGeometry(cellRect.x() + 5, cellRect.y() + 2, 
                                     cellRect.width() - 10, cellRect.height() - 4);
        
        // Set current value if a keyframe exists
        if (layers[layerIndex].keyframes.contains(frame)) {
            QString frameName = layers[layerIndex].frameNames.value(frame, "");
            int frameNumber = getKeyframeFrameNumber(frameName);
            if (frameNumber > 0) {
                frameInputField->setText(QString::number(frameNumber));
            } else {
                frameInputField->clear();
            }
        } else {
            frameInputField->clear();
        }
        
        frameInputField->show();
        frameInputField->setFocus();
        frameInputField->selectAll();
    }
}

void XsheetWidget::hideFrameInputField()
{
    frameInputField->hide();
    frameInputRow = -1;
    frameInputCol = -1;
    setFocus();
}

void XsheetWidget::onFrameInputFinished()
{
    applyFrameInput();
    hideFrameInputField();
}

int XsheetWidget::findKeyframeWithNumber(const QString &layer, int frameNumber) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex < 0) return -1;
    
    // Go through all keyframes in this layer
    for (int frame : layers[layerIndex].keyframes) {
        QString frameName = layers[layerIndex].frameNames.value(frame);
        int existingFrameNumber = getKeyframeFrameNumber(frameName);
        
        // If we find a keyframe with the same reference number
        if (existingFrameNumber == frameNumber) {
            return frame; // Return the actual frame position
        }
    }
    
    return -1; // Not found
}

void XsheetWidget::applyFrameInput()
{
    if (frameInputRow > 0 && frameInputCol >= 0 && frameInputCol < layers.size()) {
        QString text = frameInputField->text().trimmed();
        
        if (!text.isEmpty()) {
            bool ok;
            int frameNumber = text.toInt(&ok);
            
            if (ok && frameNumber > 0) {
                QString layerName = layers[frameInputCol].name;
                
                // First, find if this frame number exists in this layer
                int existingKeyframePos = findKeyframeWithNumber(layerName, frameNumber);
                
                if (existingKeyframePos > 0) {
                    // The keyframe reference already exists
                    
                    // Check if there are any keyframes between the original position and the new position
                    bool hasKeyframesBetween = false;
                    int startFrame = qMin(existingKeyframePos, frameInputRow);
                    int endFrame = qMax(existingKeyframePos, frameInputRow);
                    
                    for (int frame = startFrame + 1; frame < endFrame; frame++) {
                        if (layers[frameInputCol].keyframes.contains(frame)) {
                            hasKeyframesBetween = true;
                            break;
                        }
                    }
                    
                    if (!hasKeyframesBetween && existingKeyframePos != frameInputRow) {
                        // No keyframes between, so MOVE the keyframe (instead of duplicating)
                        
                        // Get the image and other data from the existing keyframe
                        QImage img;
                        if (layers[frameInputCol].frames.contains(existingKeyframePos)) {
                            img = layers[frameInputCol].frames[existingKeyframePos];
                            // Remove the old keyframe
                            layers[frameInputCol].frames.remove(existingKeyframePos);
                            layers[frameInputCol].keyframes.remove(existingKeyframePos);
                            layers[frameInputCol].frameNames.remove(existingKeyframePos);
                        }
                        
                        // Create the frame name with the frame NUMBER, not frame POSITION
                        QString frameName = QString("%1_%2").arg(layerName).arg(frameNumber);
                        
                        // Add the keyframe at the new position
                        layers[frameInputCol].keyframes.insert(frameInputRow);
                        layers[frameInputCol].frameNames[frameInputRow] = frameName;
                        
                        if (!img.isNull()) {
                            layers[frameInputCol].frames[frameInputRow] = img;
                        }
                        
                        qDebug() << "Moved keyframe" << frameNumber << "from frame" << existingKeyframePos << "to frame" << frameInputRow;
                    } else {
                        // There are keyframes between or it's at the same position,
                        // so DUPLICATE (reuse) the keyframe
                        
                        // Create the frame name with the input number, not the frame position
                        QString frameName = QString("%1_%2").arg(layerName).arg(frameNumber);
                        
                        // Check if there's already a keyframe at the target position
                        if (layers[frameInputCol].keyframes.contains(frameInputRow)) {
                            // Update the frame name
                            layers[frameInputCol].frameNames[frameInputRow] = frameName;
                        } else {
                            // Add a new keyframe with this frame name
                            layers[frameInputCol].keyframes.insert(frameInputRow);
                            layers[frameInputCol].frameNames[frameInputRow] = frameName;
                        }
                        
                        // Copy the image data from the source keyframe if available
                        if (layers[frameInputCol].frames.contains(existingKeyframePos)) {
                            layers[frameInputCol].frames[frameInputRow] = layers[frameInputCol].frames[existingKeyframePos];
                            qDebug() << "Reused keyframe image" << frameNumber << "from frame" << existingKeyframePos << "to frame" << frameInputRow;
                        }
                    }
                    
                    update();
                    emit keyframeAdded(layerName, frameInputRow);
                } else {
                    // Check if this number exists in any other layer with actual content
                    bool frameExists = isExistingKeyframeNumber(frameNumber);
                    
                    if (frameExists) {
                        // Create the frame name with the input number, not the frame position
                        QString frameName = QString("%1_%2").arg(layerName).arg(frameNumber);
                        
                        // Check if there's already a keyframe at this frame
                        if (layers[frameInputCol].keyframes.contains(frameInputRow)) {
                            // Update the frame name
                            layers[frameInputCol].frameNames[frameInputRow] = frameName;
                        } else {
                            // Add a new keyframe with this frame name
                            addKeyframe(layerName, frameInputRow);
                            layers[frameInputCol].frameNames[frameInputRow] = frameName;
                        }
                        
                        // Find if there's an existing keyframe with the same number in another layer
                        for (int i = 0; i < layers.size(); i++) {
                            if (i == frameInputCol) continue; // Skip the current layer
                            
                            int sourceFramePos = findKeyframeWithNumber(layers[i].name, frameNumber);
                            if (sourceFramePos > 0) {
                                // Found a keyframe with this number in another layer
                                if (layers[i].frames.contains(sourceFramePos)) {
                                    // Copy the image data from that layer's keyframe to this one
                                    layers[frameInputCol].frames[frameInputRow] = layers[i].frames[sourceFramePos];
                                    qDebug() << "Reused keyframe image from layer" << layers[i].name << "frame" << sourceFramePos;
                                    break;
                                }
                            }
                        }
                        
                        update();
                        emit keyframeAdded(layerName, frameInputRow);
                    } else {
                        // Frame number doesn't exist, but we don't show an error message
                        // Just silently do nothing as requested
                        qDebug() << "Silently ignoring non-existent frame ID:" << frameNumber;
                    }
                }
            }
        } else {
            // Empty input, remove the keyframe if it exists
            if (layers[frameInputCol].keyframes.contains(frameInputRow)) {
                removeKeyframe(layers[frameInputCol].name, frameInputRow);
            }
        }
    }
}

int XsheetWidget::getKeyframeFrameNumber(const QString &frameName) const
{
    // Extract frame number from frameName (e.g., "A_3" -> 3)
    int underscorePos = frameName.lastIndexOf('_');
    if (underscorePos >= 0 && underscorePos < frameName.length() - 1) {
        QString frameNumberStr = frameName.mid(underscorePos + 1);
        bool ok;
        int frameNumber = frameNumberStr.toInt(&ok);
        if (ok) {
            return frameNumber;
        }
    }
    return -1;
}

void XsheetWidget::addLayer(const QString &name)
{
    // Check if layer already exists
    for (const Layer &layer : layers) {
        if (layer.name == name) {
            return;
        }
    }
    
    // Add new layer
    Layer newLayer;
    newLayer.name = name;
    newLayer.visible = true;
    layers.append(newLayer);
    
    updateLayout();
    
    // If this is the first layer, make it current
    if (layers.size() == 1) {
        setCurrentLayer(name);
    } else {
        // Make the newly created layer current
        setCurrentLayer(name);
    }
    
    // Automatically add a keyframe at the current frame
    addKeyframe(name, currentFrame);
    
    // Emit layer changed signal
    emit currentLayerChanged(currentLayer);
}

void XsheetWidget::removeLayer(const QString &name)
{
    for (int i = 0; i < layers.size(); ++i) {
        if (layers[i].name == name) {
            layers.removeAt(i);
            
            // If removed layer was current, select another one
            if (currentLayer == name && !layers.isEmpty()) {
                setCurrentLayer(layers[0].name);
            }
            
            updateLayout();
            return;
        }
    }
}

void XsheetWidget::setCurrentLayer(const QString &name)
{
    if (currentLayer != name) {
        // Check if layer exists
        bool layerExists = false;
        for (const Layer &layer : layers) {
            if (layer.name == name) {
                layerExists = true;
                break;
            }
        }
        
        if (layerExists) {
            currentLayer = name;
            update();
            emit currentLayerChanged(name);
        }
    }
}

QString XsheetWidget::getCurrentLayer() const
{
    return currentLayer;
}

void XsheetWidget::addKeyframe(int frame)
{
    addKeyframe(currentLayer, frame);
}

void XsheetWidget::addKeyframe(const QString &layer, int frame)
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        if (!layers[layerIndex].keyframes.contains(frame)) {
            layers[layerIndex].keyframes.insert(frame);
            
            // Create an empty frame image if it doesn't exist
            // Use a more descriptive keyframe image name (layer_frame)
            if (!layers[layerIndex].frames.contains(frame)) {
                // Initialize with a proper size (use parent's size or default 800x600)
                QSize canvasSize = QSize(800, 600);
                if (parentWidget() && parentWidget()->parentWidget()) {
                    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parentWidget()->parentWidget());
                    if (mainWindow && mainWindow->centralWidget()) {
                        canvasSize = mainWindow->centralWidget()->size();
                    }
                }
                
                // Create a completely blank/transparent image - THIS MUST BE CLEAN WITH NO CONTENT FROM OTHER LAYERS
                QImage blankImage(canvasSize, QImage::Format_ARGB32);
                blankImage.fill(Qt::transparent);
                
                // Store the completely blank image
                layers[layerIndex].frames[frame] = blankImage;
                
                // Store the image name (layer_frame)
                layers[layerIndex].frameNames[frame] = QString("%1_%2").arg(layer).arg(frame);
            }
            
            update();
            emit keyframeAdded(layer, frame);
        }
    }
}

void XsheetWidget::removeKeyframe(int frame)
{
    removeKeyframe(currentLayer, frame);
}

void XsheetWidget::removeKeyframe(const QString &layer, int frame)
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        if (layers[layerIndex].keyframes.contains(frame)) {
            layers[layerIndex].keyframes.remove(frame);
            update();
            emit keyframeRemoved(layer, frame);
        }
    }
}

void XsheetWidget::setCurrentFrame(int frame)
{
    if (currentFrame != frame && frame > 0) {
        currentFrame = frame;
        update();
        emit currentFrameChanged(frame);
    }
}

int XsheetWidget::getCurrentFrame() const
{
    return currentFrame;
}

void XsheetWidget::setFrameImage(const QString &layer, int frame, const QImage &image)
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        layers[layerIndex].frames[frame] = image;
        
        // Add keyframe if not already present
        if (!layers[layerIndex].keyframes.contains(frame)) {
            layers[layerIndex].keyframes.insert(frame);
            
            // Set the frame name if it doesn't exist
            if (!layers[layerIndex].frameNames.contains(frame)) {
                layers[layerIndex].frameNames[frame] = QString("%1_%2").arg(layer).arg(frame);
            }
        }
        
        update();
    }
}

QImage XsheetWidget::getFrameImage(const QString &layer, int frame) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        // Return frame image if exists
        if (layers[layerIndex].frames.contains(frame)) {
            return layers[layerIndex].frames[frame];
        }
    }
    
    // Return empty image if no frame exists
    return QImage();
}

QString XsheetWidget::getFrameName(const QString &layer, int frame) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        if (layers[layerIndex].frameNames.contains(frame)) {
            return layers[layerIndex].frameNames[frame];
        }
    }
    return QString();
}

void XsheetWidget::setFrameName(const QString &layer, int frame, const QString &name)
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex >= 0 && frame > 0) {
        layers[layerIndex].frameNames[frame] = name;
        update();
    }
}

bool XsheetWidget::isScrubArea(int x, int y) const
{
    // Check if the point is in the frame numbers column
    return (x >= 0 && x < FRAME_NUM_WIDTH && 
            y >= ACTION_CENTER_HEIGHT + HEADER_HEIGHT);
}

int XsheetWidget::frameFromY(int y)
{
    // Convert y coordinate to frame number for scrubbing
    if (y < ACTION_CENTER_HEIGHT + HEADER_HEIGHT)
        return -1;
    
    return (y - ACTION_CENTER_HEIGHT - HEADER_HEIGHT) / FRAME_HEIGHT + scrollOffset;
}

bool XsheetWidget::hasActualImage(const QString &layer, int frame) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex < 0 || frame <= 0) return false;
    
    // Check if the frame has an actual image that's not just an empty/transparent one
    if (layers[layerIndex].frames.contains(frame)) {
        const QImage &image = layers[layerIndex].frames.value(frame);
        if (!image.isNull()) {
            // Check if the image has any non-transparent pixels
            // This is a simple check - we could make this more sophisticated
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QColor pixelColor = image.pixelColor(x, y);
                    if (pixelColor.alpha() > 0) {
                        return true; // Found a non-transparent pixel
                    }
                }
            }
        }
    }
    
    return false;
}

int XsheetWidget::getNearestPreviousKeyframe(const QString &layer, int frame) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex < 0 || frame <= 1) return -1;
    
    // Find the nearest previous keyframe with actual content
    for (int f = frame - 1; f >= 1; --f) {
        if (layers[layerIndex].keyframes.contains(f) && hasActualImage(layer, f)) {
            return f;
        }
    }
    
    return -1; // No previous keyframe found
}

bool XsheetWidget::isExistingKeyframeNumber(int referenceNumber) const
{
    // Check all layers to see if any keyframe has this reference number AND has actual content
    for (const Layer &layer : layers) {
        for (int frame : layer.keyframes) {
            QString frameName = layer.frameNames.value(frame);
            int existingFrameNumber = getKeyframeFrameNumber(frameName);
            
            if (existingFrameNumber == referenceNumber && hasActualImage(layer.name, frame)) {
                return true;
            }
        }
    }
    return false;
}

bool XsheetWidget::isLayerVisible(const QString &name) const
{
    int layerIndex = getLayerIndex(name);
    if (layerIndex >= 0) {
        return layers[layerIndex].visible;
    }
    return false;
}

void XsheetWidget::setLayerVisible(const QString &name, bool visible)
{
    int layerIndex = getLayerIndex(name);
    if (layerIndex >= 0) {
        layers[layerIndex].visible = visible;
        update();
        // Emit a signal to update the UI
        emit currentLayerChanged(currentLayer);
    }
}

void XsheetWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Set background color
    painter.fillRect(rect(), QColor(240, 240, 240));
    
    // Draw the action center background
    painter.fillRect(0, 0, width(), ACTION_CENTER_HEIGHT, QColor(220, 220, 220));
    painter.setPen(Qt::black);
    painter.drawLine(0, ACTION_CENTER_HEIGHT, width(), ACTION_CENTER_HEIGHT);
    
    // Calculate visible range based on scroll position
    int startFrame = scrollOffset;
    int endFrame = startFrame + visibleFrames;
    
    // Draw grid and frames
    int yPos = ACTION_CENTER_HEIGHT + HEADER_HEIGHT;
    
    // Draw header row with layer names
    painter.fillRect(0, ACTION_CENTER_HEIGHT, width(), HEADER_HEIGHT, QColor(200, 200, 200));
    painter.setPen(Qt::black);
    painter.drawLine(0, yPos, width(), yPos);
    
    // Draw frame numbers column
    painter.fillRect(0, yPos, FRAME_NUM_WIDTH, height() - yPos, QColor(200, 200, 200));
    painter.drawLine(FRAME_NUM_WIDTH, ACTION_CENTER_HEIGHT, FRAME_NUM_WIDTH, height());
    
    // Draw layer names in header
    int xPos = FRAME_NUM_WIDTH + 5;
    for (int i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        
        // Calculate rect for the visibility checkbox position (at the top portion of the header)
        QRect checkboxRect(xPos + (LAYER_WIDTH / 2) - 7, 
                         ACTION_CENTER_HEIGHT + 5, 
                         14, 14);
                         
        // Draw visibility checkbox
        painter.setPen(Qt::black);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(checkboxRect);
        
        // If visible, draw a checkmark
        if (layer.visible) {
            painter.drawLine(checkboxRect.left() + 3, checkboxRect.center().y(), 
                            checkboxRect.center().x(), checkboxRect.bottom() - 3);
            painter.drawLine(checkboxRect.center().x(), checkboxRect.bottom() - 3,
                            checkboxRect.right() - 3, checkboxRect.top() + 3);
        }
        
        // Draw layer name - position it below the checkbox
        painter.drawText(xPos + (LAYER_WIDTH - painter.fontMetrics().horizontalAdvance(layer.name)) / 2, 
                         ACTION_CENTER_HEIGHT + HEADER_HEIGHT - 5, 
                         layer.name);
        
        // Draw vertical line separating layers
        painter.drawLine(xPos + LAYER_WIDTH, ACTION_CENTER_HEIGHT, xPos + LAYER_WIDTH, height());
        
        xPos += LAYER_WIDTH;
    }
    
    // Draw frames and keyframes
    for (int frame = startFrame; frame <= endFrame; frame++) {
        // Skip negative frames
        if (frame <= 0) continue;
        
        // Calculate y position for this frame
        yPos = ACTION_CENTER_HEIGHT + HEADER_HEIGHT + (frame - startFrame) * FRAME_HEIGHT;
        
        // Draw horizontal line
        painter.setPen(QPen(QColor(180, 180, 180)));
        painter.drawLine(0, yPos + FRAME_HEIGHT, width(), yPos + FRAME_HEIGHT);
        
        // Draw frame number - highlight if it's the current frame
        painter.setPen(Qt::black);
        QString frameText = QString::number(frame);
        
        // Use bold font for the scrub area frame numbers
        QFont frameFont = painter.font();
        if (frame == currentFrame) {
            // Use bold for current frame in scrub area
            frameFont.setBold(true);
            painter.setFont(frameFont);
        }
        painter.drawText(5, yPos + FRAME_HEIGHT / 2 + painter.fontMetrics().height() / 2, frameText);
        
        // Reset font for other elements
        frameFont.setBold(false);
        painter.setFont(frameFont);
        
        // Draw keyframes for each layer
        xPos = FRAME_NUM_WIDTH;
        
        for (int i = 0; i < layers.size(); ++i) {
            const Layer& layer = layers[i];
            
            // Draw cells differently if layer is hidden
            bool isVisible = layer.visible;
            
            // Check if there's a keyframe at this frame
            bool hasKeyframe = layer.keyframes.contains(frame);
            
            if (hasKeyframe) {
                // Draw rectangle for keyframe cell
                QRect keyframeRect(xPos, yPos, LAYER_WIDTH, FRAME_HEIGHT);
                
                // Highlight current frame/layer
                if (frame == currentFrame && layer.name == currentLayer) {
                    painter.fillRect(keyframeRect, QColor(220, 220, 255));
                    painter.setPen(QPen(Qt::red, 2));
                    painter.drawRect(keyframeRect);
                } else {
                    // Use different color for hidden layers
                    if (!isVisible) {
                        painter.fillRect(keyframeRect, QColor(200, 200, 200)); // Grayed out
                    } else {
                        painter.fillRect(keyframeRect, QColor(240, 240, 240));
                    }
                    painter.setPen(Qt::black);
                    painter.drawRect(keyframeRect);
                }
                
                // Get frame name and extract frame number
                QString frameName = layer.frameNames.value(frame, "");
                int frameNumber = getKeyframeFrameNumber(frameName);
                
                // Draw the frame number in the cell
                if (frameNumber > 0) {
                    // Use dimmed text for hidden layers
                    if (!isVisible) {
                        painter.setPen(QColor(120, 120, 120)); // Dimmed text
                    } else {
                        painter.setPen(Qt::black);
                    }
                    
                    QString displayText = QString::number(frameNumber);
                    
                    // Set font style based on whether this keyframe has actual content
                    QFont keyframeFont = painter.font();
                    
                    // Check if this frame has actual content
                    bool hasContent = hasActualImage(layer.name, frame);
                    
                    // Fix: Check if this is a reference to another frame's content
                    bool isReference = false;
                    if (frameNumber != frame) {
                        // This is a reference keyframe (showing content from another frame)
                        isReference = true;
                    }
                    
                    if (hasContent) {
                        // Bold for keyframes with actual content
                        keyframeFont.setBold(true);
                    } else if (isReference) {
                        // Italic for keyframes that reference other frames
                        keyframeFont.setItalic(true);
                    }
                    
                    painter.setFont(keyframeFont);
                    painter.drawText(keyframeRect, Qt::AlignCenter, displayText);
                    
                    // Reset font
                    painter.setFont(QFont());
                }
            } else {
                // Draw empty cell for no keyframe
                QRect cellRect(xPos, yPos, LAYER_WIDTH, FRAME_HEIGHT);
                
                // Highlight current frame/layer even with no keyframe
                if (frame == currentFrame && layer.name == currentLayer) {
                    painter.fillRect(cellRect, QColor(220, 220, 255));
                } else if (!isVisible) {
                    // Use lighter color for hidden layers with no keyframe
                    painter.fillRect(cellRect, QColor(230, 230, 230));
                }
            }
            
            xPos += LAYER_WIDTH;
        }
    }
    
    // Highlight current frame with a horizontal indicator - make it more visible for scrubbing
    int currentFrameY = ACTION_CENTER_HEIGHT + HEADER_HEIGHT + (currentFrame - startFrame) * FRAME_HEIGHT;
    if (currentFrameY >= ACTION_CENTER_HEIGHT + HEADER_HEIGHT && currentFrameY <= height()) {
        painter.setPen(QPen(Qt::red, 2));
        painter.drawLine(0, currentFrameY, FRAME_NUM_WIDTH - 1, currentFrameY);
        
        // Add a small triangle indicator for better visibility during scrubbing
        QPolygon triangle;
        triangle << QPoint(FRAME_NUM_WIDTH - 7, currentFrameY)
                << QPoint(FRAME_NUM_WIDTH - 1, currentFrameY - 4)
                << QPoint(FRAME_NUM_WIDTH - 1, currentFrameY + 4);
        painter.setBrush(Qt::red);
        painter.drawPolygon(triangle);
    }
    
    // After drawing all layers, draw the layer being dragged (if any)
    if (isDraggingLayer && dragLayerIndex >= 0 && dragLayerIndex < layers.size()) {
        // Draw indicator for where the layer will be inserted
        int insertX = FRAME_NUM_WIDTH + dragLayerTargetIndex * LAYER_WIDTH;
        
        // Draw a more visible highlight at the target position
        painter.setPen(QPen(Qt::blue, 3));  // Thicker blue line
        painter.drawLine(insertX, ACTION_CENTER_HEIGHT, 
                         insertX, height());  // Extend line all the way down
        
        // Draw an arrow at the insertion point for better visual feedback
        QPolygon arrow;
        int arrowY = ACTION_CENTER_HEIGHT + HEADER_HEIGHT / 2;
        arrow << QPoint(insertX - 8, arrowY - 8)
              << QPoint(insertX, arrowY)
              << QPoint(insertX - 8, arrowY + 8);
        painter.setBrush(QBrush(Qt::blue));
        painter.drawPolygon(arrow);
        
        // Draw a "shadow" of the layer being dragged with higher contrast
        const Layer& dragLayer = layers[dragLayerIndex];
        QRect layerRect(insertX, ACTION_CENTER_HEIGHT, LAYER_WIDTH, HEADER_HEIGHT);
        
        // Draw a more obvious background for the dragged layer
        QLinearGradient gradient(layerRect.topLeft(), layerRect.bottomRight());
        gradient.setColorAt(0, QColor(150, 150, 255, 200));  // More opaque blue
        gradient.setColorAt(1, QColor(100, 100, 255, 200));
        painter.fillRect(layerRect, gradient);
        
        // Draw a border around the dragged layer
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(layerRect);
        
        // Draw the layer name with a drop shadow effect
        painter.setPen(QColor(0, 0, 0, 100));  // Shadow color
        painter.drawText(insertX + 2 + (LAYER_WIDTH - painter.fontMetrics().horizontalAdvance(dragLayer.name)) / 2, 
                         ACTION_CENTER_HEIGHT + HEADER_HEIGHT - 3, 
                         dragLayer.name);
        
        // Draw the actual layer name
        painter.setPen(Qt::black);
        painter.drawText(insertX + (LAYER_WIDTH - painter.fontMetrics().horizontalAdvance(dragLayer.name)) / 2, 
                         ACTION_CENTER_HEIGHT + HEADER_HEIGHT - 5, 
                         dragLayer.name);
        
        // Draw "Dragging" indicator at the bottom of the screen
        QString dragText = QString("Dragging layer '%1'").arg(dragLayer.name);
        painter.setPen(QColor(0, 0, 155));
        painter.drawText(FRAME_NUM_WIDTH + 10, height() - 10, dragText);
    }
}

void XsheetWidget::mousePressEvent(QMouseEvent *event)
{
    // Hide the frame input field if it's visible
    if (frameInputField->isVisible()) {
        applyFrameInput();
        hideFrameInputField();
        return;
    }
    
    // Ignore clicks in the action center area
    if (event->y() < ACTION_CENTER_HEIGHT) {
        return;
    }
    
    // Start potential dragging operation
    if (event->button() == Qt::LeftButton) {
        // Check if click is in the frame numbers column (scrub area)
        if (isScrubArea(event->x(), event->y())) {
            isScrubbing = true;
            isDragging = false;
            
            // Set current frame based on click position
            int frame = frameFromY(event->y());
            if (frame > 0) {
                setCurrentFrame(frame);
            }
            
            // Store initial position
            pressPos = event->pos();
            lastDragPos = event->pos();
            return;
        }
        
        // Check if clicked in the header row (layer visibility or selecting a layer)
        if (event->y() >= ACTION_CENTER_HEIGHT && event->y() < ACTION_CENTER_HEIGHT + HEADER_HEIGHT) {
            // Clicked on layer header
            if (event->x() > FRAME_NUM_WIDTH) {
                int layerIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
                if (layerIndex >= 0 && layerIndex < layers.size()) {
                    // Get the x position within the layer header
                    int relX = event->x() - (FRAME_NUM_WIDTH + layerIndex * LAYER_WIDTH);
                    int relY = event->y() - ACTION_CENTER_HEIGHT;
                    
                    // Check if clicked on visibility checkbox
                    QRect checkboxRect((LAYER_WIDTH / 2) - 7 + 5, 5, 14, 14);
                    if (checkboxRect.contains(relX, relY)) {
                        // Toggle layer visibility
                        Layer &layer = layers[layerIndex];
                        layer.visible = !layer.visible;
                        update();
                        
                        // Notify change so the canvas can be updated
                        emit currentLayerChanged(currentLayer);
                        return;
                    }
                    
                    // If clicked in the layer name area, start layer dragging
                    // Use a more reliable condition for determining if we're in the layer name area
                    if (relY > checkboxRect.bottom()) {
                        // Log that we're starting layer dragging
                        qDebug() << "Starting layer drag for layer" << layers[layerIndex].name << "at index" << layerIndex;
                        
                        isDraggingLayer = true;
                        dragLayerIndex = layerIndex;
                        dragLayerTargetIndex = layerIndex;
                        dragOffset = QPoint(relX, relY);
                        setCursor(Qt::ClosedHandCursor);
                        
                        // Also select this layer
                        setCurrentLayer(layers[layerIndex].name);
                        update();
                        return;
                    } else {
                        // Otherwise, just select the layer
                        setCurrentLayer(layers[layerIndex].name);
                    }
                }
            }
            return;
        }
        
        // Store initial click position
        pressPos = event->pos();
        mouseHasMoved = false;
        
        // Don't set isDragging yet - wait to see if mouse moves beyond threshold
        isDragging = false;
        lastDragPos = event->pos();
        
        // Check if clicked on a frame cell to select both layer and frame
        if (event->y() > ACTION_CENTER_HEIGHT + HEADER_HEIGHT && event->x() > FRAME_NUM_WIDTH) {
            int frame = (event->y() - ACTION_CENTER_HEIGHT - HEADER_HEIGHT) / FRAME_HEIGHT + scrollOffset;
            int layerIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
            
            if (layerIndex >= 0 && layerIndex < layers.size() && frame > 0) {
                // First, set the active layer to the one that was clicked
                QString layerName = layers[layerIndex].name;
                setCurrentLayer(layerName);
                
                // Then, set the current frame
                setCurrentFrame(frame);
                
                // Debug output to confirm layer and frame selection
                qDebug() << "Selected Layer:" << layerName << "Frame:" << frame;
            }
        }
    }
    else if (event->button() == Qt::RightButton) {
        // Right-click context menu
        // Ignore right-clicks in the action center area
        if (event->y() < ACTION_CENTER_HEIGHT) {
            return;
        }
        
        // Check if right-click is in the header area
        if (event->y() >= ACTION_CENTER_HEIGHT && event->y() < ACTION_CENTER_HEIGHT + HEADER_HEIGHT) {
            QMenu contextMenu(this);
            
            // Only show rename/remove options if clicking on an actual layer header
            if (event->x() > FRAME_NUM_WIDTH) {
                int layerIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
                if (layerIndex >= 0 && layerIndex < layers.size()) {
                    QAction *renameLayerAction = contextMenu.addAction("Rename Layer");
                    QAction *removeLayerAction = contextMenu.addAction("Remove Layer");
                    
                    QAction *selectedAction = contextMenu.exec(event->globalPos());
                    
                    if (selectedAction == renameLayerAction) {
                        bool ok;
                        QString newName = QInputDialog::getText(this, tr("Rename Layer"),
                                                            tr("New layer name:"), QLineEdit::Normal,
                                                            layers[layerIndex].name, &ok);
                        if (ok && !newName.isEmpty()) {
                            // Check if the name is unique
                            bool nameExists = false;
                            for (const Layer &layer : layers) {
                                if (layer.name == newName && layer.name != layers[layerIndex].name) {
                                    nameExists = true;
                                    break;
                                }
                            }
                            
                            if (!nameExists) {
                                // Store the old name
                                QString oldName = layers[layerIndex].name;
                                
                                // Update the layer name
                                layers[layerIndex].name = newName;
                                
                                // If this was the current layer, update the current layer name
                                if (currentLayer == oldName) {
                                    currentLayer = newName;
                                }
                                
                                update();
                            }
                            else {
                                QMessageBox::warning(this, tr("Rename Layer"),
                                                  tr("A layer with this name already exists."));
                            }
                        }
                    }
                    else if (selectedAction == removeLayerAction) {
                        // Don't allow removing the last layer
                        if (layers.size() > 1) {
                            removeLayer(layers[layerIndex].name);
                        }
                        else {
                            QMessageBox::warning(this, tr("Remove Layer"),
                                              tr("Cannot remove the only layer."));
                        }
                    }
                }
            }
        }
        // Add right-click context menu for frame cells
        else if (event->y() > ACTION_CENTER_HEIGHT + HEADER_HEIGHT && event->x() > FRAME_NUM_WIDTH) {
            int frame = (event->y() - ACTION_CENTER_HEIGHT - HEADER_HEIGHT) / FRAME_HEIGHT + scrollOffset;
            int layerIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
            
            if (layerIndex >= 0 && layerIndex < layers.size() && frame > 0) {
                QMenu contextMenu(this);
                
                // Get layer name for this cell
                QString cellLayerName = layers[layerIndex].name;
                
                // Different options depending on whether cell has a keyframe
                bool hasKeyframe = layers[layerIndex].keyframes.contains(frame);
                
                if (hasKeyframe) {
                    QAction *editFrameAction = contextMenu.addAction("Edit Keyframe");
                    QAction *removeFrameAction = contextMenu.addAction("Remove Keyframe");
                    
                    QAction *selectedAction = contextMenu.exec(event->globalPos());
                    
                    if (selectedAction == editFrameAction) {
                        // First make sure this layer and frame are selected
                        setCurrentLayer(cellLayerName);
                        setCurrentFrame(frame);
                        
                        // Show the frame input dialog
                        showFrameInputField(frame, layerIndex);
                    }
                    else if (selectedAction == removeFrameAction) {
                        removeKeyframe(cellLayerName, frame);
                    }
                } 
                else {
                    QAction *addFrameAction = contextMenu.addAction("Add Keyframe");
                    
                    QAction *selectedAction = contextMenu.exec(event->globalPos());
                    
                    if (selectedAction == addFrameAction) {
                        // Select this layer and frame first
                        setCurrentLayer(cellLayerName);
                        setCurrentFrame(frame);
                        
                        // Add a keyframe at this position
                        addKeyframe(cellLayerName, frame);
                    }
                }
            }
        }
    }
}

void XsheetWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Ignore drag if input field is visible
    if (frameInputField->isVisible()) {
        return;
    }
    
    // Handle layer dragging
    if (isDraggingLayer) {
        if (event->x() > FRAME_NUM_WIDTH) {
            // Calculate the target column index based on mouse position
            int targetIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
            targetIndex = qBound(0, targetIndex, layers.size() - 1);
            
            if (targetIndex != dragLayerTargetIndex) {
                qDebug() << "Layer drag target changed from" << dragLayerTargetIndex << "to" << targetIndex;
                dragLayerTargetIndex = targetIndex;
                update(); // Repaint to show the new potential position
            }
        }
        return;
    }
    
    // Handle scrubbing in the frame numbers column
    if (isScrubbing) {
        int frame = frameFromY(event->y());
        if (frame > 0) {
            setCurrentFrame(frame);
        }
        lastDragPos = event->pos();
        return;
    }
    
    // Check if mouse has moved beyond threshold
    QPoint delta = event->pos() - pressPos;
    if (!mouseHasMoved && (delta.manhattanLength() > DRAG_THRESHOLD)) {
        mouseHasMoved = true;
        isDragging = true;
        // We now use double-click instead of single-click with timer
    }
    
    if (isDragging) {
        // Only handle vertical scrolling
        int dy = event->y() - lastDragPos.y();
        scrollOffset = qMax(1, scrollOffset - dy / FRAME_HEIGHT);
        lastDragPos = event->pos();
        update();
    }
}

void XsheetWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // If we were dragging a layer, finalize the move
        if (isDraggingLayer) {
            if (dragLayerIndex != dragLayerTargetIndex) {
                qDebug() << "Finalizing layer drag from" << dragLayerIndex << "to" << dragLayerTargetIndex;
                
                // Move the layer to the new position
                Layer movedLayer = layers.takeAt(dragLayerIndex);
                
                // If we're moving a layer to a position after its original index,
                // we need to adjust the target index because removing the layer shifted everything
                if (dragLayerTargetIndex > dragLayerIndex) {
                    dragLayerTargetIndex--;
                }
                
                layers.insert(dragLayerTargetIndex, movedLayer);
                
                // Update currentLayer if we moved the selected layer
                if (currentLayer == movedLayer.name) {
                    emit currentLayerChanged(currentLayer);
                }
                
                // Update the layout and redraw
                updateLayout();
                
                // Emit a signal to ensure the canvas gets updated with the new layer order
                emit layersReordered();
                
                qDebug() << "Layer" << movedLayer.name << "moved from index" << dragLayerIndex << "to" << dragLayerTargetIndex;
            } else {
                qDebug() << "Layer drag canceled - same position";
            }
            
            // Reset drag state
            isDraggingLayer = false;
            dragLayerIndex = -1;
            dragLayerTargetIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
            return;
        }
        
        isDragging = false;
        isScrubbing = false;
    }
}

void XsheetWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Handle double click for editing keyframe numbers
    if (event->button() == Qt::LeftButton) {
        // Check if double-clicked on a frame cell (not in scrub area)
        if (event->y() > ACTION_CENTER_HEIGHT + HEADER_HEIGHT && event->x() > FRAME_NUM_WIDTH) {
            int frame = (event->y() - ACTION_CENTER_HEIGHT - HEADER_HEIGHT) / FRAME_HEIGHT + scrollOffset;
            int layerIndex = (event->x() - FRAME_NUM_WIDTH) / LAYER_WIDTH;
            
            if (layerIndex >= 0 && layerIndex < layers.size() && frame > 0) {
                // Show the input field for editing the keyframe number
                showFrameInputField(frame, layerIndex);
            }
        }
    }
}

void XsheetWidget::wheelEvent(QWheelEvent *event)
{
    // Handle vertical scrolling
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;
    
    scrollOffset = qMax(1, scrollOffset - numSteps);
    update();
    
    event->accept();
}

void XsheetWidget::drawFrame(QPainter &painter, int x, int y, const QImage &frame)
{
    if (!frame.isNull()) {
        painter.drawImage(x, y, frame);
    } else {
        // Draw empty frame
        painter.fillRect(x, y, FRAME_WIDTH, FRAME_HEIGHT, QColor(50, 50, 50));
    }
}

void XsheetWidget::drawTimingNumbers(QPainter &painter)
{
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8));
    
    for (int frame = 1; frame <= visibleFrames; ++frame) {
        QRect rect = getTimingNumberRect(frame);
        painter.drawText(rect, Qt::AlignCenter, QString::number(frame));
    }
}

void XsheetWidget::drawLayerNames(QPainter &painter)
{
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8));
    
    for (int layer = 0; layer < layers.size(); ++layer) {
        QRect rect = getLayerNameRect(layer);
        painter.drawText(rect, Qt::AlignCenter, layers[layer].name);
    }
}

QRect XsheetWidget::getFrameRect(int frame, int layerIndex) const
{
    int x = FRAME_NUM_WIDTH + layerIndex * LAYER_WIDTH; 
    int y = ACTION_CENTER_HEIGHT + HEADER_HEIGHT + (frame - scrollOffset) * FRAME_HEIGHT;
    return QRect(x, y, LAYER_WIDTH, FRAME_HEIGHT);
}

QRect XsheetWidget::getTimingNumberRect(int frame)
{
    return QRect(0,
                ACTION_CENTER_HEIGHT + HEADER_HEIGHT + (frame - scrollOffset) * FRAME_HEIGHT,
                FRAME_NUM_WIDTH,
                FRAME_HEIGHT);
}

QRect XsheetWidget::getLayerNameRect(int layer)
{
    return QRect(FRAME_NUM_WIDTH + layer * LAYER_WIDTH,
                ACTION_CENTER_HEIGHT,
                LAYER_WIDTH,
                HEADER_HEIGHT);
}

int XsheetWidget::frameFromX(int x)
{
    return (x - FRAME_NUM_WIDTH) / LAYER_WIDTH;
}

int XsheetWidget::layerFromY(int y)
{
    return (y - ACTION_CENTER_HEIGHT - HEADER_HEIGHT) / FRAME_HEIGHT;
}

int XsheetWidget::getLayerIndex(const QString &layerName) const
{
    for (int i = 0; i < layers.size(); ++i) {
        if (layers[i].name == layerName) {
            return i;
        }
    }
    return -1;
}

void XsheetWidget::updateLayout()
{
    // Adjust the widget size based on the number of frames and layers
    int minWidth = FRAME_NUM_WIDTH + layers.size() * LAYER_WIDTH;
    int minHeight = ACTION_CENTER_HEIGHT + HEADER_HEIGHT + visibleFrames * FRAME_HEIGHT;
    
    setMinimumSize(minWidth, minHeight);
    
    // Also resize action center
    if (actionCenter) {
        actionCenter->setFixedWidth(width());
    }
    
    update();
}

int XsheetWidget::findVisibleFrameAt(const QString &layer, int targetFrame) const
{
    int layerIndex = getLayerIndex(layer);
    if (layerIndex < 0 || targetFrame <= 0) return -1;

    // First check if there's a keyframe exactly at the target frame
    if (layers[layerIndex].frames.contains(targetFrame)) {
        return targetFrame;
    }

    // If not, find the highest frame number less than or equal to targetFrame
    int bestFrame = -1;
    for (auto it = layers[layerIndex].frames.begin(); it != layers[layerIndex].frames.end(); ++it) {
        int frameNumber = it.key();
        
        // Skip frames higher than our target
        if (frameNumber > targetFrame) continue;
        
        // Keep track of the highest frame number we've seen
        if (frameNumber > bestFrame) {
            bestFrame = frameNumber;
        }
    }
    
    return bestFrame; // Will be -1 if no appropriate frame was found
} 