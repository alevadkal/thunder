#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QFileDialog>
#include <QVariant>
#include <QWidgetAction>
#include <QApplication>
#include <QMessageBox>

#include <QQmlContext>
#include <QQuickItem>

#include <json.h>
#include <timer.h>
#include <log.h>

#include <cstring>

#include <editor/asseteditor.h>

// Misc
#include "managers/asseteditormanager/importqueue.h"
#include "managers/feedmanager/feedmanager.h"
#include "managers/projectmanager/projectmodel.h"

#include "assetmanager.h"

#include <editor/projectmanager.h>
#include <editor/undomanager.h>
#include <editor/pluginmanager.h>
#include <editor/settingsmanager.h>

#include "documentmodel.h"

// System
#include <global.h>
#include "config.h"

// Editors
#include "editors/componentbrowser/componentmodel.h"

Q_DECLARE_METATYPE(Object *)
Q_DECLARE_METATYPE(Object::ObjectList *)

namespace  {
    const char *gGeometry("main.geometry");
    const char *gWindows("main.windows");
    const char *gWorkspace("main.workspace");
};

MainWindow::MainWindow(Engine *engine, QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow),
        m_currentWorkspace(":/Workspaces/Default.ws"),
        m_queue(new ImportQueue),
        m_projectModel(new ProjectModel),
        m_feedManager(new FeedManager),
        m_documentModel(nullptr),
        m_undo(nullptr),
        m_redo(nullptr),
        m_mainEditor(nullptr),
        m_currentEditor(nullptr),
        m_builder(new QProcess(this)),
        m_forceReimport(false) {

    qRegisterMetaType<Vector2>  ("Vector2");
    qRegisterMetaType<Vector3>  ("Vector3");

    qRegisterMetaType<uint8_t>  ("uint8_t");
    qRegisterMetaType<uint32_t> ("uint32_t");

    qDebug() << "float" << qMetaTypeId<QList<float>>() << qMetaTypeId<float>();
    qDebug() << "int" << qMetaTypeId<QList<int>>() << qMetaTypeId<int>();

    qmlRegisterType<ProjectModel>("com.frostspear.thunderengine", 1, 0, "ProjectModel");

    SettingsManager::instance()->registerProperty("General/Language", QLocale());

    ui->setupUi(this);

    ui->playButton->setProperty("checkblue", true);
    ui->pauseButton->setProperty("checkblue", true);

    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::on_actionPlay_triggered);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::on_actionPause_triggered);

    ui->viewportWidget->setWindowTitle(tr("Viewport"));
    ui->propertyWidget->setWindowTitle(tr("Properties"));
    ui->projectWidget->setWindowTitle(tr("Project Settings"));
    ui->preferencesWidget->setWindowTitle(tr("Editor Preferences"));
    ui->timeline->setWindowTitle(tr("Timeline"));
    ui->classMapView->setWindowTitle(tr("Class View"));
    ui->preview->setWindowTitle(tr("Preview"));

    ui->preview->setEngine(engine);

    m_mainEditor = ui->viewportWidget;

    m_undo = UndoManager::instance()->createUndoAction(ui->menuEdit);
    m_undo->setShortcut(QKeySequence("Ctrl+Z"));
    ui->menuEdit->insertAction(ui->actionPlay, m_undo);

    m_redo = UndoManager::instance()->createRedoAction(ui->menuEdit);
    m_redo->setShortcut(QKeySequence("Ctrl+Y"));
    ui->menuEdit->insertAction(ui->actionPlay, m_redo);

    ui->menuEdit->insertSeparator(ui->actionPlay);

    ui->componentButton->setProperty("blue", true);
    ui->commitButton->setProperty("green", true);

    connect(ui->actionBuild_All, &QAction::triggered, this, &MainWindow::onBuildProject);

    ui->quickWidget->rootContext()->setContextProperty("projectsModel", m_projectModel);
    ui->quickWidget->rootContext()->setContextProperty("feedManager", m_feedManager);
    ui->quickWidget->setSource(QUrl("qrc:/QML/qml/Startup.qml"));
    QQuickItem *item = ui->quickWidget->rootObject();
    connect(item, SIGNAL(openProject(QString)), this, SLOT(onOpenProject(QString)));
    connect(item, SIGNAL(newProject()), this, SLOT(onNewProject()));
    connect(item, SIGNAL(importProject()), this, SLOT(onImportProject()));

    ComponentBrowser *comp = new ComponentBrowser(this);
    QMenu *menu = new QMenu(ui->componentButton);
    QWidgetAction *action = new QWidgetAction(menu);
    action->setDefaultWidget(comp);
    menu->addAction(action);
    ui->componentButton->setMenu(menu);
    connect(comp, SIGNAL(componentSelected(QString)), ui->viewportWidget, SIGNAL(createComponent(QString)));
    connect(comp, SIGNAL(componentSelected(QString)), menu, SLOT(hide()));

    comp->setGroups(QStringList("Components"));
    ui->components->setGroups(QStringList() << "Scene" << "Components");

    connect(ui->projectWidget, &SettingsBrowser::commited, ProjectManager::instance(), &ProjectManager::saveSettings);
    connect(ui->projectWidget, &SettingsBrowser::reverted, ProjectManager::instance(), &ProjectManager::loadSettings);

    connect(ui->preferencesWidget, &SettingsBrowser::commited, SettingsManager::instance(), &SettingsManager::saveSettings);
    connect(ui->preferencesWidget, &SettingsBrowser::reverted, SettingsManager::instance(), &SettingsManager::loadSettings);

    findWorkspaces(":/Workspaces");
    findWorkspaces("workspaces");
    ui->menuWorkspace->insertSeparator(ui->actionReset_Workspace);

    ui->actionAbout->setText(tr("About %1...").arg(EDITOR_NAME));
    connect(ui->actionAbout, &QAction::triggered, &m_aboutDlg, &AboutDialog::exec);
    connect(ui->actionPlugin_Manager, &QAction::triggered, &m_pluginDlg, &PluginDialog::exec);

    connect(ui->toolWidget, &QToolWindowManager::toolWindowVisibilityChanged, this, &MainWindow::onToolWindowVisibilityChanged);
    connect(ui->toolWidget, &QToolWindowManager::currentToolWindowChanged, this, &MainWindow::onCurrentToolWindowChanged);

    connect(ui->viewportWidget, &SceneComposer::hierarchyCreated, ui->hierarchy, &HierarchyBrowser::onSetRootObject, Qt::DirectConnection);
    connect(ui->viewportWidget, &SceneComposer::itemsUpdated, ui->hierarchy, &HierarchyBrowser::onObjectUpdated);
    connect(ui->viewportWidget, &SceneComposer::objectsSelected, ui->hierarchy, &HierarchyBrowser::onObjectSelected);
    connect(ui->viewportWidget, &SceneComposer::renameItem, ui->hierarchy, &HierarchyBrowser::onItemRename);
    connect(ui->viewportWidget, &SceneComposer::objectsSelected, ui->timeline, &Timeline::onObjectsSelected);
    connect(ui->viewportWidget, &SceneComposer::objectsChanged, ui->timeline, &Timeline::onObjectsChanged);

    connect(ui->hierarchy, &HierarchyBrowser::selected, ui->viewportWidget, &SceneComposer::onSelectActors);
    connect(ui->hierarchy, &HierarchyBrowser::updated, ui->viewportWidget, &SceneComposer::onUpdated);
    connect(ui->hierarchy, &HierarchyBrowser::parented, ui->viewportWidget, &SceneComposer::onParentActors);
    connect(ui->hierarchy, &HierarchyBrowser::focused, ui->viewportWidget, &SceneComposer::onFocusActor);
    connect(ui->hierarchy, &HierarchyBrowser::removed, ui->viewportWidget, &SceneComposer::onItemDelete);
    connect(ui->hierarchy, &HierarchyBrowser::menuRequested, ui->viewportWidget, &SceneComposer::onMenuRequested);

    connect(ui->contentBrowser, &ContentBrowser::assetsSelected, this, &MainWindow::onItemsSelected);
    connect(ui->contentBrowser, &ContentBrowser::openEditor, this, &MainWindow::onOpenEditor);

    connect(ui->timeline, &Timeline::animated, ui->propertyView, &PropertyEditor::onAnimated);
    connect(ui->timeline, &Timeline::moved, ui->viewportWidget, &SceneComposer::onUpdated);
    connect(ui->timeline, &Timeline::objectSelected, ui->viewportWidget, &SceneComposer::onSelectActors);

    ui->toolPanel->setVisible(false);
    ui->toolWidget->setVisible(false);

    ui->toolWidget->addToolWindow(ui->viewportWidget,    QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->preview,           QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->contentBrowser,    QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->hierarchy,         QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->propertyWidget,    QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->components,        QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->consoleOutput,     QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->timeline,          QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->projectWidget,     QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->preferencesWidget, QToolWindowManager::NoArea);
    ui->toolWidget->addToolWindow(ui->classMapView,      QToolWindowManager::NoArea);

    foreach(QWidget *it, ui->toolWidget->toolWindows()) {
        QAction *action = new QAction(it->windowTitle(), ui->menuWindow);
        ui->menuWindow->addAction(action);
        action->setObjectName(it->windowTitle());
        action->setData(QVariant::fromValue(it));
        action->setCheckable(true);
        action->setChecked(false);
        connect(action, SIGNAL(triggered(bool)), this, SLOT(onToolWindowActionToggled(bool)));
    }

    AssetManager::ClassMap map = AssetManager::instance()->classMaps();
    if(!map.isEmpty()) {
        ui->classMapView->setModel(map.first());
    }

    connect(AssetManager::instance(), &AssetManager::buildSuccessful, ComponentModel::instance(), &ComponentModel::update);

    onItemsSelected({});

    setGameMode(false);
    resetGeometry();

    connect(m_builder, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(onBuildFinished(int,QProcess::ExitStatus)));

    connect(m_builder, &QProcess::readyReadStandardOutput, this, &MainWindow::readOutput);
    connect(m_builder, &QProcess::readyReadStandardError, this, &MainWindow::readError);

    connect(this, &MainWindow::readBuildLogs, ui->consoleOutput, &ConsoleManager::parseLogs);

    startTimer(16);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onItemsSelected(const QList<QObject *> &items) {
    QObject *object = ui->propertyView->object();
    AssetConverterSettings *settings = dynamic_cast<AssetConverterSettings *>(object);

    if(items.isEmpty()) {
        ui->componentButton->setVisible(false);
        ui->commitButton->setVisible(false);
        ui->revertButton->setVisible(false);
        ui->propertyView->setObject(nullptr);
        return;
    }

    QObject *item = items.front();

    if(settings && settings != item) {
        AssetManager::instance()->checkImportSettings(settings);
        disconnect(settings, &AssetConverterSettings::updated, this, &MainWindow::onSettingsUpdated);
    }

    settings = dynamic_cast<AssetConverterSettings *>(item);
    if(settings) {
        connect(settings, &AssetConverterSettings::updated, this, &MainWindow::onSettingsUpdated);

        ui->commitButton->setEnabled(settings->isModified());
        ui->revertButton->setEnabled(settings->isModified());
    }
    ui->commitButton->setVisible(settings);
    ui->revertButton->setVisible(settings);

    ui->componentButton->setVisible(item->property("_next").isValid());

    ui->propertyView->setObject(item);
}

