#ifndef _QCOREAPWRAPPER_HPP_
#define _QCOREAPWRAPPER_HPP_

#include <QObject>
#include <QThread>
#include <QCoreApplication>

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

private slots:
    //void OnExec();
};

#endif // _QCOREAPWRAPPER_HPP_
