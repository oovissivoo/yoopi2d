#ifndef COSMICBOTTOMTOOLSBAR_H
#define COSMICBOTTOMTOOLSBAR_H

#include <QWidget>
#include <QColor>

class CosmicCheckboxToggle;
class QVBoxLayout;
class QTimer;

class CosmicBottomToolsBar : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicBottomToolsBar(QWidget* parent = nullptr);

    static QColor pillDisabledIconColor();

    bool lockEnabled() const;
    void setLockEnabled(bool enabled);
    void toggleLock();

    QWidget* contentWidget() const;
    void setContentWidget(QWidget* w);

    int preferredHeight() const;
    void setPreferredHeight(int h);
    bool isCollapsed() const;

signals:
    void lockEnabledChanged(bool enabled);
    void preferredHeightChanged(int h);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateToggleIcons();
    void loadSurvivalSettings();
    void saveSurvivalSettings() const;
    void applyAutoHideState(bool force = false);
    void showContent();
    void hideContent();
    void toggleLockState();
    void enforceEndCapsOnTop();
    void updateResizeGripGeometry();
    void beginResize(const QPoint& globalPos);
    void updateResize(const QPoint& globalPos);
    void endResize();

    CosmicCheckboxToggle* mLockToggle = nullptr;
    CosmicCheckboxToggle* mUnlockToggle = nullptr;

    QWidget* mLeftCap = nullptr;
    QWidget* mRightCap = nullptr;

    QWidget* mContentHost = nullptr;
    QVBoxLayout* mContentLayout = nullptr;

    QWidget* mResizeGrip = nullptr;
    QTimer* mAutoHideTimer = nullptr;

    bool mAutoHideEnabled = true;
    bool mCollapsed = false;
    bool mIsResizing = false;
    int mPreferredHeight = 220;
    int mDragStartHeight = 220;
    int mDragStartGlobalY = 0;
};

#endif // COSMICBOTTOMTOOLSBAR_H