void MainWindow::onOpenEditor(const QString &path) {
    if(m_documentModel == nullptr) {
        return;
    }
    AssetEditor *editor = m_documentModel->openFile(path);
    if(editor) {
        connect(editor, SIGNAL(updateAsset()), ui->contentBrowser, SLOT(assetUpdated()), Qt::UniqueConnection);

        if(ui->toolWidget->areaFor(editor) == nullptr) {
            QWidget *neighbor = m_mainEditor;
            for(auto &it : findChildren<QWidget *>()) {
                if(it->inherits(editor->metaObject()->className())) {
                    neighbor = it;
                    break;
                }
            }

            ui->toolWidget->removeToolWindow(editor);
            editor->setParent(this);
            ui->toolWidget->addToolWindow(editor, QToolWindowManager::ReferenceAddTo, ui->toolWidget->areaFor(neighbor));
        } else {
            ui->toolWidget->activateToolWindow(editor);
        }
    }
}

void MainWindow::onSettingsUpdated() {
    ui->commitButton->setEnabled(true);
    ui->revertButton->setEnabled(true);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QMainWindow::closeEvent(event);

    if(m_documentModel) {
        for(auto &it : m_documentModel->documents()) {
            ui->toolWidget->activateToolWindow(it);
            if(!it->checkSave()) {
                event->ignore();
                return;
            }
        }
    }

    QString str = ProjectManager::instance()->projectId();
    if(!str.isEmpty()) {
        VariantList params;
        params.push_back(qPrintable(ui->viewportWidget->map()));
        params.push_back(ui->viewportWidget->saveState());

        QSettings settings(COMPANY_NAME, EDITOR_NAME);
        settings.setValue(str, QString::fromStdString(Json::save(params)));

        SettingsManager::instance()->saveSettings();

        saveWorkspace();
    }

    QApplication::quit();
}

