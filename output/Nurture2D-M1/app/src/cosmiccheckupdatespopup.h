#ifndef COSMICCHECKUPDATESPOPUP_H
#define COSMICCHECKUPDATESPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicButton;
class QLabel;
class QProgressBar;
class QNetworkAccessManager;
class QNetworkReply;

// Redesigned Check for Updates popup.
class CosmicCheckUpdatesPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicCheckUpdatesPopup(QWidget* parent = nullptr);
    ~CosmicCheckUpdatesPopup() override;

    void startChecking();

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void networkRequestFinished(QNetworkReply* reply);

private:
    void buildUI();
    void regularBuildCheck();
    void nightlyBuildCheck();
    void showError(const QString& detail);
    void showResult(bool isNewer, const QString& latestVersion);
    bool compareVersion(const QString& current, const QString& latest);
    QString getVersionFromXml(const QString& xml);
    void gotoDownloadPage();

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mStatusLabel;
    QPointer<QLabel>             mDetailLabel;
    QPointer<QProgressBar>       mProgress;
    QPointer<CosmicButton>       mDownloadBtn;
    QPointer<CosmicButton>       mCloseBtn;

    QNetworkAccessManager* mNetworkManager = nullptr;
};

#endif // COSMICCHECKUPDATESPOPUP_H
