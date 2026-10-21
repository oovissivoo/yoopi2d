#pragma once

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QSizePolicy>
#include <QFont>
#include <Qt>

class CosmicCheckboxToggle;

// Lightweight shared layout helper for split tool option panels (left label column + right control column).
//
// Responsibilities:
// - Owns consistent row spacing / alignment for both columns.
// - Creates labels with shared font rules for all tools.
// - Ensures left/right areas and their containers do not paint their own backgrounds so
//   the shared panel background shows through.
//
// createEntry() returns the QLabel* so callers can retranslate it on language change.
// createEntryRow() returns the row container QWidget* for show/hide control.
class CosmicSplitToolLayout
{
public:
    CosmicSplitToolLayout(QVBoxLayout* leftLayout,
                          QVBoxLayout* rightLayout,
                          QWidget* leftAreaWidget,
                          int rowHeight = 22,
                          int spacing = 8)
        : m_leftLayout(leftLayout)
        , m_rightLayout(rightLayout)
        , m_leftArea(leftAreaWidget)
        , m_rowHeight(rowHeight)
    {
        if (m_leftLayout) {
            m_leftLayout->setContentsMargins(0, 0, 0, 0);
            m_leftLayout->setSpacing(spacing);
            m_leftLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            if (QWidget* leftParent = m_leftLayout->parentWidget())
                makeTransparent(leftParent);
        }
        if (m_rightLayout) {
            m_rightLayout->setContentsMargins(0, 0, 0, 0);
            m_rightLayout->setSpacing(spacing);
            m_rightLayout->setAlignment(Qt::AlignRight | Qt::AlignTop);
            if (QWidget* rightParent = m_rightLayout->parentWidget())
                makeTransparent(rightParent);
        }
        if (m_leftArea) {
            makeTransparent(m_leftArea);
            if (QWidget* container = m_leftArea->parentWidget())
                makeTransparent(container);
        }
    }

    CosmicSplitToolLayout(QVBoxLayout* rowsLayout,
                          QWidget* rowsAreaWidget,
                          int rowHeight = 22,
                          int spacing = 8)
        : m_rowsLayout(rowsLayout)
        , m_rowsArea(rowsAreaWidget)
        , m_rowHeight(rowHeight)
    {
        if (m_rowsLayout) {
            m_rowsLayout->setContentsMargins(0, 0, 0, 0);
            m_rowsLayout->setSpacing(spacing);
            m_rowsLayout->setAlignment(Qt::AlignTop);
        }
        if (m_rowsArea) {
            makeTransparent(m_rowsArea);
            if (QWidget* container = m_rowsArea->parentWidget())
                makeTransparent(container);
        }
    }

    // Create a row: label left, control right.
    // Returns the QLabel* so callers can update its text on language change.
    QLabel* createEntry(const QString& labelText, QWidget* control)
    {
        QLabel* label = nullptr;
        buildRow(labelText, control, nullptr, 0, &label, nullptr);
        return label;
    }

    // Same as createEntry() but returns the row container widget instead of the label.
    // Useful when callers need to hide an entire row.
    QWidget* createEntryRow(const QString& labelText, QWidget* control)
    {
        QWidget* row = nullptr;
        buildRow(labelText, control, nullptr, 0, nullptr, &row);
        return row;
    }

