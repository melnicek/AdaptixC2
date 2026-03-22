#ifndef ADAPTIXCLIENT_HOSTEDFILESWIDGET_H
#define ADAPTIXCLIENT_HOSTEDFILESWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QMenu>

class AdaptixWidget;

enum HostedFilesColumns {
    HC_FileName = 0,
    HC_Slug,
    HC_FileSize,
    HC_MimeType,
    HC_Source,
    HC_Uploader,
    HC_Downloads,
    HC_Date,
    HC_ColumnCount
};

/// Filter Proxy Model

class HostedFilesFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    QString filterText;

public:
    explicit HostedFilesFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString &text) {
        filterText = text.toLower();
        invalidateRowsFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        if (filterText.isEmpty()) return true;
        auto *model = sourceModel();
        for (int col = 0; col < HC_ColumnCount; col++) {
            QModelIndex idx = model->index(sourceRow, col, sourceParent);
            if (model->data(idx).toString().toLower().contains(filterText))
                return true;
        }
        return false;
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        if (left.column() == HC_FileSize || left.column() == HC_Downloads) {
            return sourceModel()->data(left, Qt::UserRole).toLongLong() < sourceModel()->data(right, Qt::UserRole).toLongLong();
        }
        if (left.column() == HC_Date) {
            return sourceModel()->data(left, Qt::UserRole).toLongLong() < sourceModel()->data(right, Qt::UserRole).toLongLong();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};

/// Table Model

class HostedFilesTableModel : public QAbstractTableModel {
    Q_OBJECT
    QVector<HostedFileData> items;

public:
    explicit HostedFilesTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex & = QModelIndex()) const override { return items.size(); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return HC_ColumnCount; }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
        switch (section) {
            case HC_FileName:  return "File Name";
            case HC_Slug:      return "URL Slug";
            case HC_FileSize:  return "Size";
            case HC_MimeType:  return "MIME Type";
            case HC_Source:    return "Source";
            case HC_Uploader:  return "Uploader";
            case HC_Downloads: return "Downloads";
            case HC_Date:      return "Date";
            default: return {};
        }
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= items.size()) return {};
        const auto &item = items[index.row()];

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case HC_FileName:  return item.FileName;
                case HC_Slug:      return item.Slug;
                case HC_FileSize:  return formatSize(item.FileSize);
                case HC_MimeType:  return item.MimeType;
                case HC_Source:    return item.Source;
                case HC_Uploader:  return item.Uploader;
                case HC_Downloads: return QString::number(item.Downloads);
                case HC_Date:      return item.Date;
                default: return {};
            }
        }
        if (role == Qt::UserRole) {
            switch (index.column()) {
                case HC_FileSize:  return item.FileSize;
                case HC_Downloads: return item.Downloads;
                case HC_Date:      return item.DateTimestamp;
                default: return {};
            }
        }
        return {};
    }

    static QString formatSize(qint64 bytes) {
        if (bytes < 1024) return QString::number(bytes) + " B";
        if (bytes < 1024*1024) return QString::number(bytes/1024.0, 'f', 1) + " KB";
        if (bytes < 1024*1024*1024) return QString::number(bytes/(1024.0*1024.0), 'f', 1) + " MB";
        return QString::number(bytes/(1024.0*1024.0*1024.0), 'f', 2) + " GB";
    }

    void add(const HostedFileData &item) {
        beginInsertRows(QModelIndex(), items.size(), items.size());
        items.append(item);
        endInsertRows();
    }

    void remove(const QString &fileId) {
        for (int i = 0; i < items.size(); i++) {
            if (items[i].FileId == fileId) {
                beginRemoveRows(QModelIndex(), i, i);
                items.removeAt(i);
                endRemoveRows();
                return;
            }
        }
    }

    void clear() {
        beginResetModel();
        items.clear();
        endResetModel();
    }

    HostedFileData itemAt(int row) const {
        if (row >= 0 && row < items.size()) return items[row];
        return {};
    }
};

/// Main Widget

class HostedFilesWidget : public DockTab {
    Q_OBJECT

    AdaptixWidget*              adaptixWidget = nullptr;
    HostedFilesTableModel*      hostedModel   = nullptr;
    HostedFilesFilterProxyModel* proxyModel   = nullptr;
    QTableView*                 tableView     = nullptr;
    QWidget*                    searchWidget  = nullptr;
    QLineEdit*                  filterInput   = nullptr;

    void createUI();

public:
    explicit HostedFilesWidget(AdaptixWidget* w);

    void Clear() const;
    void AddHostedItem(const HostedFileData &data);
    void RemoveHostedItem(const QString &fileId);

public Q_SLOTS:
    void toggleSearchPanel();
    void onFilterUpdate(const QString &text);
    void handleHostedMenu(const QPoint &pos);
    void onUploadFile();
    void onCopyURL();
    void onCopyCurl();
    void onCopyWget();
    void onDeleteHosted();
};

#endif
