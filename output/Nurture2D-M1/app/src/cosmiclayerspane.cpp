/*
 * CosmicLayersPane — inner-layer panel.
 * Drag reorder logic mirrors BrushPresetListWidget (proven pattern).
 * Visuals mirror BrushPresetItem style.
 */
#include "cosmiclayerspane.h"
#include "editor.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "structure/cellbank.h"
#include "structure/celllayerstack.h"
#include "structure/innerlayer.h"
#include "../../CUI/cosmicbutton.h"
#include "../../CUI/cosmichslider.h"
#include "../../CUI/cosmicuimetrics.h"
#include "cosmicpopupcontextmenu.h"
#include "popupcolorwheelwidget.h"
#include "viewmanager.h"
#include "graphics/bitmap/bitmapimage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QPixmap>
#include <QApplication>
#include <QSignalBlocker>
#include <QCursor>
#include <QTimer>
#include <QImageReader>
#include <QFileInfo>
#include <QLineEdit>
#include "filedialog.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>

static const int    kRowH  = 36;
static const QColor kCardBg(0x16,0x16,0x16);
static const QColor kDark  (0x1c,0x1c,0x1c);

// Tag-system colors (mirrors timelinecells.cpp layerTagDefs, excluding default)
struct TintPreset { const char* name; QColor color; };
static const TintPreset kTintPresets[] = {
    {"Red",        QColor(0xf1,0x46,0x3f,0xff)},
    {"Magenta",    QColor(0xe6,0x2a,0x65,0xff)},
    {"Purple",     QColor(0x99,0x36,0xac,0xff)},
    {"Indigo",     QColor(0x63,0x44,0xb2,0xff)},
    {"Blue",       QColor(0x3c,0x57,0xb0,0xff)},
    {"Sky",        QColor(0x1f,0x9a,0xee,0xff)},
    {"Green",      QColor(0x54,0xac,0x58,0xff)},
    {"Leaf",       QColor(0x8f,0xc0,0x57,0xff)},
    {"Lime",       QColor(0xcf,0xd9,0x52,0xff)},
    {"Yellow",     QColor(0xff,0xbf,0x3b,0xff)},
    {"Orange",     QColor(0xfd,0x97,0x31,0xff)},
    {"Orange Red", QColor(0xfc,0x59,0x34,0xff)},
};
static constexpr int kTintPresetCount = int(sizeof(kTintPresets)/sizeof(kTintPresets[0]));

// Draw a squircle icon: border color 0x1f1f1fff, fill = given color (or checker for "none")
static QPixmap squircleIcon(const QColor& fill, int sz = 20) {
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal r = sz * 0.32; // squircle corner radius
    QPainterPath path;
    path.addRoundedRect(QRectF(1.5, 1.5, sz-3, sz-3), r, r);
    if (!fill.isValid()) {
        // Checker pattern for "none"
        const int cs = (sz-3)/4;
        QImage checker(sz-3, sz-3, QImage::Format_ARGB32_Premultiplied);
        checker.fill(Qt::transparent);
        QPainter cp(&checker);
        const QColor c1(0x88,0x88,0x88,0xff), c2(0x44,0x44,0x44,0xff);
        for (int cy=0; cy*cs < checker.height(); ++cy)
            for (int cx=0; cx*cs < checker.width(); ++cx)
                cp.fillRect(cx*cs, cy*cs, cs, cs, ((cx+cy)%2==0)?c1:c2);
        cp.end();
        p.setClipPath(path);
        p.drawImage(QPoint(2,2), checker);
        p.setClipping(false);
    } else {
        p.fillPath(path, fill);
    }
    p.setPen(QPen(QColor(0xcc,0xcc,0xcc,0xff), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    return px;
}

static QPixmap svgPx(const QString& path, const QColor& tint, int sz=18) {
    QSvgRenderer r(path); if(!r.isValid()) return QPixmap();
    QImage img(sz,sz,QImage::Format_ARGB32_Premultiplied); img.fill(Qt::transparent);
    QPainter p(&img); r.render(&p);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn); p.fillRect(img.rect(),tint);
    return QPixmap::fromImage(img);
}

// ── Header container — squircle background matching the papers area ───────────
class LayerHeaderBar : public QWidget {
public:
    explicit LayerHeaderBar(QWidget* parent=nullptr) : QWidget(parent) {
        setFixedHeight(64); // two rows: 32px each
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const qreal r = 6.0;
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5,0.5,-0.5,-0.5), r, r);
        p.fillPath(path, kCardBg);
        // Divider between top and bottom row
        p.setPen(QColor(0x11,0x11,0x11));
        p.drawLine(6, 32, width()-6, 32);
    }
};

// ── Drop indicator (accent line with dots, same as brush pane) ────────────────
class LayerDropIndicator : public QWidget {
public:
    explicit LayerDropIndicator(QWidget* parent):QWidget(parent){
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedHeight(6); hide();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QColor accent=CosmicColor::accent();
        const int y=height()/2, x1=4, x2=width()-4;
        p.setPen(QPen(accent,2,Qt::SolidLine,Qt::FlatCap));
        p.drawLine(x1,y,x2,y);
        p.setPen(QPen(accent,6,Qt::SolidLine,Qt::RoundCap));
        p.drawPoint(x1,y); p.drawPoint(x2,y);
    }
};

// ── Layer item widget ─────────────────────────────────────────────────────────
// Lock column width — constant hit area biased to the lock icon size
static constexpr int kLockColW = 28; // hit area width on the right edge of each row

class LayerItem : public QWidget {
    Q_OBJECT
public:
    explicit LayerItem(int idx, const QString& name, bool vis, bool active, bool locked, QWidget* parent=nullptr)
        : QWidget(parent), mIdx(idx), mActive(active), mVis(vis), mLocked(locked), mName(name)
    {
        setFixedHeight(kRowH);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover,true);
    }
    int     idx()  const { return mIdx; }
    void    setIdx(int i){ mIdx=i; }
    QString name() const { return mName; }
    void    setName(const QString& n){ mName=n; update(); }
    bool    isActive() const { return mActive; }
    void    setActive(bool a){ mActive=a; update(); }
    bool    isVisible2() const { return mVis; }
    void    setVisible2(bool v){ mVis=v; update(); }
    bool    isLocked() const { return mLocked; }
    void    setLocked(bool l){ mLocked=l; update(); }
    void    setTintColor(const QColor& c){ mTint=c; update(); }

