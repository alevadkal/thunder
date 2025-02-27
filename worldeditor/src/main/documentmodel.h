#ifndef DOCUMENTMODEL_H
#define DOCUMENTMODEL_H

#include <QObject>
#include <QMap>

class AssetEditor;

class DocumentModel : public QObject {
    Q_OBJECT

public:
    DocumentModel();
    ~DocumentModel();

    void addEditor(AssetEditor *editor);

    void newFile(AssetEditor *editor);

    AssetEditor *openFile(const QString &path);

    QList<AssetEditor *> documents();

signals:
    void itemsSelected(const QList<QObject *> &items);
    void itemsUpdated();

public slots:
    void closeFile(AssetEditor *editor);

private:
    bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void onLoadAsset(QString path);

protected:
    typedef QMap<QString, AssetEditor *> EditorsMap;

    QList<AssetEditor *> m_documents;
    EditorsMap m_editors;

};

#endif // DOCUMENTMODEL_H
