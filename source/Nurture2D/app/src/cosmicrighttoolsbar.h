#ifndef COSMICRIGHTTOOLSBAR_H
#define COSMICRIGHTTOOLSBAR_H

#include <QWidget>
#include <QColor>
#include <QVBoxLayout>
#include <QHash>

class CosmicCheckboxToggle;

class CosmicRightToolsBar : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicRightToolsBar(QWidget* parent = nullptr);

    static QColor pillDisabledIconColor();

    bool colorWheelPaneEnabled() const;
    bool palettesPaneEnabled() const;
    bool referencePaneEnabled() const;
    bool colorSlidersPaneEnabled() const;

    void setColorWheelPaneEnabled(bool enabled);
    void setPalettesPaneEnabled(bool enabled);
    void setReferencePaneEnabled(bool enabled);
    void setColorSlidersPaneEnabled(bool enabled);

    bool paneEnabled(const QString& paneName) const;
    void setPaneEnabled(const QString& paneName, bool enabled);

    CosmicCheckboxToggle* ensureToggle(const QString& paneName, const QString& iconName);
    void removeToggle(const QString& paneName);

    void setPaneOrder(const QStringList& paneOrder);
    QStringList paneOrder() const;

    // Lock toggle methods
    bool lockEnabled() const;
    void setLockEnabled(bool enabled);

    // Methods to sync toggle order with pane order
    void updateToggleOrder(const QStringList& paneOrder);

signals:
    void colorWheelPaneEnabledChanged(bool enabled);
    void palettesPaneEnabledChanged(bool enabled);
    void referencePaneEnabledChanged(bool enabled);
    void colorSlidersPaneEnabledChanged(bool enabled);
    void paneEnabledChanged(const QString& paneName, bool enabled);
    void lockEnabledChanged(bool enabled);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void updateTabIcon(CosmicCheckboxToggle* toggle, const QString& iconName, bool checked);

    CosmicCheckboxToggle* mTabLock = nullptr;
    QHash<QString, CosmicCheckboxToggle*> mPaneToggles;
    QVBoxLayout* mTabsLayout = nullptr; // Store layout reference for reordering
    QStringList mPaneOrder;
};

#endif // COSMICRIGHTTOOLSBAR_H