signals:
    void selected(int idx);
    void visToggled(int idx);
    void lockToggled(int idx);
    void dragStarted(int idx);
    void dblClicked(int idx, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r=rect();

        // Background
        if(mActive){
            p.fillRect(r,QColor(0x2a,0x2b,0x2a));
        } else {
            p.fillRect(r,kCardBg);
            if(underMouse()) p.fillRect(r,QColor(0xff,0xff,0xff,12));
        }
        // Left accent bar
        if(mTint.isValid()){
            const QColor bar = mVis ? mTint : QColor(mTint.red(),mTint.green(),mTint.blue(),90);
            p.fillRect(0,0,4,r.height(),bar);
        } else if(mActive){
            p.fillRect(0,0,4,r.height(),QColor(0x66,0x66,0x66,0xff));
        }
        // Separator
        p.setPen(QColor(0x11,0x11,0x11));
        p.drawLine(0,r.bottom(),r.right(),r.bottom());
        // Eye icon
        const QColor eyeCol=mVis?QColor(0xcc,0xcc,0xcc):QColor(0x44,0x44,0x44);
        const QPixmap eyePx=svgPx(mVis?":/Interaction/Show.svg":":/Interaction/Hide.svg",eyeCol,18);
        p.drawPixmap(8,(r.height()-18)/2,eyePx);
        // Name (leave room for lock column on the right)
        p.setPen(mActive?QColor(0xff,0xff,0xff):QColor(0xb3,0xb3,0xb3));
        QFont f=p.font(); f.setPointSize(9); p.setFont(f);
        p.drawText(QRect(36,0,r.width()-36-kLockColW,r.height()),Qt::AlignLeft|Qt::AlignVCenter,mName);

        // ── Lock toggle (right side) ──────────────────────────────────────
        const QRect lockHit(r.right()-kLockColW, r.top(), kLockColW, r.height());
        if(mLocked){
            const QColor lockCol(0xcc,0xcc,0xcc,0xff);
            const QPixmap lkPx=svgPx(":/Interaction/LockFloatingState.svg",lockCol,16);
            p.drawPixmap(lockHit.center()-QPoint(8,8), lkPx);
        } else {
            p.setRenderHint(QPainter::Antialiasing,true);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x55,0x55,0x55,0xff));
            p.drawEllipse(lockHit.center(), 4, 4);
            p.setRenderHint(QPainter::Antialiasing,false);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if(e->button()==Qt::LeftButton){
            mPressPos=e->pos();
            mDragStarted=false;
            mSwipeDecided=false;
            mPressConsumedByAction=false;

            // Lock hit area — constant kLockColW strip on the right
            const QRect lockHit(rect().right()-kLockColW, 0, kLockColW, rect().height());
            if(lockHit.contains(e->pos())){
                mPressConsumedByAction=true;
                emit lockToggled(mIdx);
                e->accept(); return;
            }
        }
        QWidget::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if(e->buttons()&Qt::LeftButton){
            const QPoint delta=e->pos()-mPressPos;
            const int thresh=qMax(QApplication::startDragDistance(),8);
            if(!mSwipeDecided && qAbs(delta.y())>thresh){
                mSwipeDecided=true;
                if(!mDragStarted){
                    mDragStarted=true;
                    emit dragStarted(mIdx);
                }
            }
        }
        QWidget::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if(e->button()==Qt::LeftButton){
            if(mPressConsumedByAction){ mPressConsumedByAction=false; e->accept(); return; }
            if(!mDragStarted){
                if(e->pos().x()<36){
                    emit visToggled(mIdx);
                } else {
                    const int idx=mIdx;
                    mPendingSelect=true;
                    QTimer::singleShot(0,this,[this,idx](){
                        if(mPendingSelect){ mPendingSelect=false; emit selected(idx); }
                    });
                }
            }
            mDragStarted=false; mSwipeDecided=false;
        }
        QWidget::mouseReleaseEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if(e->button()==Qt::LeftButton && e->pos().x()>=36){
            mPendingSelect=false;
            emit dblClicked(mIdx, mapToGlobal(e->pos()));
        }
        QWidget::mouseDoubleClickEvent(e);
    }

private:
    int     mIdx; bool mActive,mVis,mLocked; QString mName; QColor mTint;
    QPoint  mPressPos;
    bool    mDragStarted=false, mSwipeDecided=false;
    bool    mPressConsumedByAction=false, mPendingSelect=false;
};

#include "cosmiclayerspane.moc"

// ── CosmicLayersPane ──────────────────────────────────────────────────────────

