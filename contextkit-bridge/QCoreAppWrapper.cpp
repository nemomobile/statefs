#include "QCoreAppWrapper.hpp"

int QCoreAppWrapper::argc_ = 0;
char* QCoreAppWrapper::argv_[] = {nullptr};
QCoreApplication *QCoreAppWrapper::app_ = nullptr;

QCoreAppWrapper::QCoreAppWrapper()
{
    start();
}

void QCoreAppWrapper::run()
{
    if (!QCoreApplication::instance())
    {
        app_ = new QCoreApplication(argc_, argv_);
        app_->exec();
    }
}

QCoreAppWrapper::~QCoreAppWrapper()
{
    QCoreApplication::quit();
    wait(5000);
}