    // Create a row with a primary and optional secondary control side-by-side on the right.
    void createEntryWithSecondary(const QString& labelText,
                                  QWidget* primaryControl,
                                  QWidget* secondaryControl,
                                  int secondarySpacing = 4)
    {
        buildRow(labelText, primaryControl, secondaryControl, secondarySpacing, nullptr, nullptr);
    }

private:
    // Core row builder. Fills *outLabel and *outRow if non-null.
    void buildRow(const QString& labelText,
                  QWidget* primaryControl,
                  QWidget* secondaryControl,
                  int secondarySpacing,
                  QLabel** outLabel,
                  QWidget** outRow)
    {
        if ((!m_rowsLayout || !m_rowsArea) && (!m_leftLayout || !m_rightLayout || !m_leftArea))
            return;

        const bool isCheckboxToggle = (qobject_cast<CosmicCheckboxToggle*>(primaryControl) != nullptr);

        if (m_leftLayout && m_rightLayout && m_leftArea) {
            // Two-column layout mode
            QLabel* label = new QLabel(labelText, m_leftArea);
            label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            label->setAutoFillBackground(false);
            label->setAttribute(Qt::WA_TranslucentBackground, true);
            m_leftLayout->addWidget(label);
            if (outLabel) *outLabel = label;

            primaryControl->setSizePolicy(isCheckboxToggle ? QSizePolicy::Fixed : QSizePolicy::Expanding,
                                          QSizePolicy::Fixed);
            if (m_rowHeight > 0)
                primaryControl->setFixedHeight(qMax(m_rowHeight, primaryControl->sizeHint().height()));

            if (secondaryControl) {
                secondaryControl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                if (m_rowHeight > 0)
                    secondaryControl->setFixedHeight(qMax(m_rowHeight, secondaryControl->sizeHint().height()));
            }

            QWidget* row = new QWidget;
            makeTransparent(row);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(0);

            if (isCheckboxToggle) {
                h->addStretch(1);
                h->addWidget(primaryControl, 0, Qt::AlignRight | Qt::AlignVCenter);
            } else {
                h->addWidget(primaryControl);
            }
            if (secondaryControl) {
                if (secondarySpacing > 0) h->addSpacing(secondarySpacing);
                h->addWidget(secondaryControl, 0, Qt::AlignRight | Qt::AlignVCenter);
            }

            m_rightLayout->addWidget(row);
            if (outRow) *outRow = row;
            return;
        }

        // Single-rows layout mode
        primaryControl->setSizePolicy(isCheckboxToggle ? QSizePolicy::Fixed : QSizePolicy::Expanding,
                                      QSizePolicy::Fixed);
        primaryControl->setMinimumWidth(0);
        if (m_rowHeight > 0)
            primaryControl->setFixedHeight(qMax(m_rowHeight, primaryControl->sizeHint().height()));

        if (secondaryControl) {
            secondaryControl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            secondaryControl->setMinimumWidth(0);
            if (m_rowHeight > 0)
                secondaryControl->setFixedHeight(qMax(m_rowHeight, secondaryControl->sizeHint().height()));
        }

        QWidget* row = new QWidget(m_rowsArea);
        makeTransparent(row);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        QWidget* leftArea = new QWidget(row);
        makeTransparent(leftArea);
        leftArea->setMinimumWidth(0);
        leftArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* leftLayout = new QHBoxLayout(leftArea);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(0);
        leftLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel* label = new QLabel(labelText, leftArea);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        label->setAutoFillBackground(false);
        label->setAttribute(Qt::WA_TranslucentBackground, true);
        leftLayout->addWidget(label);
        if (outLabel) *outLabel = label;

        QWidget* rightArea = new QWidget(row);
        makeTransparent(rightArea);
        rightArea->setMinimumWidth(0);
        rightArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* rightLayout = new QHBoxLayout(rightArea);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(0);
        rightLayout->setAlignment(Qt::AlignVCenter);

        if (isCheckboxToggle) {
            rightLayout->addStretch(1);
            rightLayout->addWidget(primaryControl, 0, Qt::AlignRight | Qt::AlignVCenter);
        } else {
            rightLayout->addWidget(primaryControl, 1);
        }
        if (secondaryControl) {
            if (secondarySpacing > 0) rightLayout->addSpacing(secondarySpacing);
            rightLayout->addWidget(secondaryControl, 0, Qt::AlignRight | Qt::AlignVCenter);
        }

        rowLayout->addWidget(leftArea);
        rowLayout->addWidget(rightArea);
        rowLayout->setStretch(0, 1);
        rowLayout->setStretch(1, 1);

        m_rowsLayout->addWidget(row);
        if (outRow) *outRow = row;
    }

    static void makeTransparent(QWidget* w)
    {
        if (!w) return;
        w->setAutoFillBackground(false);
        w->setAttribute(Qt::WA_StyledBackground, false);
    }

    QVBoxLayout* m_leftLayout  = nullptr;
    QVBoxLayout* m_rightLayout = nullptr;
    QWidget*     m_leftArea    = nullptr;
    QVBoxLayout* m_rowsLayout  = nullptr;
    QWidget*     m_rowsArea    = nullptr;
    int          m_rowHeight   = 0;
};