CosmicLayersPane::CosmicLayersPane(Editor* editor, QWidget* parent)
    : QWidget(parent), mEditor(editor)
{
    setAutoFillBackground(true);
    QPalette pal=palette(); pal.setColor(QPalette::Window,QColor(0x22,0x22,0x22)); setPalette(pal);
    auto* root=new QVBoxLayout(this); root->setContentsMargins(6,6,6,6); root->setSpacing(4);

    // Header — squircle container with kCardBg fill (same as papers area)
    // Two rows: top = opacity slider + lock icon, bottom = tint/add/import buttons
    auto* hdr=new LayerHeaderBar(this);
    auto* vl=new QVBoxLayout(hdr); vl->setContentsMargins(0,0,0,0); vl->setSpacing(0);

    // ── Top row: opacity slider + lock icon ──────────────────────────────────
    auto* topRow=new QWidget(hdr); topRow->setFixedHeight(32);
    auto* hl=new QHBoxLayout(topRow); hl->setContentsMargins(6,0,6,0); hl->setSpacing(4);

    // ── Bottom row: tint, add, import ────────────────────────────────────────
    auto* botRow=new QWidget(hdr); botRow->setFixedHeight(32);
    auto* bl2=new QHBoxLayout(botRow); bl2->setContentsMargins(6,3,6,3); bl2->setSpacing(4);

    mTintButton=new CosmicButton(hdr); mTintButton->setFocusPolicy(Qt::NoFocus);
    mTintButton->setBordered(false);
    mTintButton->setIcon(QIcon(squircleIcon(QColor()))); // default: checker (no tint)
    mTintButton->setIconSize(QSize(16,16)); mTintButton->setToolTip(tr("Paper Tint"));
    mTintButton->setFixedHeight(26);
    mTintButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    connect(mTintButton,&CosmicButton::clicked,this,[this](){
        const int cid=currentCellId(); if(cid<0||!mEditor) return;
        auto* menu=new CosmicPopupContextMenu(window());

        // ── None ──────────────────────────────────────────────────────────
        menu->addItem(tr("None"), QStringLiteral("none"), true, QIcon(squircleIcon(QColor())));

        menu->addSeparator();

        // ── Tag-color presets ──────────────────────────────────────────────
        for(int i=0;i<kTintPresetCount;++i){
            const TintPreset& tp=kTintPresets[i];
            menu->addItem(tr(tp.name), QString("preset_%1").arg(i), true, QIcon(squircleIcon(tp.color)));
        }

        menu->addSeparator();

        // ── Custom color ───────────────────────────────────────────────────
        menu->addItem(tr("Custom Color…"), QStringLiteral("custom"));

        const QString chosen = menu->execAt(mTintButton->mapToGlobal(QPoint(0, mTintButton->height())));
        menu->deleteLater();

        if(chosen.isEmpty()) return;

        if(chosen == QLatin1String("none")){
            applyTint(QColor());
        } else if(chosen.startsWith(QLatin1String("preset_"))){
            bool ok=false;
            const int idx=chosen.mid(7).toInt(&ok);
            if(ok && idx>=0 && idx<kTintPresetCount) applyTint(kTintPresets[idx].color);
        } else if(chosen == QLatin1String("custom")){
            if(!mColorWheelPopup){
                mColorWheelPopup=new PopupColorWheelWidget(window());
                connect(mColorWheelPopup,&PopupColorWheelWidget::colorSelected,this,[this](const QColor& c){
                    applyTint(c);
                });
            }
            // Seed with current tint if any
            const int cid2=currentCellId();
            if(cid2>=0&&mEditor){
                Layer* lay=mEditor->layers()->currentLayer();
                if(lay&&lay->type()==Layer::BITMAP){
                    auto* bl=static_cast<LayerBitmap*>(lay);
                    CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid2):nullptr;
                    if(st&&st->activeLayer()&&st->activeLayer()->tintColor.isValid())
                        mColorWheelPopup->setColor(st->activeLayer()->tintColor);
                }
            }
            const QPoint gp=mTintButton->mapToGlobal(QPoint(0,mTintButton->height()));
            mColorWheelPopup->showAt(gp);
        }
    });

    mAddButton=new CosmicButton(hdr); mAddButton->setFocusPolicy(Qt::NoFocus);
    mAddButton->setBordered(false);
    mAddButton->setIcon(QIcon(svgPx(":/Interaction/TimelineLayerAdd.svg",QColor(0xcc,0xcc,0xcc),16)));
    mAddButton->setIconSize(QSize(16,16)); mAddButton->setToolTip(tr("Add Paper"));
    mAddButton->setFixedHeight(26);
    mAddButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    connect(mAddButton,&CosmicButton::clicked,this,&CosmicLayersPane::addLayerRequested);

    mImportButton=new CosmicButton(hdr); mImportButton->setFocusPolicy(Qt::NoFocus);
    mImportButton->setBordered(false);
    mImportButton->setIcon(QIcon(svgPx(":/Interaction/Import.svg",QColor(0xcc,0xcc,0xcc),16)));
    mImportButton->setIconSize(QSize(16,16)); mImportButton->setToolTip(tr("Import Image as Paper"));
    mImportButton->setFixedHeight(26);
    mImportButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    connect(mImportButton,&CosmicButton::clicked,this,&CosmicLayersPane::importImageToLayerRequested);

    mDeleteButton=new CosmicButton(hdr); mDeleteButton->setFocusPolicy(Qt::NoFocus);
    mDeleteButton->setBordered(false);
    mDeleteButton->setIcon(QIcon(svgPx(":/Interaction/TimelineLayerRemove.svg",QColor(0xcc,0xcc,0xcc),16)));
    mDeleteButton->setIconSize(QSize(16,16)); mDeleteButton->setToolTip(tr("Delete Paper"));
    mDeleteButton->setFixedHeight(26);
    mDeleteButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    connect(mDeleteButton,&CosmicButton::clicked,this,&CosmicLayersPane::removeLayerRequested);

    mOpacitySlider=new CosmicHSlider(Qt::Horizontal,hdr);
    mOpacitySlider->setRange(0,100); mOpacitySlider->setValue(100);
    mOpacitySlider->setLabelMode(CosmicHSlider::Textless);
    mOpacitySlider->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    mOpacitySlider->setFixedHeight(22); mOpacitySlider->setToolTip(tr("Paper Opacity"));
    connect(mOpacitySlider,&QSlider::valueChanged,this,[this](int val){
        const int cid=currentCellId(); if(cid<0||!mEditor) return;
        Layer* lay=mEditor->layers()->currentLayer();
        if(!lay||lay->type()!=Layer::BITMAP) return;
        auto* bl=static_cast<LayerBitmap*>(lay);
        CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
        if(!st) return; InnerLayer* il=st->activeLayer(); if(!il) return;
        il->setOpacity(val/100.0); st->invalidateComposite();
        mEditor->updateUndoContext();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
        mEditor->updateFrame();
    });

    // Lock column header — static icon, same width as per-row lock hit area
    auto* lockHdrLabel = new QLabel(hdr);
    lockHdrLabel->setFixedSize(kLockColW, 28);
    lockHdrLabel->setAlignment(Qt::AlignCenter);
    lockHdrLabel->setPixmap(svgPx(":/Interaction/LockFloatingState.svg", QColor(0x66,0x66,0x66), 16));
    lockHdrLabel->setToolTip(tr("Lock / Unlock Paper"));
    mLockHdrLabel = lockHdrLabel;

    // Top row: opacity slider + lock icon
    hl->addWidget(mOpacitySlider,1); hl->addWidget(lockHdrLabel);

    // Bottom row: tint, add, import, delete — all expand equally
    bl2->addWidget(mTintButton,1); bl2->addWidget(mAddButton,1); bl2->addWidget(mImportButton,1); bl2->addWidget(mDeleteButton,1);

    vl->addWidget(botRow); vl->addWidget(topRow);
    root->addWidget(hdr);

    // Card — papers list area with accent border
    auto* card = new QFrame(this);
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(QString(
        "QFrame#papersCard { background:%1; border:1px solid %2; border-radius:4px; }"
        "QFrame#papersCard > QScrollArea { border:none; }")
        .arg(QLatin1String("#161616"), CosmicColor::accentHex()));
    card->setObjectName("papersCard");
    auto* cl=new QVBoxLayout(card); cl->setContentsMargins(1,1,1,1); cl->setSpacing(0);

    mEmptyStateLabel=new QLabel(tr("No bitmap cell active"),card);
    mEmptyStateLabel->setAlignment(Qt::AlignCenter);
    { QPalette p=mEmptyStateLabel->palette(); p.setColor(QPalette::WindowText,QColor(0x55,0x55,0x55)); mEmptyStateLabel->setPalette(p); }
    mEmptyStateLabel->setFont(QFont(font().family(),9));
    mEmptyStateLabel->setVisible(false); cl->addWidget(mEmptyStateLabel);

    mScrollArea=new QScrollArea(card);
    mScrollArea->setFrameShape(QFrame::NoFrame);
    mScrollArea->setWidgetResizable(true);
    mScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mScrollArea->setStyleSheet(
        "QScrollArea{background:#161616;border:none;}"
        "QScrollArea > QWidget > QWidget{background:#161616;}"
        "QScrollBar:vertical{background:#1f1f1f;width:12px;border:none;margin:0;}"
        "QScrollBar::handle:vertical{background:#505050;border-radius:6px;min-height:26px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    if(mScrollArea->viewport()){
        mScrollArea->viewport()->setAutoFillBackground(true);
        QPalette vp=mScrollArea->viewport()->palette();
        vp.setColor(QPalette::Window,kCardBg);
        mScrollArea->viewport()->setPalette(vp);
    }

    mContainerWidget=new QWidget(mScrollArea);
    mContainerWidget->setAutoFillBackground(false);
    mContainerLayout=new QVBoxLayout(mContainerWidget);
    mContainerLayout->setContentsMargins(0,0,0,0); mContainerLayout->setSpacing(0);
    mContainerLayout->addStretch(1);
    mScrollArea->setWidget(mContainerWidget);
    cl->addWidget(mScrollArea,1);

    // Drop indicator (parented to scroll area viewport, same as brush pane)
    mDropIndicator=new LayerDropIndicator(mScrollArea->viewport());
    mDropIndicator->installEventFilter(this);

    // Image drag-and-drop indicator (accent bar, separate from reorder indicator)
    mImageDropIndicator = new QWidget(mScrollArea->viewport());
    mImageDropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    mImageDropIndicator->setFixedHeight(3);
    mImageDropIndicator->hide();
    mImageDropIndicator->setStyleSheet(
        QString("background: %1; border-radius: 1px;").arg(CosmicColor::accentHex()));

    // Enable drops on the scroll area viewport
    mScrollArea->viewport()->setAcceptDrops(true);
    mScrollArea->viewport()->installEventFilter(this);

    root->addWidget(card,1);

    connectSignals();
    refresh();
}

