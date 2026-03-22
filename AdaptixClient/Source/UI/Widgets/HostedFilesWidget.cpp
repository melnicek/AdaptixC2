#include <UI/Widgets/HostedFilesWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>

REGISTER_DOCK_WIDGET(HostedFilesWidget, "Hosted Files", false)

HostedFilesWidget::HostedFilesWidget(AdaptixWidget* w)
    : DockTab("Hosted Files", w->GetProfile()->GetProject(), ":/icons/downloads")
    , adaptixWidget(w)
{
    createUI();
    connect(tableView, &QTableView::customContextMenuRequested, this, &HostedFilesWidget::handleHostedMenu);
    auto* shortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(shortcut, &QShortcut::activated, this, &HostedFilesWidget::toggleSearchPanel);
    this->dockWidget->setWidget(this);
}

void HostedFilesWidget::createUI()
{
    hostedModel = new HostedFilesTableModel(this);
    proxyModel  = new HostedFilesFilterProxyModel(this);
    proxyModel->setSourceModel(hostedModel);
    proxyModel->setSortRole(Qt::UserRole);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setSortingEnabled(true);
    tableView->setAlternatingRowColors(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setItemDelegate(new PaddingDelegate(5, tableView));

    tableView->setColumnWidth(HC_FileName, 200);
    tableView->setColumnWidth(HC_Slug, 200);
    tableView->setColumnWidth(HC_FileSize, 80);
    tableView->setColumnWidth(HC_MimeType, 150);
    tableView->setColumnWidth(HC_Source, 70);
    tableView->setColumnWidth(HC_Uploader, 80);
    tableView->setColumnWidth(HC_Downloads, 80);

    // Search panel
    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);
    auto *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    filterInput = new QLineEdit(searchWidget);
    filterInput->setPlaceholderText("Filter...");
    searchLayout->addWidget(filterInput);
    connect(filterInput, &QLineEdit::textChanged, this, &HostedFilesWidget::onFilterUpdate);

    auto *mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(searchWidget, 0, 0);
    mainLayout->addWidget(tableView, 1, 0);
}

void HostedFilesWidget::Clear() const
{
    hostedModel->clear();
}

void HostedFilesWidget::AddHostedItem(const HostedFileData &data)
{
    {
        QWriteLocker locker(&adaptixWidget->HostedFilesLock);
        if (adaptixWidget->HostedFiles.contains(data.FileId))
            return;
        adaptixWidget->HostedFiles[data.FileId] = data;
    }
    hostedModel->add(data);
}

void HostedFilesWidget::RemoveHostedItem(const QString &fileId)
{
    {
        QWriteLocker locker(&adaptixWidget->HostedFilesLock);
        adaptixWidget->HostedFiles.remove(fileId);
    }
    hostedModel->remove(fileId);
}

void HostedFilesWidget::toggleSearchPanel()
{
    searchWidget->setVisible(!searchWidget->isVisible());
    if (searchWidget->isVisible())
        filterInput->setFocus();
}

void HostedFilesWidget::onFilterUpdate(const QString &text)
{
    proxyModel->setFilterText(text);
}

void HostedFilesWidget::handleHostedMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction("Upload File", this, &HostedFilesWidget::onUploadFile);
    menu.addSeparator();

    QModelIndex index = tableView->indexAt(pos);
    if (index.isValid()) {
        menu.addAction("Copy URL", this, &HostedFilesWidget::onCopyURL);
        menu.addAction("Copy curl command", this, &HostedFilesWidget::onCopyCurl);
        menu.addAction("Copy wget command", this, &HostedFilesWidget::onCopyWget);
        menu.addSeparator();
        menu.addAction("Delete", this, &HostedFilesWidget::onDeleteHosted);
    }
    menu.exec(tableView->viewport()->mapToGlobal(pos));
}

void HostedFilesWidget::onUploadFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select file to host");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        MessageError("Failed to open file");
        return;
    }
    QByteArray content = file.readAll();
    file.close();

    QString fileName = QFileInfo(filePath).fileName();

    HttpReqHostedUploadAsync(fileName, content, *(adaptixWidget->GetProfile()),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Upload failed" : message);
        });
}

void HostedFilesWidget::onCopyURL()
{
    QModelIndex idx = proxyModel->mapToSource(tableView->currentIndex());
    auto item = hostedModel->itemAt(idx.row());
    if (!item.URL.isEmpty())
        QApplication::clipboard()->setText(item.URL);
}

void HostedFilesWidget::onCopyCurl()
{
    QModelIndex idx = proxyModel->mapToSource(tableView->currentIndex());
    auto item = hostedModel->itemAt(idx.row());
    if (!item.URL.isEmpty())
        QApplication::clipboard()->setText(QString("curl -k '%1' -o %2").arg(item.URL, item.FileName));
}

void HostedFilesWidget::onCopyWget()
{
    QModelIndex idx = proxyModel->mapToSource(tableView->currentIndex());
    auto item = hostedModel->itemAt(idx.row());
    if (!item.URL.isEmpty())
        QApplication::clipboard()->setText(QString("wget --no-check-certificate '%1' -O %2").arg(item.URL, item.FileName));
}

void HostedFilesWidget::onDeleteHosted()
{
    auto selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    auto reply = QMessageBox::question(this, "Delete Hosted Files",
        QString("Delete %1 hosted file(s)?").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QStringList ids;
    for (const auto &idx : selected) {
        QModelIndex srcIdx = proxyModel->mapToSource(idx);
        auto item = hostedModel->itemAt(srcIdx.row());
        if (!item.FileId.isEmpty())
            ids.append(item.FileId);
    }

    HttpReqHostedDeleteAsync(ids, *(adaptixWidget->GetProfile()),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Delete failed" : message);
        });
}
