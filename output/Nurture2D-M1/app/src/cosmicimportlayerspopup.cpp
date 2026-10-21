#include "cosmicimportlayerspopup.h"

#include "cosmicwindowheader.h"
#include "filedialog.h"
#include "fileformat.h"
#include "layermanager.h"
#include "soundmanager.h"
#include "layer.h"
#include "layersound.h"
#include "layervector.h"
#include "layerbitmap.h"
#include "cellbank.h"
#include "bitmapimage.h"
#include "soundclip.h"
#include "n2dprojectio.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QParallelAnimationGroup>
#include "cosmicprogresspopup.h"
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {

QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    const int r = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "QWidget{ background: #1e1e1e; border-radius: %1px; }").arg(r));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(10);
    return card;
}

QLabel* makeLabel(const QString& text, QWidget* parent,
                  int fontSize = 11, bool muted = false)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "QLabel{ background: transparent; color: %1; font-size: %2px; }")
        .arg(muted ? QStringLiteral("#777777") : QStringLiteral("#b3b3b3"))
        .arg(fontSize));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-size: 11px; font-weight: 600; }"));
    return lbl;
}

QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("QFrame{ background: #2a2a2a; border: none; }"));
    return sep;
}

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicImportLayersPopup::CosmicImportLayersPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(400, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

void CosmicImportLayersPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ImportLayersRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ImportLayersRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Import Layers"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── source file card ──────────────────────────────────────────────────────
    QVBoxLayout* srcLay = nullptr;
    auto* srcCard = makeCard(root, srcLay);
    lay->addWidget(srcCard);
    srcLay->addWidget(makeSectionTitle(tr("Source File"), srcCard));
    srcLay->addWidget(makeLabel(tr("Open a Nurture2D project to import layers from."),
                                srcCard, 10, true));
    srcLay->addWidget(makeSeparator(srcCard));

    auto* fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    mFileLabel = makeLabel(tr("No file selected"), srcCard, 11, true);
    mFileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fileRow->addWidget(mFileLabel, 1);

    auto* browseBtn = new CosmicButton(tr("Open File"), srcCard);
    browseBtn->setBordered(true);
    browseBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    fileRow->addWidget(browseBtn);
    srcLay->addLayout(fileRow);

    // ── layer selection card ──────────────────────────────────────────────────
    QVBoxLayout* selLay = nullptr;
    auto* selCard = makeCard(root, selLay);
    lay->addWidget(selCard);
    selLay->addWidget(makeSectionTitle(tr("Select Layers to Import"), selCard));
    selLay->addWidget(makeLabel(tr("Hold Shift or Ctrl to select multiple layers."),
                                selCard, 10, true));
    selLay->addWidget(makeSeparator(selCard));

    mLayerList = new QListWidget(selCard);
    mLayerList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mLayerList->setFixedHeight(160);
    mLayerList->setStyleSheet(QStringLiteral(
        "QListWidget{ background: #252525; color: #b3b3b3; border: 1px solid #3a3a3a;"
        " border-radius: 4px; font-size: 11px; outline: none; }"
        "QListWidget::item{ padding: 5px 8px; }"
        "QListWidget::item:selected{ background: #40555c; color: #ffffff; }"
        "QListWidget::item:hover{ background: #2e2e2e; }"));
    selLay->addWidget(mLayerList);

    mEmptyHint = makeLabel(tr("Open a file above to see its layers here."),
                           selCard, 10, true);
    mEmptyHint->setAlignment(Qt::AlignCenter);
    selLay->addWidget(mEmptyHint);

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    mCancelBtn = new CosmicButton(tr("Cancel"), root);
    mCancelBtn->setBordered(true);
    mCancelBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCancelBtn);

    mImportBtn = new CosmicButton(tr("Import Selected"), root);
    mImportBtn->setBordered(true);
    const QColor accent = CosmicColor::accent();
    mImportBtn->setBackgroundColors(accent, accent.lighter(115), accent.darker(115));
    mImportBtn->setTextColor(QColor(0xff, 0xff, 0xff));
    mImportBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    mImportBtn->setEnabled(false);
    btnRow->addWidget(mImportBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    // ── connections ───────────────────────────────────────────────────────────
    connect(mHeader,    &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(mCancelBtn, &QPushButton::clicked,               this, &QDialog::reject);
    connect(mImportBtn, &QPushButton::clicked,               this, &CosmicImportLayersPopup::onImport);
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicImportLayersPopup::onBrowseFile);
    connect(mLayerList, &QListWidget::itemSelectionChanged,  this, &CosmicImportLayersPopup::onSelectionChanged);
}

void CosmicImportLayersPopup::setCore(Editor* editor)
{
    mEditor = editor;
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicImportLayersPopup::onBrowseFile()
{
    mFileName.clear();
    if (mLayerList) mLayerList->clear();
    mFileName = FileDialog::getOpenFileName(this, FileType::ANIMATION, tr("Choose file"));
    if (mFileName.isEmpty()) return;

    if (mFileLabel) {
        QFileInfo fi(mFileName);
        mFileLabel->setText(fi.fileName());
        mFileLabel->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 11px; }"));
    }
    loadLayersFromFile();
}

