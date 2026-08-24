#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QUrl>

/**
 * The documents in the library folder, pinned ones first.
 *
 * FolderListModel could enumerate the folder but cannot order it by anything
 * the app knows, and pinning is exactly that. Owning the list also gives the
 * sidebar somewhere to put deletion, and lets a rename or an outside change
 * refresh the view without the writer reopening anything.
 */
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QUrl folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        FileUrlRole,
        PinnedRole,
    };

    explicit LibraryModel(QObject *parent = nullptr);

    QUrl folder() const;
    void setFolder(const QUrl &folder);
    int count() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool isPinned(const QUrl &url) const;
    Q_INVOKABLE void togglePinned(const QUrl &url);
    /** Move a document to the trash. Never unlinks: a mis-click is recoverable. */
    Q_INVOKABLE bool moveToTrash(const QUrl &url);
    Q_INVOKABLE void refresh();

    static QString displayNameFor(const QString &fileName);

signals:
    void folderChanged();
    void countChanged();
    void trashFailed(const QString &fileName);

private:
    struct Entry {
        QString fileName;
        QString filePath;
        bool pinned = false;

        bool operator==(const Entry &other) const {
            return fileName == other.fileName && filePath == other.filePath
                && pinned == other.pinned;
        }
    };

    QStringList pinnedPaths() const;
    void setPinnedPaths(const QStringList &paths);
    void watchFolder();

    QUrl m_folder;
    QList<Entry> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshDebounce;
};
