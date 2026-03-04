#include <QCoreApplication>
#include "filereceiver.h"  // Include the header file

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    FileReceiver receiver;  // Instantiate the class
    return a.exec();
}