void CosmicImportLayersPopup::onSelectionChanged()
{
    mItemsSelected.clear();
    if (!mLayerList) return;
    for (int i = 0; i < mLayerList->count(); i++)
        if (mLayerList->item(i)->isSelected())
            mItemsSelected.append(i);
    if (mImportBtn) mImportBtn->setEnabled(!mItemsSelected.isEmpty());
}

void CosmicImportLayersPopup::onImport()
{
    if (!mEditor || !mImportObject || !mLayerList) return;

    Object* object = mEditor->object();
    const int currentFrame = mEditor->currentFrame();
    QMap<int, int> importedColors;

    for (const QListWidgetItem* item : mLayerList->selectedItems()) {
        Layer* layer = mImportObject->takeLayer(item->data(Qt::UserRole).toInt());
        layer->setName(mEditor->layers()->nameSuggestLayer(item->text()));
        loadKeyFrames(layer);
        object->addLayer(layer);

        if (layer->type() == Layer::VECTOR) {
            LayerVector* lv = static_cast<LayerVector*>(layer);
            for (int i = 0; i < mImportObject->getColorCount(); i++) {
                if (!lv->usesColor(i)) continue;
                if (!importedColors.contains(i)) {
                    object->addColor(mImportObject->getColor(i));
                    importedColors[i] = object->getColorCount() - 1;
                }
                lv->moveColor(i, importedColors[i]);
            }
        }

        if (layer->type() == Layer::SOUND) {
            LayerSound* ls = static_cast<LayerSound*>(layer);
            ls->foreachKeyFrame([this](KeyFrame* key) {
                SoundClip* clip = dynamic_cast<SoundClip*>(key);
                mEditor->sound()->loadSound(clip, clip->fileName());
            });
        }
    }

    mEditor->object()->modification();
    mImportObject.reset();
    mEditor->scrubTo(currentFrame);
    mEditor->updateFrame();
    accept();
}

void CosmicImportLayersPopup::loadLayersFromFile()
{
    if (!mLayerList) return;

    CosmicProgressPopup progress(this);
    progress.setTitle(tr("Opening document…"));
    progress.setRange(0, 100);
    progress.showCentered();

    Status st = Status::OK;
    const N2DProjectIO::ProgressCallbacks cb{
        [&progress](int p) {
            progress.setValue(p);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        },
        [&progress](int max) { progress.setRange(0, max + 3); }
    };

    mImportObject = N2DProjectIO::loadSingleShotProjectAsObject(mFileName, cb, &st);
    mLayerList->clear();

    if (!st.ok() || !mImportObject) {
        if (mEmptyHint) mEmptyHint->setText(tr("Failed to open file."));
        return;
    }

    bool hasLayers = false;
    for (int i = 0; i < mImportObject->getLayerCount(); i++) {
        Layer* layer = mImportObject->getLayer(i);
        if (!layer || layer->type() == Layer::CAMERA) continue;
        auto* item = new QListWidgetItem(layer->name());
        item->setData(Qt::UserRole, layer->id());
        mLayerList->addItem(item);
        hasLayers = true;
    }

    if (mEmptyHint)
        mEmptyHint->setVisible(!hasLayers);
    if (mLayerList)
        mLayerList->setVisible(hasLayers);
}

void CosmicImportLayersPopup::loadKeyFrames(Layer* importedLayer)
{
    if (!importedLayer) return;
    if (importedLayer->type() == Layer::BITMAP) {
        LayerBitmap* bl = static_cast<LayerBitmap*>(importedLayer);
        CellBank* bank = bl->cellBank();
        if (bank) {
            for (int id : bank->getAllCellIds()) {
                if (BitmapImage* img = bank->getCell(id)) {
                    img->loadFile();
                    img->setFileName(QString());
                    img->setModified(true);
                }
                if (QUndoStack* stack = bank->getOrCreateUndoStack(id))
                    stack->clear();
            }
        }
    }
    importedLayer->foreachKeyFrame([](KeyFrame* k) { k->loadFile(); });
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicImportLayersPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(400);
    setMaximumWidth(400);
    adjustSize();

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width() * 0.93)), qMax(1, int(s.height() * 0.93)));
    const QRect  g1(QPoint(c.x() - s0.width()/2, c.y() - s0.height()/2), s0);
    setGeometry(g1);

    auto* op = new QPropertyAnimation(this, "windowOpacity", this);
    op->setDuration(80); op->setStartValue(0.0); op->setEndValue(1.0);
    auto* gm = new QPropertyAnimation(this, "geometry", this);
    gm->setDuration(95); gm->setStartValue(g1); gm->setEndValue(g0);
    auto* grp = new QParallelAnimationGroup(this);
    grp->addAnimation(op); grp->addAnimation(gm);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}