void MainWindow::on_commitButton_clicked() {
    AssetConverterSettings *s = dynamic_cast<AssetConverterSettings *>(ui->propertyView->object());
    if(s && s->isModified()) {
        s->saveSettings();
        AssetManager::instance()->pushToImport(s);
        AssetManager::instance()->reimport();
    }

    ui->commitButton->setEnabled(false);
    ui->revertButton->setEnabled(false);
}

void MainWindow::on_revertButton_clicked() {
    AssetConverterSettings *s = dynamic_cast<AssetConverterSettings *>(ui->propertyView->object());
    if(s && s->isModified()) {
        s->loadSettings();
        ui->propertyView->onUpdated();
    }

    ui->commitButton->setEnabled(false);
    ui->revertButton->setEnabled(false);
}

void MainWindow::on_actionNew_triggered() {
    m_documentModel->newFile(m_mainEditor);

    UndoManager::instance()->clear();
}

void MainWindow::on_actionSave_triggered() {
    if(!Engine::isGameMode()) {
        m_currentEditor->onSave();
    } else {
        QApplication::beep();
    }
}

void MainWindow::on_actionSave_As_triggered() {
    if(!Engine::isGameMode()) {
        m_currentEditor->onSaveAs();
    }
}

void MainWindow::on_actionPlay_triggered() {
    setGameMode(!Engine::isGameMode());
}

