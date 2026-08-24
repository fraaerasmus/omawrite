#include "librarymodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {
const QString pinnedSetting = QStringLiteral("library/pinned");
const QStringList documentFilters{QStringLiteral("*.md"), QStringLiteral("*.markdown"),
                                  QStringLiteral("*.txt")};
}

LibraryModel::LibraryModel(QObject *parent) : QAbstractListModel(parent) {
    // A save rewrites the file through QSaveFile, which the watcher sees as a
    // burst of directory changes. Coalesce them so a keystroke does not rebuild
    // the list three times.
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(120);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &LibraryModel::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_refreshDebounce.start(); });
}

QUrl LibraryModel::folder() const {
    return m_folder;
}

void LibraryModel::setFolder(const QUrl &folder) {
    if (folder == m_folder)
        return;
    m_folder = folder;
    emit folderChanged();
    watchFolder();
    refresh();
}

int LibraryModel::count() const {
    return static_cast<int>(m_entries.size());
}

int LibraryModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : count();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case FileNameRole:
        return entry.fileName;
    case DisplayNameRole:
        return displayNameFor(entry.fileName);
    case FileUrlRole:
        return QUrl::fromLocalFile(entry.filePath);
    case PinnedRole:
        return entry.pinned;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    return {
        {FileNameRole, "fileName"},
        {DisplayNameRole, "displayName"},
        {FileUrlRole, "fileUrl"},
        {PinnedRole, "pinned"},
    };
}

/** The extension is noise when every row in the list has one. */
QString LibraryModel::displayNameFor(const QString &fileName) {
    const QFileInfo info(fileName);
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")
        || suffix == QStringLiteral("txt"))
        return info.completeBaseName();
    return fileName;
}

QStringList LibraryModel::pinnedPaths() const {
    return QSettings().value(pinnedSetting).toStringList();
}

void LibraryModel::setPinnedPaths(const QStringList &paths) {
    QSettings().setValue(pinnedSetting, paths);
}

bool LibraryModel::isPinned(const QUrl &url) const {
    return url.isLocalFile() && pinnedPaths().contains(url.toLocalFile());
}

void LibraryModel::togglePinned(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    const QString path = url.toLocalFile();
    QStringList pinned = pinnedPaths();
    if (pinned.contains(path))
        pinned.removeAll(path);
    else
        pinned.append(path);
    setPinnedPaths(pinned);
    refresh();
}

bool LibraryModel::moveToTrash(const QUrl &url) {
    if (!url.isLocalFile())
        return false;

    const QString path = url.toLocalFile();
    QFile file(path);
    if (!file.moveToTrash()) {
        // Deliberately no unlink fallback. Deleting someone's writing outright
        // because the desktop has no trash is worse than refusing.
        emit trashFailed(QFileInfo(path).fileName());
        return false;
    }

    QStringList pinned = pinnedPaths();
    if (pinned.removeAll(path) > 0)
        setPinnedPaths(pinned);
    refresh();
    return true;
}

void LibraryModel::watchFolder() {
    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    if (m_folder.isLocalFile())
        m_watcher.addPath(m_folder.toLocalFile());
}

void LibraryModel::refresh() {
    QList<Entry> pinnedEntries;
    QList<Entry> rest;

    if (m_folder.isLocalFile()) {
        const QStringList pinned = pinnedPaths();
        const QDir dir(m_folder.toLocalFile());
        const QFileInfoList found =
            dir.entryInfoList(documentFilters, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &info : found) {
            Entry entry{info.fileName(), info.absoluteFilePath(),
                        pinned.contains(info.absoluteFilePath())};
            (entry.pinned ? pinnedEntries : rest).append(entry);
        }
    }

    // Pinned first, each group already alphabetical from entryInfoList.
    QList<Entry> updated = pinnedEntries;
    updated.append(rest);
    if (updated == m_entries)
        return;

    beginResetModel();
    m_entries = updated;
    endResetModel();
    emit countChanged();
}
