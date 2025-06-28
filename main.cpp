#include <QApplication>
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QProcess>
#include <QLabel>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>

class ArchiveTool : public QMainWindow {
    Q_OBJECT

public:
    ArchiveTool(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Archive Tool");
        resize(700, 500);
        setAcceptDrops(true);

        QWidget *central = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(central);

        infoLabel = new QLabel("Drag an archive to extract or create a new archive.");
        layout->addWidget(infoLabel);

        fileList = new QListWidget(this);
        fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        layout->addWidget(fileList);

        progressBar = new QProgressBar(this);
        progressBar->setMinimum(0);
        progressBar->setMaximum(100);
        progressBar->setValue(0);
        progressBar->setTextVisible(true);
        layout->addWidget(progressBar);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        extractButton = new QPushButton("Extract Selected");
        extractAllButton = new QPushButton("Extract All");
        saveArchiveButton = new QPushButton("Save Archive As...");
        btnLayout->addWidget(extractButton);
        btnLayout->addWidget(extractAllButton);
        btnLayout->addWidget(saveArchiveButton);
        layout->addLayout(btnLayout);

        extractButton->setEnabled(false);
        extractAllButton->setEnabled(false);
        saveArchiveButton->setEnabled(false);

        setCentralWidget(central);

        QMenu *fileMenu = menuBar()->addMenu("File");
        QAction *newArchiveAction = fileMenu->addAction("New Archive");
        connect(newArchiveAction, &QAction::triggered, this, &ArchiveTool::newArchive);

        connect(extractButton, &QPushButton::clicked, this, &ArchiveTool::extractSelectedFiles);
        connect(extractAllButton, &QPushButton::clicked, this, &ArchiveTool::extractAllFiles);
        connect(saveArchiveButton, &QPushButton::clicked, this, &ArchiveTool::saveArchive);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.isEmpty()) return;

        if (createMode) {
            for (const QUrl &url : urls) {
                QString path = url.toLocalFile();
                if (!path.isEmpty() && !newArchiveFiles.contains(path)) {
                    newArchiveFiles << path;
                    fileList->addItem(QFileInfo(path).fileName());
                }
            }
        } else {
            archivePath = urls.first().toLocalFile();
            if (!QFileInfo(archivePath).exists()) return;
            listArchiveContents();
        }
    }