void MainWindow::on_actionPause_triggered() {
    ui->preview->setGamePause(!ui->preview->isGamePause());
    ui->pauseButton->setChecked(ui->preview->isGamePause());
}

void MainWindow::setGameMode(bool mode) {
    if(mode) {
        if(ui->preview->parent() == nullptr) {
            ui->toolWidget->moveToolWindow(ui->preview, QToolWindowManager::ReferenceAddTo, ui->toolWidget->areaFor(ui->viewportWidget));
        }
        ui->toolWidget->activateToolWindow(ui->preview);
        ui->viewportWidget->backupScenes();
        Timer::reset();
    } else {
        ui->toolWidget->activateToolWindow(ui->viewportWidget);
        ui->viewportWidget->restoreBackupScenes();

        ui->preview->setGamePause(false);
        ui->pauseButton->setChecked(false);
        ui->actionPause->setChecked(false);
    }

    ui->playButton->setChecked(mode);
    ui->actionPlay->setChecked(mode);

    m_undo->setEnabled(!mode);
    m_redo->setEnabled(!mode);

    Engine::setGameMode(mode);
}

void MainWindow::on_actionTake_Screenshot_triggered() {
    ui->viewportWidget->takeScreenshot();
}

void MainWindow::onOpenProject(const QString &path) {
    connect(m_queue, &ImportQueue::importFinished, this, &MainWindow::onImportFinished, Qt::QueuedConnection);

    ui->quickWidget->setVisible(false);
    resetWorkspace();

    m_projectModel->addProject(path);
    ProjectManager::instance()->init(path);

    m_forceReimport = false;
    QString projectSDK = ProjectManager::instance()->projectSdk();
    if(projectSDK != SDK_VERSION) {
        m_forceReimport = true;
    }

    Engine::file()->fsearchPathAdd(qPrintable(ProjectManager::instance()->importPath()), true);

    if(!PluginManager::instance()->rescanProject(ProjectManager::instance()->pluginsPath())) {
        AssetManager::instance()->rebuild();
    }

    m_forceReimport |= !Engine::reloadBundle();

    PluginManager::instance()->initSystems();

    AssetManager::instance()->rescan(m_forceReimport);

    for(QString &it : ProjectManager::instance()->platforms()) {
        QString name = it;
        name.replace(0, 1, name.at(0).toUpper());
        QAction *action = ui->menuBuild_Project->addAction(tr("Build for %1").arg(name));
        action->setProperty(qPrintable(gPlatforms), it);
        connect(action, &QAction::triggered, this, &MainWindow::onBuildProject);
    }

    ui->timeline->showBar();
}