int CosmicLayersPane::currentCellId() const {
    if(!mEditor) return -1;
    Layer* lay=mEditor->layers()->currentLayer();
    if(!lay||lay->type()!=Layer::BITMAP) return -1;
    auto* bl=static_cast<LayerBitmap*>(lay);
    int fr=mEditor->currentFrame();
    KeyFrame* kf=bl->getKeyFrameAt(fr); if(!kf) kf=bl->getLastKeyFrameAtPosition(fr);
    if(!kf) return -1;
    auto* bkf=dynamic_cast<BitmapKeyFrame*>(kf);
    return bkf?bkf->cellId():-1;
}

void CosmicLayersPane::refresh() {
    const int cid=currentCellId();
    if(cid<0){
        mEmptyStateLabel->setVisible(true); mScrollArea->setVisible(false);
        mAddButton->setEnabled(false); mImportButton->setEnabled(false); mDeleteButton->setEnabled(false); mOpacitySlider->setEnabled(false); mTintButton->setEnabled(false);
        return;
    }
    Layer* lay=mEditor->layers()->currentLayer();
    auto* bl=static_cast<LayerBitmap*>(lay);
    CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
    if(!st){
        mEmptyStateLabel->setVisible(true); mScrollArea->setVisible(false);
        mAddButton->setEnabled(false); mImportButton->setEnabled(false); mDeleteButton->setEnabled(false); mOpacitySlider->setEnabled(false); mTintButton->setEnabled(false);
        return;
    }
    mEmptyStateLabel->setVisible(false); mScrollArea->setVisible(true);
    mAddButton->setEnabled(true); mImportButton->setEnabled(true); mDeleteButton->setEnabled(true); mOpacitySlider->setEnabled(true);
    mTintButton->setEnabled(true);
    updateTintButtonIcon();

    const int cnt=st->layerCount(), ai=st->activeIndex();

    if(InnerLayer* il=st->activeLayer()){
        QSignalBlocker sb(mOpacitySlider);
        mOpacitySlider->setValue(qRound(il->opacity*100.0));
    }

    // Rebuild item list — top visual = highest logical index (CSP convention)
    while(QLayoutItem* li=mContainerLayout->takeAt(0)){ if(li->widget()) li->widget()->deleteLater(); delete li; }
    mItems.clear();

    for(int i=cnt-1;i>=0;--i){
        const InnerLayer* il=st->layerAt(i); if(!il) continue;
        auto* item=new LayerItem(i,il->name,il->visible,i==ai,il->locked,mContainerWidget);
        if(il->tintColor.isValid()) item->setTintColor(il->tintColor);
        connect(item,&LayerItem::selected,   this,[this](int idx){ onItemSelected(idx); });
        connect(item,&LayerItem::visToggled, this,[this](int idx){ onItemVisToggled(idx); });
        connect(item,&LayerItem::lockToggled,this,[this](int idx){ onItemLockToggled(idx); });
        connect(item,&LayerItem::dragStarted,this,[this,item](int /*logicalIdx*/){
            // Find visual index (position in mItems list)
            const int vi=mItems.indexOf(item);
            if(vi>=0) startDrag(vi);
        });
        connect(item,&LayerItem::dblClicked, this,[this](int idx,QPoint){ onItemDblClicked(idx); });
        mContainerLayout->addWidget(item);
        mItems.append(item);
    }
    mContainerLayout->addStretch(1);
    mContainerWidget->setMinimumHeight(cnt*kRowH);
}

