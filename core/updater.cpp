#include "updater.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTextStream>

// ── Configure these two constants to match your GitHub repo ──────────────────
static const char *GITHUB_OWNER = "athul-ka";
static const char *GITHUB_REPO  = "Ledger-Trading-Journal";
// ─────────────────────────────────────────────────────────────────────────────

#ifndef APP_VERSION
#  define APP_VERSION "dev"
#endif

Updater::Updater(QObject *parent) : QObject(parent) {}

void Updater::checkForUpdates(QWidget *parent)
{
    m_parent = parent;

    QNetworkRequest req(QUrl(
        QString("https://api.github.com/repos/%1/%2/releases/latest")
            .arg(GITHUB_OWNER, GITHUB_REPO)));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Ledger-App");

    m_checkReply = m_nam.get(req);
    connect(m_checkReply, &QNetworkReply::finished, this, &Updater::onCheckFinished);
}

void Updater::onCheckFinished()
{
    m_checkReply->deleteLater();

    if (m_checkReply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(m_parent, "Update Check Failed",
            "Could not reach GitHub:\n" + m_checkReply->errorString());
        return;
    }

    QJsonDocument doc  = QJsonDocument::fromJson(m_checkReply->readAll());
    QJsonObject   root = doc.object();
    m_latestVersion    = root["tag_name"].toString(); // e.g. "v1.2.0"

    QString current = QString("v") + APP_VERSION;

    if (m_latestVersion.isEmpty() || m_latestVersion == current) {
        QMessageBox::information(m_parent, "Up to Date",
            QString("You are running the latest version (%1).").arg(current));
        return;
    }

    // Find the first .zip asset in the release
    for (const QJsonValue &v : root["assets"].toArray()) {
        QJsonObject asset = v.toObject();
        if (asset["name"].toString().endsWith(".zip")) {
            m_downloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }

    if (m_downloadUrl.isEmpty()) {
        QMessageBox::warning(m_parent, "Update Available",
            QString("Version %1 is available but no download asset was found.\n"
                    "Visit the GitHub releases page to download manually.")
                .arg(m_latestVersion));
        return;
    }

    int ret = QMessageBox::question(m_parent, "Update Available",
        QString("Version %1 is available (you have %2).\n\nDownload and install now?")
            .arg(m_latestVersion, current),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
        startDownload();
}

void Updater::startDownload()
{
    m_progress = new QProgressDialog("Downloading update…", "Cancel", 0, 100, m_parent);
    m_progress->setWindowModality(Qt::WindowModal);
    m_progress->setMinimumDuration(0);
    m_progress->setValue(0);
    m_progress->show();

    QNetworkRequest req{QUrl(m_downloadUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_downloadReply = m_nam.get(req);

    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &Updater::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &Updater::onDownloadFinished);
    connect(m_progress, &QProgressDialog::canceled,
            m_downloadReply, &QNetworkReply::abort);
}

void Updater::onDownloadProgress(qint64 received, qint64 total)
{
    if (m_progress && total > 0)
        m_progress->setValue(static_cast<int>(received * 100 / total));
}

void Updater::onDownloadFinished()
{
    m_downloadReply->deleteLater();

    if (m_progress) {
        m_progress->close();
        m_progress->deleteLater();
        m_progress = nullptr;
    }

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        if (m_downloadReply->error() != QNetworkReply::OperationCanceledError)
            QMessageBox::warning(m_parent, "Download Failed",
                m_downloadReply->errorString());
        return;
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString zipPath = tempDir + "/LedgerUpdate.zip";
    QFile   zipFile(zipPath);

    if (!zipFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(m_parent, "Update Error",
            "Could not write the downloaded file to the temp directory.");
        return;
    }
    zipFile.write(m_downloadReply->readAll());
    zipFile.close();

    applyUpdate(zipPath);
}

void Updater::applyUpdate(const QString &zipPath)
{
    QString appDir     = QCoreApplication::applicationDirPath();
    QString extractDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + "/LedgerUpdateExtracted";
    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + "/ledger_update.ps1";

    // PowerShell script: waits for app to exit, extracts zip, copies all files
    // over the current install folder, then restarts Ledger.exe.
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(m_parent, "Update Error",
            "Could not write the updater script.");
        return;
    }
    QTextStream out(&script);
    out << "Start-Sleep -Seconds 2\n";
    out << QString("if (Test-Path '%1') { Remove-Item '%1' -Recurse -Force }\n")
           .arg(extractDir);
    out << QString("Expand-Archive -Force -Path '%1' -DestinationPath '%2'\n")
           .arg(zipPath, extractDir);
    out << QString("Copy-Item -Path '%1\\*' -Destination '%2' -Recurse -Force\n")
           .arg(extractDir, appDir);
    out << QString("Start-Process '%1\\Ledger.exe'\n").arg(appDir);
    script.close();

    QMessageBox::information(m_parent, "Restarting to Apply Update",
        "The update will be applied in a moment.\nThe app will restart automatically.");

    QProcess::startDetached("powershell",
        {"-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", scriptPath});

    QCoreApplication::quit();
}