void MainWindow::onNewProject() {
    QString path = QFileDialog::getSaveFileName(this, tr("Create New Project"),
                                                ProjectManager::instance()->myProjectsPath(), "*" + gProjectExt);
    if(!path.isEmpty()) {
        QFileInfo info(path);
        if(info.suffix().isEmpty()) {
            path += gProjectExt;
        }
        QFile file(path);
        if(file.open(QIODevice::WriteOnly)) {
            file.close();
            onOpenProject(path);
        }
    }
}

void MainWindow::onImportProject() {
    QString path = QFileDialog::getOpenFileName(this, tr("Import Existing Project"),
                                                ProjectManager::instance()->myProjectsPath(), "*" + gProjectExt);
    if(!path.isEmpty()) {
        onOpenProject(path);
    }
}

void MainWindow::onImportFinished() {
    m_documentModel = new DocumentModel;
    connect(m_documentModel, &DocumentModel::itemsSelected, this, &MainWindow::onItemsSelected);
    connect(m_documentModel, &DocumentModel::itemsUpdated, ui->propertyView, &PropertyEditor::onUpdated);

    ui->viewportWidget->init();
    m_documentModel->addEditor(ui->viewportWidget);

    QSettings settings(COMPANY_NAME, EDITOR_NAME);
    QVariant map = settings.value(ProjectManager::instance()->projectId());
    if(map.isValid()) {
        VariantList list = Json::load(map.toString().toStdString()).toList();
        if(!list.empty()) {
            auto it = list.begin();

            string map = it->toString();
            if(map.empty()) {
                on_actionNew_triggered();
            } else {
                m_documentModel->openFile(it->toString().c_str());
            }
            it++;
            VariantList params = it->toList();
            if(params.size() > 3) {
                ui->viewportWidget->restoreState(params);
            }
        } else {
            on_actionNew_triggered();
        }
    } else {
        on_actionNew_triggered();
    }
    disconnect(m_queue, nullptr, this, nullptr);

    ui->preferencesWidget->setModel(SettingsManager::instance());
    ui->projectWidget->setModel(ProjectManager::instance());

    ComponentModel::instance()->update();
    SettingsManager::instance()->loadSettings();

    ui->actionNew->setEnabled(true);
    ui->actionSave->setEnabled(true);
    ui->actionSave_As->setEnabled(true);
    ui->menuEdit->setEnabled(true);
    ui->menuWindow->setEnabled(true);
    ui->menuBuild_Project->setEnabled(true);

    if(m_forceReimport) {
        ProjectManager::instance()->setProjectSdk(SDK_VERSION);
        ProjectManager::instance()->saveSettings();
    }
}

