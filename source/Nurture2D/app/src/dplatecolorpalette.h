#pragma once
#include <QWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QColor>

class DplateColorPalette : public QWidget {
    Q_OBJECT
public:
    explicit DplateColorPalette(QWidget* parent = nullptr);
    void setActiveColor(const QColor&);
signals:
    void colorChanged(const QColor&);
private:
    void updateGlow(QToolButton* active);
}; 