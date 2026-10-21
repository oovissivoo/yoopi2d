#ifndef COSMICRENAMEOPERATOR_H
#define COSMICRENAMEOPERATOR_H

#include <QObject>
#include <QPointer>

class QLineEdit;
class QWidget;

class CosmicRenameOperator : public QObject
{
    Q_OBJECT

public:
    explicit CosmicRenameOperator(QObject* parent = nullptr);

    void start(QWidget* host, const QRect& rect, const QString& initialText);
    void cancel();
    bool isActive() const;

signals:
    void committed(const QString& newText);
    void canceled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void ensureEditor(QWidget* host);
    void finishCommit();
    void finishCancel();

    QPointer<QWidget> mHost;
    QPointer<QLineEdit> mEdit;
    bool mActive = false;
};

#endif // COSMICRENAMEOPERATOR_H
