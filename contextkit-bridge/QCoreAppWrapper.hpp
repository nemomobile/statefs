#ifndef _QCOREAPWRAPPER_HPP_
#define _QCOREAPWRAPPER_HPP_

#include <QObject>
#include <QThread>
#include <QCoreApplication>
#include <thread>

class QCoreAppWrapper : public QThread
{
    Q_OBJECT
public:
    QCoreAppWrapper();
    virtual ~QCoreAppWrapper();

private:
    void run();
    static int argc_;
    static char* argv_[];
    static QCoreApplication *app_;

    mutable std::mutex mutex_;
    mutable std::condition_variable exited_;
private slots:
    //void OnExec();
};

#endif // _QCOREAPWRAPPER_HPP_