void CosmicLayersPane::onItemSelected(int logicalIdx) {
    const int cid=currentCellId(); if(cid<0||!mEditor) return;
    mEditor->onActiveInnerLayerChanged(cid,logicalIdx);
    // Update active highlight in-place — no full rebuild needed.
    for(LayerItem* it : mItems){
        if(it) it->setActive(it->idx()==logicalIdx);
    }
    // Update opacity slider to reflect newly active layer
    Layer* lay=mEditor->layers()->currentLayer();
    if(lay&&lay->type()==Layer::BITMAP){
        auto* bl=static_cast<LayerBitmap*>(lay);
        CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
        if(st){ if(InnerLayer* il=st->activeLayer()){
            QSignalBlocker sb(mOpacitySlider);
            mOpacitySlider->setValue(qRound(il->opacity*100.0));
        }}
    }
}

void CosmicLayersPane::onItemVisToggled(int logicalIdx) {
    const int cid=currentCellId(); if(cid<0||!mEditor) return;
    Layer* lay=mEditor->layers()->currentLayer();
    if(!lay||lay->type()!=Layer::BITMAP) return;
    auto* bl=static_cast<LayerBitmap*>(lay);
    CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
    if(!st) return;
    InnerLayer* il=st->layerAt(logicalIdx); if(!il) return;
    il->visible=!il->visible; st->invalidateComposite();
    mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
    mEditor->updateFrame(); refresh();
}

void CosmicLayersPane::onItemLockToggled(int logicalIdx) {
    const int cid=currentCellId(); if(cid<0||!mEditor) return;
    Layer* lay=mEditor->layers()->currentLayer();
    if(!lay||lay->type()!=Layer::BITMAP) return;
    auto* bl=static_cast<LayerBitmap*>(lay);
    CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
    if(!st) return;
    InnerLayer* il=st->layerAt(logicalIdx); if(!il) return;
    il->locked=!il->locked;
    // Update the item in-place — no full rebuild needed
    for(LayerItem* it : mItems){
        if(it && it->idx()==logicalIdx){ it->setLocked(il->locked); break; }
    }
}

void CosmicLayersPane::onItemDblClicked(int logicalIdx) {
    startInlineRename(logicalIdx);
}

QRect CosmicLayersPane::inlineRenameRect(int logicalIdx) const {
    if (!mScrollArea) return QRect();
    QWidget* vp = mScrollArea->viewport();

    // Find the LayerItem for this logical index
    LayerItem* item = nullptr;
    for (LayerItem* it : mItems) {
        if (it && it->idx() == logicalIdx) { item = it; break; }
    }
    if (!item) return QRect();

    // Map item's name-text area into viewport coordinates
    const int scrollOff = mScrollArea->verticalScrollBar() ? mScrollArea->verticalScrollBar()->value() : 0;
    const QPoint itemInContainer = item->pos(); // item is in mContainerWidget
    const QPoint containerInVp   = mContainerWidget->mapTo(vp, QPoint(0, 0));
    const int itemY = containerInVp.y() + itemInContainer.y();

    const QFontMetrics fm(item->font());
    const int editH = fm.height() + 4;
    const int editX = 36;
    const int editW = vp->width() - editX - 8;
    const int editY = itemY + (item->height() - editH) / 2;

    Q_UNUSED(scrollOff);
    return QRect(editX, editY, editW, editH);
}

void CosmicLayersPane::startInlineRename(int logicalIdx) {
    if (mIsInlineEditing) return;

    const QRect r = inlineRenameRect(logicalIdx);
    if (!r.isValid()) return;

    // Fetch current name
    QString currentName;
    const int cid = currentCellId();
    if (cid >= 0 && mEditor) {
        Layer* lay = mEditor->layers()->currentLayer();
        if (lay && lay->type() == Layer::BITMAP) {
            auto* bl = static_cast<LayerBitmap*>(lay);
            CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(cid) : nullptr;
            if (st) { if (const InnerLayer* il = st->layerAt(logicalIdx)) currentName = il->name; }
        }
    }

    mRenamingIndex = logicalIdx;
    mIsInlineEditing = true;

    // Lazily create the editor, parented to the viewport (stable, never rebuilt)
    if (!mInlineEditor) {
        mInlineEditor = new QLineEdit(mScrollArea->viewport());
        mInlineEditor->setFrame(false);
        mInlineEditor->setTextMargins(0, 0, 0, 0);
        mInlineEditor->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mInlineEditor->setStyleSheet(CosmicInlineEdit::stylesheet());
        mInlineEditor->installEventFilter(this);
        QFont f = mInlineEditor->font();
        f.setPixelSize(12);
        mInlineEditor->setFont(f);
    }

    mInlineEditor->setText(currentName);
    mInlineEditor->setGeometry(r);
    mInlineEditor->show();
    mInlineEditor->raise();
    mInlineEditor->setFocus(Qt::OtherFocusReason);
    mInlineEditor->grabKeyboard();
    mInlineEditor->selectAll();

    if (!mHasAppEventFilter) {
        qApp->installEventFilter(this);
        mHasAppEventFilter = true;
    }
}

void CosmicLayersPane::finishInlineRename(bool accept) {
    if (!mIsInlineEditing) return;

    if (accept && mRenamingIndex >= 0) {
        const QString name = mInlineEditor->text().trimmed();
        if (!name.isEmpty()) {
            const int cid = currentCellId();
            if (cid >= 0 && mEditor) {
                Layer* lay = mEditor->layers()->currentLayer();
                if (lay && lay->type() == Layer::BITMAP) {
                    auto* bl = static_cast<LayerBitmap*>(lay);
                    CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(cid) : nullptr;
                    if (st) { if (InnerLayer* il = st->layerAt(mRenamingIndex)) il->name = name; }
                }
            }
        }
    }

    if (mInlineEditor) {
        if (QWidget::keyboardGrabber() == mInlineEditor)
            mInlineEditor->releaseKeyboard();
        mInlineEditor->hide();
    }

    mIsInlineEditing = false;
    mRenamingIndex   = -1;

    if (mHasAppEventFilter) {
        qApp->removeEventFilter(this);
        mHasAppEventFilter = false;
    }

    refresh();
}

// ── Drag reorder — mirrors BrushPresetListWidget exactly ─────────────────────

void CosmicLayersPane::startDrag(int visualIndex) {
    if(mIsDragging||visualIndex<0||visualIndex>=mItems.size()) return;
    mIsDragging=true; mDragFromIndex=visualIndex; mDropIndex=visualIndex;

    // Install event filter on all items (same as brush pane)
    for(LayerItem* it:mItems) if(it) it->installEventFilter(this);

    if(mDropIndicator&&mScrollArea&&mScrollArea->viewport()){
        QWidget* vp=mScrollArea->viewport();
        const int scrollOff=mScrollArea->verticalScrollBar()?mScrollArea->verticalScrollBar()->value():0;
        const int lineY=visualIndex*kRowH-scrollOff;
        mDropIndicator->setGeometry(0,lineY-3,vp->width(),6);
        mDropIndicator->raise(); mDropIndicator->show();
    }
    if(mScrollArea&&mScrollArea->viewport()) mScrollArea->viewport()->update();
}

