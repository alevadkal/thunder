#include "hierarchybrowser.h"
#include "ui_hierarchybrowser.h"

#include <QDrag>
#include <QPainter>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QStyledItemDelegate>

#include <object.h>
#include <components/scene.h>
#include <components/actor.h>

#include "config.h"

#include "objecthierarchymodel.h"

#define ROW_SENCE 4

class HierarchyDelegate : public QStyledItemDelegate {
public:
    explicit HierarchyDelegate(QObject *parent = nullptr) :
            QStyledItemDelegate(parent) {

    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        Q_UNUSED(index)
        editor->setGeometry(option.rect);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QModelIndex origin = index;
        const ObjectsFilter *filter = dynamic_cast<const ObjectsFilter *>(index.model());
        if(filter) {
            origin = filter->mapToSource(origin);
        }
        Object *object = static_cast<Object *>(origin.internalPointer());
        Scene *scene = dynamic_cast<Scene *>(object);
        if(scene && index.column() < 2) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 64));
            painter->drawRect(option.rect);
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
};

void TreeView::paintEvent(QPaintEvent *ev) {
    int size = header()->defaultSectionSize();
    int count = header()->count() - 2;

    QPainter painter(viewport());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 64));
    painter.drawRect(0, 0, size * count, height());

    QTreeView::paintEvent(ev);
}

class RubberBand : public QRubberBand {
public:
    explicit RubberBand(Shape shape, QWidget *parent = nullptr) :
        QRubberBand(shape, parent) {

    }

    void paintEvent(QPaintEvent *ev) {
        if(shape() == Rectangle) {
            QRubberBand::paintEvent(ev);
        } else {
            QPainter p(this);
            p.setPen(QPen(blue, 2));
            p.setBrush(Qt::white);
            p.drawLine(0, 2, geometry().width(), 2);
            p.drawEllipse(1, 1, 3, 3);
        }
    }

    QColor blue = QColor(2, 119, 189); // #0277bd
};

HierarchyBrowser::HierarchyBrowser(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::HierarchyBrowser),
        m_rect(nullptr),
        m_line(nullptr),
        m_filter(new ObjectsFilter(this)) {

    ui->setupUi(this);

    m_rect = new RubberBand(QRubberBand::Rectangle, ui->treeView);
    m_line = new RubberBand(QRubberBand::Line, ui->treeView);

    m_filter->setSourceModel(new ObjectHierarchyModel(this));

    ui->treeView->setModel(m_filter);
    ui->treeView->setItemDelegate(new HierarchyDelegate);
    ui->treeView->installEventFilter(this);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeView->itemDelegate(), SIGNAL(commitData(QWidget*)), this, SIGNAL(updated()));
    connect(ui->treeView, SIGNAL(dragStarted(Qt::DropActions)), this, SLOT(onDragStarted(Qt::DropActions)));
    connect(ui->treeView, SIGNAL(dragEnter(QDragEnterEvent*)), this, SLOT(onDragEnter(QDragEnterEvent*)));
    connect(ui->treeView, SIGNAL(dragMove(QDragMoveEvent*)), this, SLOT(onDragMove(QDragMoveEvent*)));
    connect(ui->treeView, SIGNAL(dragLeave(QDragLeaveEvent*)), this, SLOT(onDragLeave(QDragLeaveEvent*)));
    connect(ui->treeView, SIGNAL(drop(QDropEvent*)), this, SLOT(onDrop(QDropEvent*)));

    ui->treeView->header()->moveSection(2, 0);
    ui->treeView->header()->moveSection(3, 1);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeView->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->treeView->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->treeView->header()->hideSection(1);
}

HierarchyBrowser::~HierarchyBrowser() {
    delete ui;
}

void HierarchyBrowser::onSetRootObject(Object *object) {
    ObjectHierarchyModel *model = static_cast<ObjectHierarchyModel *>(m_filter->sourceModel());
    model->setRoot(object);

    ui->treeView->expandToDepth(0);
}

void expandToIndex(const QModelIndex &index, QTreeView *view) {
    QModelIndex parent = index.parent();
    if(!parent.isValid()) {
        return;
    }

    if(!view->isExpanded(parent)) {
        view->setExpanded(parent, true);
        expandToIndex(parent, view);
    }
}

