#ifndef Magicdemo_H
#define Magicdemo_H

#include <QWidget>

// cumbia
#include <qulogimpl.h>
#include <cucontrolsfactorypool.h>
#include <quplot_base.h>
class CumbiaPool;
// cumbia

namespace Ui {
class Magicdemo;
}

class MyDisplayVector : public QuPlotBase {
    Q_OBJECT

    Q_PROPERTY(QList <double> myData READ myData WRITE setMyData)
public:
    MyDisplayVector(QWidget *parent);

    void setMyData(const QList<double> &y);
    QList<double> myData() const;

private:
    QList<  double> m_data;
};

class Magicdemo : public QWidget
{
    Q_OBJECT

public:
    explicit Magicdemo(CumbiaPool *cu_p, QWidget *parent = 0);
    ~Magicdemo();

private:
    Ui::Magicdemo *ui;

    // cumbia
    CumbiaPool *cu_pool;
    QuLogImpl m_log_impl;
    CuControlsFactoryPool m_ctrl_factory_pool;
    // cumbia
};

#endif // Magicdemo_H