void CosmicLayersPane::updateDrag(const QPoint& globalPos) {
    if(!mIsDragging) return;
    const int nd=getDropIndex(globalPos);
    if(nd!=mDropIndex){
        mDropIndex=nd;
        if(mDropIndicator&&mScrollArea&&mScrollArea->viewport()){
            QWidget* vp=mScrollArea->viewport();
            const int scrollOff=mScrollArea->verticalScrollBar()?mScrollArea->verticalScrollBar()->value():0;
            const int lineY=mDropIndex*kRowH-scrollOff;
            mDropIndicator->setGeometry(0,lineY-3,vp->width(),6);
            mDropIndicator->update();
        }
    }
}

void CosmicLayersPane::endDrag() {
    if(!mIsDragging) return;
    mIsDragging=false;
    for(LayerItem* it:mItems) if(it) it->removeEventFilter(this);
    if(mDropIndicator) mDropIndicator->hide();

    const int from=mDragFromIndex;
    // Same conversion as brush pane: gap slot -> item index
    int to=mDropIndex;
    if(to>from) to-=1;
    to=qBound(0,to,(int)mItems.size()-1);

    if(from!=to && from>=0 && from<(int)mItems.size()){
        // Convert visual indices to logical indices
        // Visual 0 = top = highest logical index
        const int cnt=(int)mItems.size();
        const int fromLogical=cnt-1-from;
        const int toLogical=cnt-1-to;

        const int cid=currentCellId();
        if(cid>=0&&mEditor){
            Layer* lay=mEditor->layers()->currentLayer();
            if(lay&&lay->type()==Layer::BITMAP){
                auto* bl=static_cast<LayerBitmap*>(lay);
                CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
                if(st){
                    st->moveLayer(fromLogical,toLogical);
                    st->invalidateComposite();
                    // Make the moved layer the active one so it's visually highlighted
                    mEditor->onActiveInnerLayerChanged(cid, toLogical);
                    mEditor->updateUndoContext();
                    mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
                    mEditor->updateFrame();
                }
            }
        }
    }
    // No-reorder case: LayerItem::mouseReleaseEvent already emitted selected() if no drag occurred

    mDragFromIndex=-1; mDropIndex=-1;
    if(mScrollArea&&mScrollArea->viewport()) mScrollArea->viewport()->update();
    refresh();
}

int CosmicLayersPane::getDropIndex(const QPoint& globalPos) const {
    if(!mContainerWidget) return 0;
    const QPoint local=mContainerWidget->mapFromGlobal(globalPos);
    // Same formula as brush pane: (y + rowH/2) / rowH
    return qBound(0,(local.y()+kRowH/2)/kRowH,(int)mItems.size());
}

bool CosmicLayersPane::eventFilter(QObject* watched, QEvent* event) {
    // ── Inline rename: editor events ────────────────────────────────────────
    if (mIsInlineEditing && watched == mInlineEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                finishInlineRename(true);
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                finishInlineRename(false);
                return true;
            }
        }
        // Suppress FocusOut — we commit via app-level MouseButtonPress instead,
        // exactly like TimeLineCells does.
        if (event->type() == QEvent::FocusOut) {
            return false;
        }
    }

    // ── Inline rename: app-level outside-click commit ────────────────────────
    if (mIsInlineEditing &&
        watched != mInlineEditor &&
        event->type() == QEvent::MouseButtonPress)
    {
        auto* me = static_cast<QMouseEvent*>(event);
        const bool insideEditor = mInlineEditor && mInlineEditor->isVisible() &&
                                  mInlineEditor->rect().contains(mInlineEditor->mapFromGlobal(me->globalPos()));
        if (!insideEditor) {
            finishInlineRename(true);
            return false;
        }
    }

    // ── Drop indicator paint ─────────────────────────────────────────────────
    if (watched == mDropIndicator && event->type() == QEvent::Paint) {
        return false;
    }

    // ── Image drag-and-drop: viewport events ─────────────────────────────────
    if (mScrollArea && watched == mScrollArea->viewport()) {
        switch (event->type()) {
        case QEvent::DragEnter:
            handleImageDragEnter(static_cast<QDragEnterEvent*>(event));
            return true;
        case QEvent::DragMove:
            handleImageDragMove(static_cast<QDragMoveEvent*>(event));
            return true;
        case QEvent::DragLeave:
            hideImageDropIndicator();
            return true;
        case QEvent::Drop:
            handleImageDrop(static_cast<QDropEvent*>(event));
            return true;
        default:
            break;
        }
    }

    // ── Drag: intercept move/release from child items ────────────────────────
    if (mIsDragging) {
        if (event->type() == QEvent::MouseMove)          { updateDrag(QCursor::pos()); return true; }
        if (event->type() == QEvent::MouseButtonRelease) { endDrag(); return true; }
    }

    return QWidget::eventFilter(watched, event);
}

void CosmicLayersPane::mouseMoveEvent(QMouseEvent* e) {
    if(mIsDragging) updateDrag(QCursor::pos());
    QWidget::mouseMoveEvent(e);
}

void CosmicLayersPane::mouseReleaseEvent(QMouseEvent* e) {
    if(mIsDragging) endDrag();
    QWidget::mouseReleaseEvent(e);
}

// ── Image drag-and-drop into papers pane ─────────────────────────────────────

static bool urlsHaveImages(const QList<QUrl>& urls)
{
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QFileInfo fi(path);
        if (fi.isDir()) {
            for (const QFileInfo& f : QDir(path).entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot))
                if (QImageReader(f.absoluteFilePath()).canRead()) return true;
        } else if (QImageReader(path).canRead()) {
            return true;
        }
    }
    return false;
}

static QList<QString> collectImagePaths(const QList<QUrl>& urls)
{
    QList<QString> paths;
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QFileInfo fi(path);
        if (fi.isDir()) {
            for (const QFileInfo& f : QDir(path).entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot))
                if (QImageReader(f.absoluteFilePath()).canRead()) paths << f.absoluteFilePath();
        } else if (QImageReader(path).canRead()) {
            paths << path;
        }
    }
    return paths;
}

