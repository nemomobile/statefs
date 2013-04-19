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
        std::lock_guard<std::mutex> lock(mutex_);
        exited_.notify_one();
    }
}

QCoreAppWrapper::~QCoreAppWrapper()
{
    std::unique_lock<std::mutex> lock(mutex_);
    QCoreApplication::quit();
    exited_.wait_for(lock,  std::chrono::milliseconds(2000)
                     , [this]() { return isFinished(); });
}
