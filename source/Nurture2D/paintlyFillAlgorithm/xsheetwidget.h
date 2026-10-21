#ifndef XSHEETWIDGET_H
#define XSHEETWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QVector>
#include <QMap>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QSet>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>

class XsheetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit XsheetWidget(QWidget *parent = nullptr);
    ~XsheetWidget();

    // Layer management
    void addLayer(const QString &name);
    void removeLayer(const QString &name);
    void setCurrentLayer(const QString &name);
    QString getCurrentLayer() const;
    
    // Layer info methods
    int getLayerCount() const { return layers.size(); }
    QString getLayerName(int index) const { return (index >= 0 && index < layers.size()) ? layers[index].name : QString(); }
    int getLayerIndex(const QString &layerName) const;
    // bool isLayerHidden(const QString &name) const; // Future feature

    // Frame management
    void addKeyframe(int frame);
    void addKeyframe(const QString &layer, int frame);
    void removeKeyframe(int frame);
    void removeKeyframe(const QString &layer, int frame);
    void setCurrentFrame(int frame);
    int getCurrentFrame() const;

    // Frame data management
    void setFrameImage(const QString &layer, int frame, const QImage &image);
    QImage getFrameImage(const QString &layer, int frame) const;
    QString getFrameName(const QString &layer, int frame) const;
    void setFrameName(const QString &layer, int frame, const QString &name);
    
    // Get the nearest previous keyframe with actual image content
    int getNearestPreviousKeyframe(const QString &layer, int frame) const;
    
    // Find the keyframe that should be visible at a given frame in the timeline
    // Returns -1 if no keyframe should be shown
    int findVisibleFrameAt(const QString &layer, int targetFrame) const;
    
    // Add method to find keyframe with the same reference number
    int findKeyframeWithNumber(const QString &layer, int frameNumber) const;

    // Add new methods for layer visibility
    bool isLayerVisible(const QString &name) const;
    void setLayerVisible(const QString &name, bool visible);

signals:
    void currentFrameChanged(int frame);
    void currentLayerChanged(const QString &layer);
    void keyframeAdded(const QString &layer, int frame);
    void keyframeRemoved(const QString &layer, int frame);
    void layersReordered(); // New signal when layers are reordered

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Constants for frame and layer dimensions
    static const int FRAME_WIDTH = 100;
    static const int FRAME_HEIGHT = 24;
    static const int LAYER_WIDTH = 60;  // Reduced from 100 to 60 for thinner columns
    static const int HEADER_HEIGHT = 30;
    static const int FRAME_NUM_WIDTH = 30;
    static const int KEYFRAME_SIZE = 16;
    static const int ACTION_CENTER_HEIGHT = 40; // Height for the action center
    static const int DRAG_THRESHOLD = 10; // Pixels to move before considering it a drag

    struct Layer {
        QString name;
        QMap<int, QImage> frames;  // frame number -> image
        QSet<int> keyframes;       // Set of frame numbers with keyframes
        QMap<int, QString> frameNames; // frame number -> image name
        bool visible = true;       // Whether the layer is visible or not
    };

    QVector<Layer> layers;
    QString currentLayer;
    int currentFrame;
    int visibleFrames;
    int scrollOffset;
    bool isDragging;
    bool isScrubbing; // Flag to track if user is scrubbing in the frame number column
    QPoint lastDragPos;
    QPoint pressPos; // Position where mouse was pressed 
    bool mouseHasMoved; // Flag to track if mouse has moved significantly
    
    // New UI elements
    QWidget *actionCenter;
    QHBoxLayout *actionCenterLayout;
    QPushButton *addLayerButton;
    QPushButton *addKeyframeButton;
    QPushButton *removeKeyframeButton;
    
    QLineEdit *frameInputField;
    int frameInputRow; // Row where input field is being shown
    int frameInputCol; // Column where input field is being shown
    
    // Layer dragging
    bool isDraggingLayer;
    int dragLayerIndex;
    int dragLayerTargetIndex;
    QPoint dragOffset;
    
    void setupUI();
    void setupActionCenter();
    void showFrameInputField(int frame, int layerIndex);
    void hideFrameInputField();
    void applyFrameInput();
    
    void updateLayout();
    void drawFrame(QPainter &painter, int x, int y, const QImage &frame);
    void drawTimingNumbers(QPainter &painter);
    void drawLayerNames(QPainter &painter);
    QRect getFrameRect(int frame, int layerIndex) const;
    QRect getTimingNumberRect(int frame);
    QRect getLayerNameRect(int layer);
    int frameFromX(int x);
    int layerFromY(int y);
    int frameFromY(int y); // Convert y coordinate to frame number for scrubbing
    bool isScrubArea(int x, int y) const; // Check if point is in frame number scrub area
    
    // Helper methods
    int getKeyframeFrameNumber(const QString &frameName) const;
    bool isExistingKeyframeNumber(int referenceNumber) const; // Check if a keyframe number exists in any layer
    bool hasActualImage(const QString &layer, int frame) const; // Check if a frame has actual image content

private slots:
    void onAddLayerClicked();
    void onAddKeyframeClicked();
    void onRemoveKeyframeClicked();
    void onFrameInputFinished();
};

#endif // XSHEETWIDGET_H 