int CosmicLayersPane::getImageDropIndex(const QPoint& viewportPos) const
{
    if (!mContainerWidget) return 0;
    const QPoint local = mContainerWidget->mapFrom(mScrollArea->viewport(), viewportPos);
    // Each item is kRowH tall; gap between items snaps at midpoint
    const int count = mItems.size();
    if (count == 0) return 0;
    const int slot = qBound(0, (local.y() + kRowH / 2) / kRowH, count);
    return slot;
}

void CosmicLayersPane::updateImageDropIndicator(const QPoint& viewportPos)
{
    if (!mImageDropIndicator || !mScrollArea) return;
    QWidget* vp = mScrollArea->viewport();
    const int slot = getImageDropIndex(viewportPos);
    const int scrollOff = mScrollArea->verticalScrollBar() ? mScrollArea->verticalScrollBar()->value() : 0;
    const int lineY = slot * kRowH - scrollOff - 1;
    mImageDropIndicator->setGeometry(4, lineY, vp->width() - 8, 3);
    mImageDropIndicator->raise();
    mImageDropIndicator->show();
}

void CosmicLayersPane::hideImageDropIndicator()
{
    if (mImageDropIndicator) mImageDropIndicator->hide();
    mIsImageDragging = false;
}

void CosmicLayersPane::handleImageDragEnter(QDragEnterEvent* e)
{
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }
    if (!urlsHaveImages(e->mimeData()->urls())) { e->ignore(); return; }
    // Only accept if we have an active cell to drop into
    if (currentCellId() < 0) { e->ignore(); return; }
    mIsImageDragging = true;
    updateImageDropIndicator(e->pos());
    e->acceptProposedAction();
}

void CosmicLayersPane::handleImageDragMove(QDragMoveEvent* e)
{
    if (!mIsImageDragging) { e->ignore(); return; }
    updateImageDropIndicator(e->pos());
    e->acceptProposedAction();
}

void CosmicLayersPane::handleImageDrop(QDropEvent* e)
{
    hideImageDropIndicator();
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }
    const QList<QString> paths = collectImagePaths(e->mimeData()->urls());
    if (paths.isEmpty()) { e->ignore(); return; }
    const int cid = currentCellId();
    if (cid < 0 || !mEditor) { e->ignore(); return; }

    Layer* lay = mEditor->layers()->currentLayer();
    if (!lay || lay->type() != Layer::BITMAP) { e->ignore(); return; }
    auto* bl = static_cast<LayerBitmap*>(lay);
    if (!bl->cellBank()) { e->ignore(); return; }

    // Visual slot 0 = top (highest logical index), slot count = bottom.
    // addLayer(name, aboveIndex) inserts at aboveIndex+1 (minimum index 1).
    // For the bottom slot (targetLogical == 0), insert at 1 then move down to 0.
    const int visualSlot = getImageDropIndex(e->pos());
    const int count = mItems.size();
    const int targetLogical = count - visualSlot;
    // aboveIndex: to land at targetLogical we need aboveIndex = targetLogical - 1.
    // Clamp to 0 so we never pass -1 (which uses active index).
    int aboveIndex = qMax(0, targetLogical - 1);
    const bool insertAtBottom = (targetLogical == 0);

    const QSize canvasSize = mEditor->view()->canvasSize();

    for (int i = 0; i < paths.size(); ++i) {
        QImageReader reader(paths[i]);
        QImage img = reader.read();
        if (img.isNull()) continue;

        if (canvasSize.width() > 0 && canvasSize.height() > 0)
            img = img.scaled(canvasSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const QPoint topLeft(-img.width() / 2, -img.height() / 2);
        const QString layerName = QFileInfo(paths[i]).baseName();

        const int newIdx = bl->cellBank()->addInnerLayer(cid, layerName, aboveIndex);
        // If dropping at the bottom, move the new layer down to index 0
        if (insertAtBottom) {
            CellLayerStack* st2 = bl->cellBank()->getCellLayerStack(cid);
            if (st2) st2->moveLayer(newIdx, 0);
            bl->cellBank()->setActiveInnerLayerIndex(cid, 0);
        } else {
            bl->cellBank()->setActiveInnerLayerIndex(cid, newIdx);
        }
        CellLayerStack* st = bl->cellBank()->getCellLayerStack(cid);
        if (!st) continue;
        InnerLayer* il = st->activeLayer();
        if (!il || !il->image) continue;

        BitmapImage importedBmp(topLeft, img);
        il->image->paste(&importedBmp);
        st->invalidateComposite();

        // Each subsequent image goes one slot higher
        aboveIndex = insertAtBottom ? 0 : newIdx; // next image inserts above the one we just added
    }

    mEditor->updateUndoContext();
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    mEditor->updateFrame();
    refresh();
    e->acceptProposedAction();
}

void CosmicLayersPane::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange) return;

    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Button tooltips
    if (mTintButton)    mTintButton->setToolTip(safetr("Paper Tint"));
    if (mAddButton)     mAddButton->setToolTip(safetr("Add Paper"));
    if (mImportButton)  mImportButton->setToolTip(safetr("Import Image as Paper"));
    if (mDeleteButton)  mDeleteButton->setToolTip(safetr("Delete Paper"));
    if (mOpacitySlider) mOpacitySlider->setToolTip(safetr("Paper Opacity"));
    if (mLockHdrLabel)  mLockHdrLabel->setToolTip(safetr("Lock / Unlock Paper"));

    // Empty state label
    if (mEmptyStateLabel) mEmptyStateLabel->setText(safetr("No bitmap cell active"));

    // Note: tint context menu items (None, color names, Custom Color…) are
    // created fresh on every click via tr() in the lambda — always translated.
}

