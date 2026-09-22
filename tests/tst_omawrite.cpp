#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "librarymodel.h"
#include "markdownhighlighter.h"

class OmawriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    void autosavesAnOpenFileWithoutAnExplicitSave() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("draft.md"));

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(!backend.modified());

        editor->setProperty("text", QStringLiteral("written, never saved by hand"));
        QVERIFY(backend.modified());

        // The debounce has to actually debounce: nothing on disk immediately.
        QFile early(path);
        QVERIFY(early.open(QIODevice::ReadOnly));
        QVERIFY(!early.readAll().contains("never saved by hand"));
        early.close();

        QTRY_VERIFY_WITH_TIMEOUT(!backend.modified(), 5000);
        QFile written(path);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(written.readAll()).trimmed(),
                 QStringLiteral("written, never saved by hand"));
    }

    void autosaveNeverRaisesTheSaveAsDialog() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        // An untitled buffer has nowhere to go. save() would answer that with
        // the Save As dialog, which an idle timer must never trigger.
        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        editor->setProperty("text", QStringLiteral("untitled and unsaved"));
        QVERIFY(backend.modified());

        QTest::qWait(2500);
        QCOMPARE(saveDialogSpy.count(), 0);
        QVERIFY(backend.modified());
    }

    void createsNumberedDocumentsInTheLibrary() {
        QTemporaryDir library;
        QVERIFY(library.isValid());

        Backend backend;
        backend.chooseLibraryRoot(QUrl::fromLocalFile(library.path()));
        QCOMPARE(backend.libraryRoot().toLocalFile(), library.path());

        backend.newDocumentInLibrary();
        const QString first = QDir(library.path()).filePath(QStringLiteral("Untitled.md"));
        QVERIFY(QFileInfo::exists(first));
        QCOMPARE(backend.fileUrl().toLocalFile(), first);

        // A second new document must not silently reopen the first.
        backend.newDocumentInLibrary();
        const QString second = QDir(library.path()).filePath(QStringLiteral("Untitled 2.md"));
        QVERIFY(QFileInfo::exists(second));
        QCOMPARE(backend.fileUrl().toLocalFile(), second);
    }

    void newLibraryDocumentsAutosave() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        QTemporaryDir library;
        QVERIFY(library.isValid());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        // The point of creating the file up front: a new document has a URL, so
        // autosave covers it with no dialog and no Ctrl+S.
        backend.chooseLibraryRoot(QUrl::fromLocalFile(library.path()));
        backend.newDocumentInLibrary();

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        editor->setProperty("text", QStringLiteral("straight into the library"));
        QTRY_VERIFY_WITH_TIMEOUT(!backend.modified(), 5000);
        QCOMPARE(saveDialogSpy.count(), 0);

        QFile written(QDir(library.path()).filePath(QStringLiteral("Untitled.md")));
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(written.readAll()).trimmed(),
                 QStringLiteral("straight into the library"));
    }

    void adjacentDocumentStepsThroughTheLibrary() {
        QTemporaryDir library;
        QVERIFY(library.isValid());
        Backend backend;
        backend.chooseLibraryRoot(QUrl::fromLocalFile(library.path()));

        // Nothing open yet: stepping enters the list from either end.
        QVERIFY(backend.adjacentDocument(1).isEmpty());
        for (int i = 0; i < 3; ++i)
            backend.newDocumentInLibrary();
        QCOMPARE(backend.library()->count(), 3);
        const QUrl first = backend.library()->urlAt(0);
        const QUrl last = backend.library()->urlAt(2);

        backend.open(first);
        QCOMPARE(backend.adjacentDocument(-1), first);
        QCOMPARE(backend.adjacentDocument(1), backend.library()->urlAt(1));
        backend.open(last);
        QCOMPARE(backend.adjacentDocument(1), last);
    }

    void trashCurrentDocumentMovesToANeighbour() {
        QTemporaryDir library;
        QVERIFY(library.isValid());
        Backend backend;
        backend.chooseLibraryRoot(QUrl::fromLocalFile(library.path()));
        backend.newDocumentInLibrary();
        backend.newDocumentInLibrary();
        const QUrl doomed = backend.fileUrl();

        backend.trashCurrentDocument();
        if (QFileInfo::exists(doomed.toLocalFile()))
            QSKIP("No usable trash on this system");
        QVERIFY(backend.fileUrl() != doomed);
        QCOMPARE(backend.library()->count(), 1);
        QCOMPARE(backend.library()->indexOf(backend.fileUrl()), 0);
    }

    void showsTheLibrarySidebarByDefault() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *sidebar = window->findChild<QObject *>(QStringLiteral("librarySidebar"));
        QVERIFY(sidebar);
        QVERIFY(sidebar->property("visible").toBool());
        QVERIFY(window->findChild<QObject *>(QStringLiteral("libraryList")));

        backend.setSidebarVisible(false);
        QVERIFY(!sidebar->property("visible").toBool());
        QVERIFY(backend.sidebarVisible() == false);
    }

    void remembersEditorZoomAcrossSessions() {
        Backend backend;
        backend.resetEditorZoom();
        QCOMPARE(backend.editorZoom(), 1.0);

        backend.adjustEditorZoom(2);
        QVERIFY(qFuzzyCompare(backend.editorZoom(), 1.2));

        // A second Backend is what a relaunch looks like: the value comes back
        // from QSettings, not from the object that set it.
        Backend relaunched;
        QVERIFY(qFuzzyCompare(relaunched.editorZoom(), 1.2));

        backend.setEditorZoom(99.0);
        QCOMPARE(backend.editorZoom(), 2.5);
        backend.setEditorZoom(0.01);
        QCOMPARE(backend.editorZoom(), 0.6);

        backend.resetEditorZoom();
        QCOMPARE(backend.editorZoom(), 1.0);
    }

    void zoomScalesTheEditorButNotTheChrome() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        backend.resetEditorZoom();
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        backend.setEditorZoom(1.5);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 30);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 30);

        // Chrome follows the desktop text scale alone, so it is unmoved.
        QObject *sidebar = window->findChild<QObject *>(QStringLiteral("librarySidebar"));
        QVERIFY(sidebar);
        QCOMPARE(sidebar->property("width").toInt(), 240);

        backend.resetEditorZoom();
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);
    }

    void listsPinnedDocumentsFirst() {
        QTemporaryDir library;
        QVERIFY(library.isValid());
        const QDir dir(library.path());
        for (const QString &name : {QStringLiteral("Alpha.md"), QStringLiteral("Beta.md"),
                                    QStringLiteral("Gamma.md")}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
        }

        LibraryModel model;
        model.setFolder(QUrl::fromLocalFile(library.path()));
        QCOMPARE(model.count(), 3);
        QCOMPARE(model.data(model.index(0), LibraryModel::DisplayNameRole).toString(),
                 QStringLiteral("Alpha"));

        const QUrl gamma = QUrl::fromLocalFile(dir.filePath(QStringLiteral("Gamma.md")));
        model.togglePinned(gamma);
        QVERIFY(model.isPinned(gamma));
        QCOMPARE(model.data(model.index(0), LibraryModel::DisplayNameRole).toString(),
                 QStringLiteral("Gamma"));
        QVERIFY(model.data(model.index(0), LibraryModel::PinnedRole).toBool());
        // The unpinned remainder keeps its own alphabetical order.
        QCOMPARE(model.data(model.index(1), LibraryModel::DisplayNameRole).toString(),
                 QStringLiteral("Alpha"));

        model.togglePinned(gamma);
        QVERIFY(!model.isPinned(gamma));
        QCOMPARE(model.data(model.index(0), LibraryModel::DisplayNameRole).toString(),
                 QStringLiteral("Alpha"));
    }

    void trashingADocumentDropsItsPin() {
        QTemporaryDir library;
        QVERIFY(library.isValid());
        const QString path = QDir(library.path()).filePath(QStringLiteral("Doomed.md"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("gone soon");
        file.close();

        LibraryModel model;
        model.setFolder(QUrl::fromLocalFile(library.path()));
        const QUrl url = QUrl::fromLocalFile(path);
        model.togglePinned(url);
        QCOMPARE(model.count(), 1);
        QVERIFY(model.isPinned(url));

        if (!model.moveToTrash(url))
            QSKIP("No usable trash on this system");

        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(model.count(), 0);
        // A stale pin would resurrect the row if the name were ever reused.
        QVERIFY(!model.isPinned(url));
    }

    void bindsTheLibraryModelAndCollapseControl() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());
        QTemporaryDir library;
        QVERIFY(library.isValid());
        QFile seed(QDir(library.path()).filePath(QStringLiteral("Only.md")));
        QVERIFY(seed.open(QIODevice::WriteOnly));
        seed.close();

        Backend backend;
        backend.chooseLibraryRoot(QUrl::fromLocalFile(library.path()));
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QCOMPARE(backend.library()->count(), 1);
        QVERIFY(window->findChild<QObject *>(QStringLiteral("collapseButton")));

        // The row delegate and its context menu are not reachable from here:
        // offscreen, the ListView never instantiates a delegate, so the menu has
        // no object to find. Pin and trash are covered against the model above;
        // this only asserts the sidebar is wired to it.
        QObject *sidebar = window->findChild<QObject *>(QStringLiteral("librarySidebar"));
        QVERIFY(sidebar);
        QCOMPARE(qvariant_cast<QObject *>(sidebar->property("model")), backend.library());

        const QUrl only = QUrl::fromLocalFile(QDir(library.path()).filePath(QStringLiteral("Only.md")));
        backend.library()->togglePinned(only);
        QVERIFY(backend.library()->isPinned(only));
        backend.library()->togglePinned(only);
        QVERIFY(!backend.library()->isPinned(only));
    }

    void collapseButtonHidesTheSidebar() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        backend.setSidebarVisible(true);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *sidebar = window->findChild<QObject *>(QStringLiteral("librarySidebar"));
        QVERIFY(sidebar);
        QVERIFY(sidebar->property("visible").toBool());

        QVERIFY(QMetaObject::invokeMethod(sidebar, "collapseRequested"));
        QVERIFY(!sidebar->property("visible").toBool());
        // It has to stick, or the button is just a flicker.
        QVERIFY(!backend.sidebarVisible());

        backend.setSidebarVisible(true);
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmawriteTest)
#include "tst_omawrite.moc"
