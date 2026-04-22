#include <QApplication>
#include <QFont>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Ledger");
    a.setApplicationDisplayName("Ledger");
    a.setFont(QFont("Noto Sans", 10));

    MainWindow w;
    w.show();

    return a.exec();
}