void CosmicLayersPane::connectSignals() {
    connect(this,&CosmicLayersPane::addLayerRequested,this,[this](){
        const int cid=currentCellId(); if(cid<0||!mEditor) return;
        Layer* lay=mEditor->layers()->currentLayer();
        if(!lay||lay->type()!=Layer::BITMAP) return;
        auto* bl=static_cast<LayerBitmap*>(lay); if(!bl->cellBank()) return;
        CellLayerStack* st=bl->cellBank()->getCellLayerStack(cid);
        int maxN=0;
        if(st){ for(int i=0;i<st->layerCount();++i){ const InnerLayer* il=st->layerAt(i); if(!il) continue; const QString& n=il->name; if(n.startsWith("Paper ")){ bool ok=false; int num=n.mid(6).toInt(&ok); if(ok&&num>maxN) maxN=num; } } }
        bl->cellBank()->addInnerLayer(cid, tr("Paper %1").arg(maxN+1));
        mEditor->updateUndoContext();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
        mEditor->updateFrame(); refresh();
    });
    connect(this,&CosmicLayersPane::removeLayerRequested,this,[this](){
        const int cid=currentCellId(); if(cid<0||!mEditor) return;
        Layer* lay=mEditor->layers()->currentLayer();
        if(!lay||lay->type()!=Layer::BITMAP) return;
        auto* bl=static_cast<LayerBitmap*>(lay);
        CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
        if(!st||st->layerCount()<=1) return;
        bl->cellBank()->removeInnerLayer(cid,st->activeIndex());
        st->invalidateComposite();
        mEditor->updateUndoContext();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
        mEditor->updateFrame(); refresh();
    });
    connect(this,&CosmicLayersPane::importImageToLayerRequested,this,[this](){
        const int cid=currentCellId(); if(cid<0||!mEditor) return;
        Layer* lay=mEditor->layers()->currentLayer();
        if(!lay||lay->type()!=Layer::BITMAP) return;
        auto* bl=static_cast<LayerBitmap*>(lay); if(!bl->cellBank()) return;

        const QString filePath=FileDialog::getOpenFileName(
            this, FileType::IMAGE, tr("Import Image as Layer"));
        if(filePath.isEmpty()) return;

        QImageReader reader(filePath);
        QImage img=reader.read();
        if(img.isNull()) return;

        // Scale image to fit canvas, preserving aspect ratio.
        // "Extra space" is handled by using the full canvas rect as the target.
        const QSize canvasSize=mEditor->view()->canvasSize();
        if(canvasSize.width()>0 && canvasSize.height()>0 && !img.isNull()){
            img=img.scaled(canvasSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Center on canvas
        const QPoint topLeft(-img.width()/2, -img.height()/2);

        // Add a new inner layer named after the file
        const QString layerName=QFileInfo(filePath).baseName();
        CellLayerStack* st=bl->cellBank()->getCellLayerStack(cid);
        const int newIdx=bl->cellBank()->addInnerLayer(cid, layerName);
        bl->cellBank()->setActiveInnerLayerIndex(cid, newIdx);
        st=bl->cellBank()->getCellLayerStack(cid);
        if(!st) return;
        InnerLayer* il=st->activeLayer(); if(!il||!il->image) return;

        // Paste the scaled image into the new inner layer's BitmapImage
        BitmapImage importedBmp(topLeft, img);
        il->image->paste(&importedBmp);
        st->invalidateComposite();

        mEditor->updateUndoContext();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
        mEditor->updateFrame(); refresh();
    });
    if(mEditor){
        connect(mEditor,&Editor::scrubbed,      this,[this](int){ refresh(); });
        connect(mEditor,&Editor::frameModified, this,[this](int){ refresh(); });
        connect(mEditor,&Editor::framesModified,this,[this](){ refresh(); });
        connect(mEditor,&Editor::objectLoaded,  this,[this](){ refresh(); });
        connect(mEditor->layers(),&LayerManager::currentLayerChanged,this,[this](int){ refresh(); });
    }
}

void CosmicLayersPane::applyTint(const QColor& color) {
    const int cid=currentCellId(); if(cid<0||!mEditor) return;
    Layer* lay=mEditor->layers()->currentLayer();
    if(!lay||lay->type()!=Layer::BITMAP) return;
    auto* bl=static_cast<LayerBitmap*>(lay);
    CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
    if(!st) return;
    InnerLayer* il=st->activeLayer(); if(!il) return;
    il->tintColor=color;
    st->invalidateComposite();
    mEditor->setModified(mEditor->layers()->currentLayerIndex(),mEditor->currentFrame());
    mEditor->updateFrame();
    updateTintButtonIcon();
}

void CosmicLayersPane::updateTintButtonIcon() {
    if(!mTintButton) return;
    const int cid=currentCellId();
    QColor tint;
    if(cid>=0&&mEditor){
        Layer* lay=mEditor->layers()->currentLayer();
        if(lay&&lay->type()==Layer::BITMAP){
            auto* bl=static_cast<LayerBitmap*>(lay);
            CellLayerStack* st=bl->cellBank()?bl->cellBank()->getCellLayerStack(cid):nullptr;
            if(st&&st->activeLayer()) tint=st->activeLayer()->tintColor;
        }
    }
    mTintButton->setIcon(QIcon(squircleIcon(tint)));
}

void CosmicLayersPane::setLassoRestricted(bool restricted)
{
    // Disable/enable every interactive control in the header and the layer list.
    if (mAddButton)     mAddButton->setEnabled(!restricted);
    if (mImportButton)  mImportButton->setEnabled(!restricted);
    if (mOpacitySlider) mOpacitySlider->setEnabled(!restricted);
    if (mTintButton)    mTintButton->setEnabled(!restricted);
    // Block layer selection, vis toggle, and drag reorder
    if (mScrollArea)    mScrollArea->setEnabled(!restricted);
}

// ── Shortcut-triggered inner-layer navigation ─────────────────────────────────
// The inner-layer stack is ordered 0 = bottom, (count-1) = top.
// In the UI the topmost row is the highest index, so "above" = index + 1.
void CosmicLayersPane::selectPaperAbove()
{
    const int cid = currentCellId();
    if (cid < 0 || !mEditor) return;
    Layer* lay = mEditor->layers()->currentLayer();
    if (!lay || lay->type() != Layer::BITMAP) return;
    auto* bl = static_cast<LayerBitmap*>(lay);
    CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(cid) : nullptr;
    if (!st || st->layerCount() <= 1) return;
    const int current = st->activeIndex();
    if (current >= st->layerCount() - 1) return; // already at top
    mEditor->onActiveInnerLayerChanged(cid, current + 1);
    refresh();
}

void CosmicLayersPane::selectPaperBelow()
{
    const int cid = currentCellId();
    if (cid < 0 || !mEditor) return;
    Layer* lay = mEditor->layers()->currentLayer();
    if (!lay || lay->type() != Layer::BITMAP) return;
    auto* bl = static_cast<LayerBitmap*>(lay);
    CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(cid) : nullptr;
    if (!st || st->layerCount() <= 1) return;
    const int current = st->activeIndex();
    if (current <= 0) return; // already at bottom
    mEditor->onActiveInnerLayerChanged(cid, current - 1);
    refresh();
}
