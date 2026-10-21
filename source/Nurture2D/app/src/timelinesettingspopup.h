#ifndef TIMELINESETTINGSPOPUP_H
#define TIMELINESETTINGSPOPUP_H

#include "cosmicpopupwindow.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "cosmicwindowheader.h"
#include "windowsscrollbarspage.h"  // for WindowOptionTile
#include <QPointer>

class TimelineSettingsPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit TimelineSettingsPopup(QWidget* parent = nullptr);

    CosmicInputField* loopStartField()    const { return mLoopStartField; }
    CosmicInputField* loopEndField()      const { return mLoopEndField; }
    CosmicDropDown*   timestampDropDown() const { return mTimestampDropDown; }

    // Scrollbar tiles — caller wires clicked() and calls setActive()
    WindowOptionTile* hScrollBottom() const { return mHScrollBottom; }
    WindowOptionTile* hScrollTop()    const { return mHScrollTop; }
    WindowOptionTile* hScrollBoth()   const { return mHScrollBoth; }
    WindowOptionTile* vScrollLeft()   const { return mVScrollLeft; }
    WindowOptionTile* vScrollRight()  const { return mVScrollRight; }
    WindowOptionTile* vScrollBoth()   const { return mVScrollBoth; }

protected:
    void showEvent(QShowEvent* e) override;

private:
    QPointer<CosmicWindowHeader> mHeader;
    QPointer<CosmicInputField>   mLoopStartField;
    QPointer<CosmicInputField>   mLoopEndField;
    QPointer<CosmicDropDown>     mTimestampDropDown;

    QPointer<WindowOptionTile>   mHScrollBottom;
    QPointer<WindowOptionTile>   mHScrollTop;
    QPointer<WindowOptionTile>   mHScrollBoth;
    QPointer<WindowOptionTile>   mVScrollLeft;
    QPointer<WindowOptionTile>   mVScrollRight;
    QPointer<WindowOptionTile>   mVScrollBoth;
};

#endif // TIMELINESETTINGSPOPUP_H
