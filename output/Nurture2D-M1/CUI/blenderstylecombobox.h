#ifndef BLENDERSTYLECOMBOBOX_H
#define BLENDERSTYLECOMBOBOX_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QMouseEvent>
#include <QPaintEvent>

/**
 * BlenderStyleComboBox - A segmented control widget matching Blender's UI aesthetic
 * 
 * This widget displays multiple options in a unified horizontal bar with:
 * - Rounded corners on the far left and far right only
 * - Seamlessly connected middle sections
 * - Highlighted selected state
 * - Single unified component (not separate buttons)
 * 
 * Usage:
 *   auto* combo = new BlenderStyleComboBox(this);
 *   combo->setOptions({"Off", "Low", "Med", "High"});
 *   combo->setCurrentIndex(0);
 *   connect(combo, &BlenderStyleComboBox::currentIndexChanged, ...);
 */
class BlenderStyleComboBox : public QWidget
{
    Q_OBJECT

public:
    explicit BlenderStyleComboBox(QWidget *parent = nullptr);
    ~BlenderStyleComboBox();
    
    // Set the available options
    void setOptions(const QStringList &options);
    
    // Get/Set current selection
    int currentIndex() const { return mCurrentIndex; }
    void setCurrentIndex(int index);
    
    // Get current option text
    QString currentText() const;
    
    // Enable/disable specific options
    void setOptionEnabled(int index, bool enabled);
    bool isOptionEnabled(int index) const;
    
    // Styling
    void setAccentColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setSelectedTextColor(const QColor &color);
    void setCornerRadius(int radius);
    
    // Get the global accent color from preferences
    static QColor getAccentColor();

signals:
    void currentIndexChanged(int index);
    void optionClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    // Data
    QStringList mOptions;
    QVector<bool> mEnabledStates;
    int mCurrentIndex;
    int mHoverIndex;
    int mPressedIndex;
    
    // Styling
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    QColor mSelectedTextColor;
    int mCornerRadius;
    
    // Helper functions
    int indexAtPosition(const QPoint &pos) const;
    QRect segmentRect(int index) const;
    void initializeColors();
};

#endif // BLENDERSTYLECOMBOBOX_H

