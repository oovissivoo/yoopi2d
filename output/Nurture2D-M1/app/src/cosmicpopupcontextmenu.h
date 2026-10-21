#ifndef COSMICPOPUPCONTEXTMENU_H
#define COSMICPOPUPCONTEXTMENU_H

#include "cosmicpopupwindow.h"

#include <QIcon>
#include <QPointer>
#include <QString>

class CosmicButton;
class QVBoxLayout;
class QWidget;

class CosmicPopupContextMenu : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicPopupContextMenu(QWidget* parent = nullptr);
    ~CosmicPopupContextMenu() override;

    // Plain item — returns its id when clicked
    void addItem(const QString& text, const QString& id, bool enabled = true, const QIcon& icon = QIcon());

    // Checkable item — toggles visually and returns its id when clicked
    void addCheckableItem(const QString& text, const QString& id, bool checked, bool enabled = true);

    void addSeparator();

    // Blocking exec: shows at globalPos, returns the chosen id (empty = dismissed)
    QString execAt(const QPoint& globalPos);

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void leaveEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void buildUi();

    QPointer<QWidget>     mRoot;
    QPointer<QVBoxLayout> mLayout;

    QString mChosenId;
};

#endif // COSMICPOPUPCONTEXTMENU_H