void HierarchyBrowser::onObjectSelected(Object::ObjectList objects) {
    QItemSelectionModel *select = ui->treeView->selectionModel();
    QAbstractItemModel *model = ui->treeView->model();
    select->select(QModelIndex(), QItemSelectionModel::Clear);
    for(auto object : objects) {
        QModelIndexList list = model->match(model->index(0, 0), Qt::UserRole,
                                            QString::number(object->uuid()),
                                            -1, Qt::MatchExactly | Qt::MatchRecursive);
        for(QModelIndex &it : list) {
            select->select(it, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            expandToIndex(it, ui->treeView);
        }
    }
}

void HierarchyBrowser::onObjectUpdated() {
    QAbstractItemModel *model = m_filter->sourceModel();

    emit model->layoutAboutToBeChanged();
    emit model->layoutChanged();
}

void HierarchyBrowser::onDragEnter(QDragEnterEvent *e) {
    if(e->mimeData()->hasFormat(gMimeObject)) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void HierarchyBrowser::onDragLeave(QDragLeaveEvent *) {
    m_rect->hide();
    m_line->hide();
}

void HierarchyBrowser::onDragMove(QDragMoveEvent *e) {
    QModelIndex index = ui->treeView->indexAt(e->pos());
    if(index.isValid()) {
        QRect r = ui->treeView->visualRect(index);
        r.setWidth(ui->treeView->width());
        r.translate(0, ui->treeView->header()->height() + 1);

        int y = ui->treeView->header()->height() + 1 + e->pos().y();
        if(abs(r.top() - y) < ROW_SENCE && index.parent().isValid()) { // Before
            r.setBottom(r.top() + 1);
            r.setTop(r.top() - 3);

            m_rect->hide();
            m_line->show();
            m_line->setGeometry(r);
            m_line->repaint();
        } else if(abs(r.bottom() - y) < ROW_SENCE && index.parent().isValid()) { // After
            QModelIndex child = ui->treeView->model()->index(0, 0, index);
            if(child.isValid()) {
                QRect c = ui->treeView->visualRect(child);
                r.setX(c.x());
            }

            r.setTop(r.bottom() - 2);
            r.setBottom(r.bottom() + 2);

            m_rect->hide();
            m_line->show();
            m_line->setGeometry(r);
            m_line->repaint();
        } else {
            r.setX(0);

            m_line->hide();
            m_rect->show();
            m_rect->setGeometry(r);
            m_rect->repaint();
        }
    } else {
        m_rect->hide();
        m_line->hide();
    }
}

void HierarchyBrowser::onDrop(QDropEvent *e) {
    ObjectHierarchyModel *model = static_cast<ObjectHierarchyModel *>(m_filter->sourceModel());

    Object::ObjectList objects;
    Object *parent = model->root();
    int position = -1;
    if(e->mimeData()->hasFormat(gMimeObject)) {
        QString path(e->mimeData()->data(gMimeObject));
        foreach(const QString &it, path.split(";")) {
            QString id = it.left(it.indexOf(':'));
            Object *item = model->findObject(id.toUInt());
            if(item) {
                QModelIndex origin = ui->treeView->indexAt(e->pos());
                QModelIndex index = m_filter->mapToSource(origin);
                if(index.isValid()) {
                    QRect r = ui->treeView->visualRect(origin);
                    r.setWidth(ui->treeView->width());
                    r.translate(0, ui->treeView->header()->height() + 1);

                    int y = ui->treeView->header()->height() + 1 + e->pos().y();
                    if(abs(r.top() - y) < ROW_SENCE && index.parent().isValid()) {
                        // Set before
                        position = index.row();
                        index = index.parent();
                    } else if(abs(r.bottom() - y) < ROW_SENCE && index.parent().isValid()) {
                        // Set after
                        position = index.row() + 1;
                        index = index.parent();
                    }

                    if(item != index.internalPointer()) {
                        objects.push_back(item);
                        parent = static_cast<Object *>(index.internalPointer());
                    }
                }
            }
        }
    }
    if(!objects.empty()) {
        emit parented(objects, parent, position);
    }
    m_rect->hide();
    m_line->hide();
}

void HierarchyBrowser::onDragStarted(Qt::DropActions supportedActions) {
    QMimeData *mimeData = new QMimeData;
    QStringList list;
    foreach(const QModelIndex &it, ui->treeView->selectionModel()->selectedIndexes()) {
        QModelIndex index = m_filter->mapToSource(it);
        if(index.column() == 0) {
            Object *object = static_cast<Object *>(index.internalPointer());
            list.push_back(QString::number(object->uuid()) + ":" + object->name().c_str());
        }
    }
    mimeData->setData(gMimeObject, qPrintable(list.join(";")));

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(supportedActions, Qt::MoveAction);
}

void HierarchyBrowser::on_treeView_clicked(const QModelIndex &index) {
    QItemSelectionModel *select = ui->treeView->selectionModel();
    Object::ObjectList list;
    foreach(QModelIndex it, select->selectedRows()) {
        list.push_back(static_cast<Object *>(m_filter->mapToSource(it).internalPointer()));
    }

    Object *object = static_cast<Object *>(m_filter->mapToSource(index).internalPointer());
    Actor *actor = dynamic_cast<Actor *>(object);
    if(actor) {
        if(index.column() == 2) {
            actor->setHideFlags(actor->hideFlags() ^ Actor::ENABLE);
        } else if(index.column() == 3) {
            actor->setHideFlags(actor->hideFlags() ^ Actor::SELECTABLE);
        }

        emit selected(list);
    }
}

void HierarchyBrowser::on_treeView_doubleClicked(const QModelIndex &index) {
    emit focused(static_cast<Object *>(m_filter->mapToSource(index).internalPointer()));
}

void HierarchyBrowser::on_lineEdit_textChanged(const QString &arg1) {
    m_filter->setFilterFixedString(arg1);
}

void HierarchyBrowser::on_treeView_customContextMenuRequested(const QPoint &pos) {
    QItemSelectionModel *select = ui->treeView->selectionModel();

    Object::ObjectList list;
    foreach(QModelIndex it, select->selectedRows()) {
        Object *object = static_cast<Object *>(m_filter->mapToSource(it).internalPointer());
        Actor *actor = dynamic_cast<Actor *>(object);
        if(actor) {
            list.push_back(actor);
        }
    }
    if(!list.empty()) {
        emit selected(list);
    }

    QPoint point = static_cast<QWidget*>(QObject::sender())->mapToGlobal(pos);

    QModelIndex index = ui->treeView->indexAt(pos);
    Object *object = static_cast<Object *>(m_filter->mapToSource(index).internalPointer());

    emit menuRequested(object, point);
}

void HierarchyBrowser::onItemRename() {
    QItemSelectionModel *select = ui->treeView->selectionModel();
    foreach(QModelIndex it, select->selectedRows()) {
        ui->treeView->edit(it);
    }
}

void HierarchyBrowser::changeEvent(QEvent *event) {
    if(event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

bool HierarchyBrowser::eventFilter(QObject *obj, QEvent *event) {
    if(event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_Delete) {
            emit removed();
        }
        return true;
    }
    return QObject::eventFilter(obj, event);
}
