#ifndef COSMICLEFTTOOLSBAR_H
#define COSMICLEFTTOOLSBAR_H

#include <QWidget>
#include <QColor>
#include <QVBoxLayout>
#include <QHash>

class CosmicCheckboxToggle;

class CosmicLeftToolsBar : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicLeftToolsBar(QWidget* parent = nullptr);

    static QColor pillDisabledIconColor();

    bool toolsPaneEnabled() const;
    bool toolsOptionsPaneEnabled() const;
    bool brushesPaneEnabled() const;

    void setToolsPaneEnabled(bool enabled);
    void setToolsOptionsPaneEnabled(bool enabled);
    void setBrushesPaneEnabled(bool enabled);

    bool paneEnabled(const QString& paneName) const;
    void setPaneEnabled(const QString& paneName, bool enabled);

    CosmicCheckboxToggle* ensureToggle(const QString& paneName, const QString& iconName);
    void removeToggle(const QString& paneName);

    void setPaneOrder(const QStringList& paneOrder);
    QStringList paneOrder() const;

    void updateToggleOrder(const QStringList& paneOrder);

    // Lock toggle methods
    bool lockEnabled() const;
    void setLockEnabled(bool enabled);

signals:
    void toolsPaneEnabledChanged(bool enabled);
    void toolsOptionsPaneEnabledChanged(bool enabled);
    void brushesPaneEnabledChanged(bool enabled);
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

#endif // COSMICLEFTTOOLSBAR_H
