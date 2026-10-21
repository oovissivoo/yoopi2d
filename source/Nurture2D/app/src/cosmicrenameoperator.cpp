#include "cosmicrenameoperator.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>
#include <QWidget>

#include "../../CUI/cosmicuimetrics.h"

CosmicRenameOperator::CosmicRenameOperator(QObject* parent)
    : QObject(parent)
{
}

void CosmicRenameOperator::ensureEditor(QWidget* host)
{
    if (!host) {
        return;
    }

    if (mEdit && mEdit->parentWidget() != host) {
        mEdit->deleteLater();
        mEdit = nullptr;
    }

    if (!mEdit) {
        mEdit = new QLineEdit(host);
        mEdit->setFrame(false);
        mEdit->installEventFilter(this);
        mEdit->setStyleSheet(CosmicInlineEdit::stylesheet());
        QFont f = mEdit->font();
        f.setPixelSize(12);
        mEdit->setFont(f);
        mEdit->setTextMargins(0, 0, 0, 0);
    }
}

void CosmicRenameOperator::start(QWidget* host, const QRect& rect, const QString& initialText)
{
    if (!host) {
        return;
    }

    mHost = host;
    ensureEditor(host);

    if (!mEdit) {
        return;
    }

    mEdit->setText(initialText);

    // Height matches the text line, vertically centred in the provided rect
    const QFontMetrics fm(mEdit->font());
    const int editH = fm.height() + 4;
    const QRect editRect(rect.left(),
                         rect.top() + (rect.height() - editH) / 2,
                         rect.width(),
                         editH);
    mEdit->setGeometry(editRect);
    mEdit->show();
    mEdit->raise();

    if (QWidget* w = host->window()) {
        w->raise();
        w->activateWindow();
    }

    mEdit->setFocus(Qt::OtherFocusReason);
    mEdit->grabKeyboard();

    // Some call sites (e.g. context menu actions) can steal focus on the same event loop.
    // Re-assert focus + selection on the next tick for consistent UX.
    const QPointer<QLineEdit> edit = mEdit;
    QTimer::singleShot(0, mEdit, [edit]() {
        if (!edit.data()) {
            return;
        }
        if (QWidget* w = edit->window()) {
            w->raise();
            w->activateWindow();
        }
        edit->setFocus(Qt::OtherFocusReason);
        edit->selectAll();
    });

    mActive = true;
}

void CosmicRenameOperator::cancel()
{
    finishCancel();
}

bool CosmicRenameOperator::isActive() const
{
    return mActive && mEdit && mEdit->isVisible();
}

void CosmicRenameOperator::finishCommit()
{
    if (!mEdit) {
        mActive = false;
        return;
    }

    const QString text = mEdit->text().trimmed();
    if (QWidget::keyboardGrabber() == mEdit) {
        mEdit->releaseKeyboard();
    }
    mEdit->hide();
    mActive = false;
    emit committed(text);
}

void CosmicRenameOperator::finishCancel()
{
    if (mEdit) {
        if (QWidget::keyboardGrabber() == mEdit) {
            mEdit->releaseKeyboard();
        }
        mEdit->hide();
    }
    if (mActive) {
        mActive = false;
        emit canceled();
    }
}

bool CosmicRenameOperator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mEdit) {
        switch (event->type()) {
        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                finishCancel();
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                finishCommit();
                return true;
            }
            break;
        }
        case QEvent::FocusOut:
            finishCommit();
            return true;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}
