#include "magicdemo.h"
#include "ui_magicdemo.h"

// cumbia
#include <cumbiapool.h>
#include <cuserviceprovider.h>
#include <cumacros.h>
#include <quapps.h>
// cumbia

#include <cumagicplugininterface.h>
#include <QtDebug>
#include <QMetaProperty>
#include <quplotcurve.h>

MyDisplayVector::MyDisplayVector(QWidget *parent) : QuPlotBase(parent) {
    setXAxisAutoscaleEnabled(true);
    setYAxisAutoscaleEnabled(true);
}

void MyDisplayVector::setMyData(const QList<double> &y) {
    m_data = y;
    QVector<double> x;
    for(int i = 0; i < y.size(); i++)
        x << i;
    if(!curve("MyData curve"))
        addCurve("MyData curve");
    setData("MyData curve", x, y.toVector());
    refresh();
}

QList<double> MyDisplayVector::myData() const {
    qDebug() << __PRETTY_FUNCTION__ << "data: " << m_data;
    return m_data;
}

Magicdemo::Magicdemo(CumbiaPool *cumbia_pool, QWidget *parent) :
    QWidget(parent)
{
    // cumbia
    CuModuleLoader mloader(cumbia_pool, &m_ctrl_factory_pool, &m_log_impl);
    cu_pool = cumbia_pool;
    ui = new Ui::Magicdemo;
    ui->setupUi(this, cu_pool, m_ctrl_factory_pool);

    // mloader.modules() to get the list of loaded modules
    // cumbia

    // load magic plugin
    QObject *magic_plo;
    CuMagicPluginInterface *plugin_i = CuMagicPluginInterface::get_instance(cumbia_pool, m_ctrl_factory_pool, magic_plo);
    if(!plugin_i)
        perr("Magicdemo: failed to load plugin \"%s\"", CuMagicPluginInterface::file_name);
    else {
        CuMagicI *magic1 = plugin_i->new_magic(ui->lcdNumber, "$1/double_scalar");
        CuMagicI *magic2 = plugin_i->new_magic(ui->progressBar, "$1/short_scalar");
        CuMagicI *magic3 = plugin_i->new_magic(ui->textBrowser, "$1/string_scalar", "html");
        CuMagicI *magic4 = plugin_i->new_magic(ui->plot, "$1/double_spectrum_ro[1-10,10,10,10,15,16,20-26]", "myData");
        magic4->mapProperty("min", "yLowerBound");
        magic4->mapProperty("max", "yUpperBound");
        CuMagicI *magic5 = plugin_i->new_magic(this, "$1/double_spectrum");
        CuMagicI *magic6 = plugin_i->new_magic(ui->checkBox, "$1/boolean_scalar", "checked");
        magic5->map(0, "x0");
        magic5->map(1, "x1");
        magic5->map(2, "x2");
        magic5->map(3, "x3");
        magic5->map(4, "x4");

        CuMagicI *magic7 = plugin_i->new_magic(ui->x0_2, "$1/double_spectrum[0]");
        CuMagicI *magic8 = plugin_i->new_magic(ui->x1_2, "$1/double_spectrum[1]");
        CuMagicI *magic9 = plugin_i->new_magic(ui->x2_2, "$1/double_spectrum[2]");
        CuMagicI *magic10 = plugin_i->new_magic(ui->x3_2, "$1/double_spectrum[3]");
        CuMagicI *magic11 = plugin_i->new_magic(ui->x4_2, "$1/double_spectrum[4]");

    }

    ///
    ///
    /// test
    ///
    ///
    QObject *plo = new QObject(this);
    plo->setProperty("intlist", QList<QVariant>() << 1 << 2 << 3);
    plo->setProperty("emptyintlist", QList<QVariant> ());

    qDebug() << __PRETTY_FUNCTION__ << plo->property("intlist") << plo->property("emptyintlist");

    qDebug() << __PRETTY_FUNCTION__ << ui->plot->property("myData");

    QMetaProperty mp = ui->plot->metaObject()->property(ui->plot->metaObject()->indexOfProperty("myData"));
    qDebug() << __PRETTY_FUNCTION__ << "myData property type" << mp.type() << "my data prop userType"
        << mp.userType() << "type name" << mp.typeName() << "QMetaType::type from name" << QMetaType::type(mp.typeName())
        << "type from meta type" << QMetaType::type(mp.typeName());
    std::vector<double> v1{ 1, 2, 2.3, 2.7, 3.45 };
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QList<double> vl(v1.begin(), v1.end());
#else
    QList<double> vl = QVector<double>::fromStdVector(v1).toList();
#endif
    qDebug() << __PRETTY_FUNCTION__ << "setting property my data " << vl;
    QVariant v = QVariant::fromValue(v1);
    bool ok = ui->plot->setProperty("myData", v);
    qDebug() << __PRETTY_FUNCTION__ << "done ? " << ok << "variant " << v;

}

Magicdemo::~Magicdemo()
{
    delete ui;
}

