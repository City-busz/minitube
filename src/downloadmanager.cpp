/* $BEGIN_LICENSE

This file is part of Minitube.
Copyright 2009, Flavio Tordini <flavio.tordini@gmail.com>

Minitube is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Minitube is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Minitube.  If not, see <http://www.gnu.org/licenses/>.

$END_LICENSE */

#include "downloadmanager.h"
#include "downloaditem.h"
#include "downloadmodel.h"
#include "video.h"
#include "constants.h"
#include "mainwindow.h"
#ifdef APP_ACTIVATION
#include "activation.h"
#endif
#ifdef APP_EXTRA
#include "extra.h"
#endif
#include "datautils.h"
#include "iconutils.h"

static DownloadManager *downloadManagerInstance = 0;

DownloadManager::DownloadManager(QWidget *parent) :
    QObject(parent),
    downloadModel(new DownloadModel(this, this))
{ }

DownloadManager* DownloadManager::instance() {
    if (!downloadManagerInstance) downloadManagerInstance = new DownloadManager();
    return downloadManagerInstance;
}

void DownloadManager::clear() {
    qDeleteAll(items);
    items.clear();
    updateStatusMessage();
}

int DownloadManager::activeItems() {
    int num = 0;
    for (DownloadItem *item : items) {
        if (item->status() == Downloading || item->status() == Starting) num++;
    }
    return num;
}

DownloadItem* DownloadManager::itemForVideo(Video* video) {
    for (DownloadItem *item : items) {
        if (item->getVideo()->getId() == video->getId()) return item;
    }
    return 0;
}

void DownloadManager::addItem(Video *video) {
    // qDebug() << __FUNCTION__ << video->title();

    DownloadItem *item = itemForVideo(video);
    if (item != 0) {
        if (item->status() == Failed || item->status() == Idle) {
            qDebug() << "Restarting download" << video->getTitle();
            item->tryAgain();
        } else {
            qDebug() << "Already downloading video" << video->getTitle();
        }
        return;
    }

    connect(video, SIGNAL(gotStreamUrl(QUrl)), SLOT(gotStreamUrl(QUrl)));
    // TODO handle signal errors
    // connect(video, SIGNAL(errorStreamUrl(QString)), SLOT(handleError(QString)));
    video->loadStreamUrl();

    // see you in gotStreamUrl()
}

void DownloadManager::gotStreamUrl(QUrl url) {

    Video *video = static_cast<Video*>(sender());
    if (!video) {
        qDebug() << "Cannot get video in" << __FUNCTION__;
        return;
    }

    video->disconnect(this);

    QString basename = DataUtils::stringToFilename(video->getTitle());
    if (basename.isEmpty()) basename = video->getId();

    QString filename = currentDownloadFolder() + "/" + basename + ".mp4";

    Video *videoCopy = video->clone();
    DownloadItem *item = new DownloadItem(videoCopy, url, filename, this);

    downloadModel->beginInsertRows(QModelIndex(), 0, 0);
    items.prepend(item);
    downloadModel->endInsertRows();

    // connect(item, SIGNAL(statusChanged()), SLOT(updateStatusMessage()));
    connect(item, SIGNAL(finished()), SLOT(itemFinished()));
    item->start();

    updateStatusMessage();
}

void DownloadManager::itemFinished() {
    if (activeItems() == 0) emit finished();
#ifdef APP_EXTRA
    DownloadItem *item = static_cast<DownloadItem*>(sender());
    if (!item) {
        qDebug() << "Cannot get item in" << __FUNCTION__;
        return;
    }
    Video *video = item->getVideo();
    if (!video) return;
    QString stats = tr("%1 downloaded in %2").arg(
                DownloadItem::formattedFilesize(item->bytesTotal()),
                DownloadItem::formattedTime(item->totalTime(), false));
    Extra::notify(tr("Download finished"), video->getTitle(), stats);
#endif
}

void DownloadManager::updateStatusMessage() {
    QString message = tr("%n Download(s)", "", activeItems());
    emit statusMessageChanged(message);
}

QString DownloadManager::defaultDownloadFolder() {
    // download in the Movies system folder
    QString path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    const QDir moviesDir(path);
    if (!moviesDir.exists()) {
        // fallback to Desktop
        path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

        const QDir desktopDir(path);
        if (!desktopDir.exists()) {
            // fallback to Home
            path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
    }
    return path;
}

QString DownloadManager::currentDownloadFolder() {
    QSettings settings;
    QString path = settings.value("downloadFolder").toString();
    if (path.isEmpty()) path = defaultDownloadFolder();
    return path;
}
