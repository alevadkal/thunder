#include "editor/asseteditor.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMap>

#include "editor/projectmanager.h"

AssetEditor::AssetEditor() {

}
AssetEditor::~AssetEditor() {

}

void AssetEditor::onNewAsset() {
    m_settings.clear();
}

void AssetEditor::loadAsset(AssetConverterSettings *settings) {
    m_settings = { settings };
}

void AssetEditor::saveAsset(const QString &path) {
    Q_UNUSED(path)
}

bool AssetEditor::isSingleInstance() const {
    return true;
}

AssetEditor *AssetEditor::createInstance() {
    return nullptr;
}

const QList<AssetConverterSettings *> &AssetEditor::documentsSettings() const {
    return m_settings;
}

void AssetEditor::setModified(bool flag) {
    Q_UNUSED(flag)
}

void AssetEditor::onActivated() {
    emit itemsSelected({});
}

int AssetEditor::closeAssetDialog() {
    QMessageBox msgBox(nullptr);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText(tr("The asset has been modified."));
    msgBox.setInformativeText(tr("Do you want to save your changes?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    return msgBox.exec();
}

bool AssetEditor::checkSave() {
    if(isModified()) {
        int result = closeAssetDialog();
        if(result == QMessageBox::Cancel) {
            return false;
        } else if(result == QMessageBox::Yes) {
            onSave();
        } else {
            setModified(false);
        }
    }
    return true;
}

void AssetEditor::onSave() {
    if(!m_settings.isEmpty()) {
        if(!m_settings.first()->source().isEmpty()) {
            saveAsset(m_settings.first()->source());
        } else {
            onSaveAs();
        }
    }
}

void AssetEditor::onSaveAs() {
    if(m_settings.isEmpty()) {
        return;
    }
    QString dir = ProjectManager::instance()->contentPath();

    QString assetType = m_settings.first()->typeName();

    QMap<QString, QStringList> dictionary;
    for(auto &it : suffixes()) {
        dictionary[assetType].push_back(it);
    }

    QStringList filter;
    for(auto it = dictionary.begin(); it != dictionary.end(); ++it) {
        QString item = it.key() + " (";
        for(auto &suffix : it.value()) {
            item += "*." + suffix;
        }
        item += ")";
        filter.push_back(item);
    }

    QString path = QFileDialog::getSaveFileName(nullptr,
                                                QString("Save ") + assetType,
                                                dir, filter.join(";;"));
    if(!path.isEmpty()) {
        QFileInfo info(path);
        if(info.suffix().isEmpty()) {
            path += "." + dictionary.begin().value().front();
        }
        saveAsset(path);
    }
}
