#pragma once
#include <QObject>
#include <QNetworkAccessManager>

class QWidget;
class QProgressDialog;
class QNetworkReply;

class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);
    void checkForUpdates(QWidget *parent = nullptr);

private:
    void startDownload();
    void applyUpdate(const QString &zipPath);

    QNetworkAccessManager m_nam;
    QNetworkReply *m_checkReply    = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QProgressDialog *m_progress    = nullptr;
    QWidget *m_parent              = nullptr;
    QString m_latestVersion;
    QString m_downloadUrl;

private slots:
    void onCheckFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
};