void MainWindow::onWorkspaceActionClicked() {
    m_currentWorkspace = static_cast<QAction*>(sender())->data().toString();
    on_actionReset_Workspace_triggered();
}

void MainWindow::onToolWindowActionToggled(bool state) {
    QWidget *toolWindow = static_cast<QAction*>(sender())->data().value<QWidget *>();
    ui->toolWidget->moveToolWindow(toolWindow, state ?
                                      QToolWindowManager::NewFloatingArea :
                                      QToolWindowManager::NoArea);
}

void MainWindow::onToolWindowVisibilityChanged(QWidget *toolWindow, bool visible) {
    QAction *action = ui->menuWindow->findChild<QAction *>(toolWindow->windowTitle());
    if(action) {
        action->blockSignals(true);
        action->setChecked(visible);
        action->blockSignals(false);
    }
}

void MainWindow::on_actionSave_Workspace_triggered() {
    QString path = QFileDialog::getSaveFileName(this,
                                               tr("Save Workspace"),
                                               "workspaces",
                                               tr("Workspaces (*.ws)") );
    if(path.length() > 0) {
        QFile file(path);
        if(file.open(QIODevice::WriteOnly)) {
            QVariantMap layout;
            layout[gWindows] = ui->toolWidget->saveState();

            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << layout;

            file.write(data);
            file.close();
        }
    }
}

void MainWindow::on_actionReset_Workspace_triggered() {
    QFile file(m_currentWorkspace);
    if(file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QDataStream ds(&data, QIODevice::ReadOnly);
        QVariantMap layout;
        ds >> layout;
        ui->toolWidget->restoreState(layout.value(gWindows));

        for(auto it : ui->menuWorkspace->children()) {
            QAction *action = static_cast<QAction*>(it);
            action->blockSignals(true);
            action->setChecked((action->data().toString() == m_currentWorkspace));
            action->blockSignals(false);
        }
    }
}

void MainWindow::saveWorkspace() {
    QSettings settings(COMPANY_NAME, EDITOR_NAME);
    settings.setValue(gGeometry, saveGeometry());
    settings.setValue(gWindows, ui->toolWidget->saveState());
    settings.setValue(gWorkspace, m_currentWorkspace);
    qDebug() << "Workspace saved";
}

void MainWindow::resetWorkspace() {
    QSettings settings(COMPANY_NAME, EDITOR_NAME);
    QVariant windows = settings.value(gWindows);
    m_currentWorkspace = settings.value(gWorkspace, m_currentWorkspace).toString();
    ui->toolPanel->setVisible(true);
    ui->toolWidget->setVisible(true);
    if(!windows.isValid() || !ui->toolWidget->restoreState(windows)) {
        on_actionReset_Workspace_triggered();
    } else {
        for(auto it : ui->menuWorkspace->children()) {
            QAction *action = static_cast<QAction*>(it);
            action->blockSignals(true);
            action->setChecked((action->data().toString() == m_currentWorkspace));
            action->blockSignals(false);
        }
    }
}