private:
    QListWidget *fileList;
    QPushButton *extractButton;
    QPushButton *extractAllButton;
    QPushButton *saveArchiveButton;
    QLabel *infoLabel;
    QProgressBar *progressBar;

    QString archivePath;
    QStringList archiveContents;
    QStringList newArchiveFiles;
    bool createMode = false;

    void listArchiveContents() {
        fileList->clear();
        archiveContents.clear();
        extractButton->setEnabled(false);
        extractAllButton->setEnabled(false);
        saveArchiveButton->setEnabled(false);
        createMode = false;
        progressBar->setValue(0);

        QFileInfo fileInfo(archivePath);
        QString suffix = fileInfo.suffix().toLower();
        QProcess proc;
        QStringList args;

        if (suffix == "7z") {
            args << "l" << "-ba" << archivePath;
            proc.start("7z", args);
        } else if (suffix == "rar") {
            args << "lb" << archivePath;
            proc.start("unrar", args);
        } else if (suffix == "tar" || suffix == "xz") {
            args << "-tf" << archivePath;
            proc.start("tar", args);
        } else if (suffix == "zip") {
            args << "-l" << archivePath;
            proc.start("unzip", args);
        } else {
            QMessageBox::warning(this, "Unsupported", "Unsupported file type.");
            return;
        }

        if (!proc.waitForFinished(8000)) {
            QMessageBox::warning(this, "Error", "Failed to list archive.");
            return;
        }

        QString output = proc.readAllStandardOutput();
        QStringList lines = output.split('\n', QString::SkipEmptyParts);

        for (const QString &line : lines) {
            if (suffix == "zip" && (line.contains("----------") || line.startsWith(" Length") || line.startsWith(" Archive:") || line.startsWith("   Date"))) continue;

            QString name = line.trimmed();
            if (suffix == "7z" || suffix == "rar") {
                name = line.section(' ', -1);
            }

            fileList->addItem(name);
            archiveContents << name;
        }

        extractButton->setEnabled(true);
        extractAllButton->setEnabled(true);
        progressBar->setValue(100);
    }

    void extractSelectedFiles() {
        QList<QListWidgetItem*> selectedItems = fileList->selectedItems();
        if (selectedItems.isEmpty()) return;

        QString destDir = QFileDialog::getExistingDirectory(this, "Select Extract Directory");
        if (destDir.isEmpty()) return;

        int total = selectedItems.size();
        int count = 0;

        for (QListWidgetItem *item : selectedItems) {
            QString filename = item->text();
            if (extractSingleFile(filename, destDir)) {
                count++;
            }
            progressBar->setValue((count * 100) / total);
        }

        QMessageBox::information(this, "Done", "Selected files extracted.");
        progressBar->setValue(100);
    }

    void extractAllFiles() {
        QString destDir = QFileDialog::getExistingDirectory(this, "Extract All To");
        if (destDir.isEmpty()) return;

        int total = archiveContents.size();
        int count = 0;

        for (const QString &filename : archiveContents) {
            extractSingleFile(filename, destDir);
            count++;
            progressBar->setValue((count * 100) / total);
        }

        QMessageBox::information(this, "Done", "All files extracted.");
        progressBar->setValue(100);
    }

    bool extractSingleFile(const QString &filename, const QString &destDir) {
        QFileInfo fileInfo(archivePath);
        QString suffix = fileInfo.suffix().toLower();
        QProcess proc;

        if (suffix == "7z") {
            proc.start("7z", QStringList() << "e" << archivePath << filename << "-o" + destDir << "-y");
        } else if (suffix == "rar") {
            proc.start("unrar", QStringList() << "e" << archivePath << filename << destDir);
        } else if (suffix == "zip") {
            proc.setWorkingDirectory(destDir);
            proc.start("unzip", QStringList() << archivePath << filename);
        } else if (suffix == "tar" || suffix == "xz") {
            proc.setWorkingDirectory(destDir);
            proc.start("tar", QStringList() << "-xvf" << archivePath << filename);
        }

        return proc.waitForFinished(15000);
    }

    void newArchive() {
        fileList->clear();
        newArchiveFiles.clear();
        archivePath.clear();
        createMode = true;
        extractButton->setEnabled(false);
        extractAllButton->setEnabled(false);
        saveArchiveButton->setEnabled(true);
        progressBar->setValue(0);
        infoLabel->setText("Drag files here to add to new archive.");
    }

    void saveArchive() {
        QString outPath = QFileDialog::getSaveFileName(this, "Save Archive As", "", "Archives (*.zip *.7z *.tar *.xz)");
        if (outPath.isEmpty() || newArchiveFiles.isEmpty()) return;

        QFileInfo fi(outPath);
        QString suffix = fi.suffix().toLower();
        QProcess proc;
        QStringList args;

        if (suffix == "zip") {
            args << outPath;
            args << newArchiveFiles;
            proc.start("zip", args);
        } else if (suffix == "7z") {
            args << "a" << outPath;
            args << newArchiveFiles;
            proc.start("7z", args);
        } else if (suffix == "tar") {
            args << "-cvf" << outPath;
            args << newArchiveFiles;
            proc.start("tar", args);
        } else if (suffix == "xz") {
            QString tarPath = outPath;
            if (!tarPath.endsWith(".tar.xz")) tarPath += ".tar.xz";
            args << "-cJf" << tarPath;
            args << newArchiveFiles;
            proc.start("tar", args);
        } else {
            QMessageBox::warning(this, "Unsupported", "Unsupported archive format.");
            return;
        }

        progressBar->setValue(50);
        if (!proc.waitForFinished(10000)) {
            QMessageBox::warning(this, "Error", "Failed to create archive.");
        } else {
            QMessageBox::information(this, "Archive Saved", "Archive saved successfully.");
            fileList->clear();
            newArchiveFiles.clear();
            createMode = false;
            progressBar->setValue(100);
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ArchiveTool win;
    win.show();
    return app.exec();
}