void MainWindow::findWorkspaces(const QString &dir) {
    QDirIterator it(dir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        if(!info.baseName().isEmpty()) {
            QAction *action = new QAction(info.baseName(), ui->menuWorkspace);
            action->setCheckable(true);
            action->setData(info.filePath());
            ui->menuWorkspace->insertAction(ui->actionReset_Workspace, action);
            connect(action, SIGNAL(triggered()), this, SLOT(onWorkspaceActionClicked()));
        }
    }
}

void MainWindow::resetGeometry() {
    QSettings settings(COMPANY_NAME, EDITOR_NAME);
    restoreGeometry(settings.value(gGeometry).toByteArray());
}

void MainWindow::onBuildProject() {
    QAction *action = dynamic_cast<QAction *>(sender());
    if(action) {
        build(action->property(qPrintable(gPlatforms)).toString());
    }
}

void MainWindow::onCurrentToolWindowChanged(QWidget *toolWindow) {
    AssetEditor *editor = dynamic_cast<AssetEditor *>(toolWindow);
    if(editor == m_currentEditor) {
        return;
    }

    if(editor) {
        m_currentEditor = editor;
    } else {
        m_currentEditor = m_mainEditor;
    }
    ui->hierarchy->onSetRootObject(nullptr);
    m_currentEditor->onActivated();
}

void MainWindow::on_menuFile_aboutToShow() {
    QString name;
    if(m_currentEditor && m_currentEditor != m_mainEditor) {
        AssetConverterSettings *settings = m_currentEditor->documentsSettings().first();
        name = QString(" \"%1\"").arg(settings->source());
    }
    ui->actionSave->setText(tr("Save%1").arg(name));
    ui->actionSave_As->setText(tr("Save%1 As...").arg(name));
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

void MainWindow::on_actionReport_Issue_triggered() {
    QDesktopServices::openUrl(QUrl("https://github.com/thunder-engine/thunder/issues/new/choose", QUrl::TolerantMode));
}

void MainWindow::on_actionAPI_Reference_triggered() {
    QDesktopServices::openUrl(QUrl("https://doc.thunderengine.org/en/latest/reference/index.html", QUrl::TolerantMode));
}

void MainWindow::on_actionThunder_Answers_triggered() {
    QDesktopServices::openUrl(QUrl("https://github.com/thunder-engine/thunder/discussions", QUrl::TolerantMode));
}

void MainWindow::on_actionThunder_Manual_triggered() {
    QDesktopServices::openUrl(QUrl("https://doc.thunderengine.org/en/latest", QUrl::TolerantMode));
}

void MainWindow::on_actionExit_triggered() {
    closeEvent(new QCloseEvent);
}

void MainWindow::build(QString platform) {
    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("Select Target Directory"),
                                                    "",
                                                    QFileDialog::ShowDirsOnly |
                                                    QFileDialog::DontResolveSymlinks);

    if(!dir.isEmpty()) {
        ProjectManager *mgr = ProjectManager::instance();

        QStringList args;
        args << "-s" << mgr->projectPath() << "-t" << dir;

        if(!platform.isEmpty()) {
            args << "-p" << platform;
        }

        qDebug() << args.join(" ");

        m_builder->start("Builder", args);
        if(!m_builder->waitForStarted()) {
            aError() << qPrintable(m_builder->errorString());
        }
    }
}

void MainWindow::onBuildFinished(int exitCode, QProcess::ExitStatus) {
    QMessageBox msg;
    if(exitCode == 0) {
        msg.setText("Build Succeeded.");
        msg.setIcon(QMessageBox::Information);
        aInfo() << qPrintable(msg.text());
    } else {
        msg.setText("Build Failed. Please check log output for more details.");
        msg.setIcon(QMessageBox::Critical);
        aError() << qPrintable(msg.text());
    }
    msg.exec();
}

void MainWindow::readOutput() {
    emit readBuildLogs(m_builder->readAllStandardOutput());
}

void MainWindow::readError() {
    emit readBuildLogs(m_builder->readAllStandardError());
}

void MainWindow::timerEvent(QTimerEvent *) {
    Timer::update();
}
