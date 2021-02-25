#include "cumagic.h"
#include <cucontext.h>
#include <cucontrolsreader_abs.h>
#include <cudata.h>
#include <QTimer>
#include <QMap>
#include <QtDebug>
#include <cucontrolsreader_abs.h>
#include <qwidget.h>
#include <QMetaProperty>
#include <qustringlist.h>
#include <qustring.h>

class CuMagicPluginPrivate {
public:
    CumbiaPool *cu_pool;
    CuControlsFactoryPool fpoo;
};

CuMagicPlugin::CuMagicPlugin(QObject *parent) : QObject(parent)
{
    d = new CuMagicPluginPrivate;
}

CuMagicPlugin::~CuMagicPlugin() {
    delete d;
}

/*!
 * \brief CuMagicPlugin::new_magic create a new CuMagic instance
 *
 * See CuMagic constructor documentation
 *
 * \note
 * *source* and *property* are optional and can be specified later on the CuMagic object
 */
CuMagicI *CuMagicPlugin::new_magic(QObject *target, const QString &source, const QString &property) const {
    return new CuMagic(target, d->cu_pool, d->fpoo, source, property);
}

void CuMagicPlugin::init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool) {
    d->cu_pool = cumbia_pool;
    d->fpoo = fpool;
}

/*!
 * \brief CuMagic::CuMagic magic object that can be attached to any Qt object to display read values
 * \param target the target object, which becomes the parent of this object (means automatic destruction)
 * \param cu_pool a pointer to a previously allocated CumbiaPool
 * \param fpoo a const reference to CuControlsFactoryPool
 * \param src the source for the readings. Can additionally be provided later with setSource
 * \param property if specified, write the provided *property* instead of automatically guessing the property
 *
 * \par Implementation
 * CuMagic reads from source and tries to display the result according to the available properties of the
 * target object, in this order:
 *
 * \li value
 * \li text
 *
 * \par Error handling
 * If an error occurs and the property *disable_on_error* is not defined or defined and set to false, and the
 * target is a widget, then the widget is disabled on error.
 * The contents of *msg* stored in the data is set as tooltip, if the target is a widget
 *
 * \par New data notification
 * New data is notified by the newData signal
 *
 * \par Properties
 * \li *disable_on_error*: if false, a read error does not disable the target. Default: if widget, the target is disabled
 */
CuMagic::CuMagic(QObject *target, CumbiaPool *cu_pool, const CuControlsFactoryPool &fpoo, const QString& src, const QString& property) :
    QObject(target)
{
    d = new CuMagicPrivate;
    d->context = new CuContext(cu_pool, fpoo);
    d->on_error_value = CuVariant(-1);
    d->t_prop = property;
    if(!src.isEmpty()) CuMagic::setSource(src);
}

CuMagic::~CuMagic()
{
    printf("\e[1;31mCuMagic.~CuMagic %p\e[0m\n", this);
    if(d->context)
        delete d->context;
    delete d;
}

void CuMagic::setErrorValue(const CuVariant &v) {
    d->on_error_value = v;
}

void CuMagic::map(size_t idx, const QString &onam) {
    qDebug() << __PRETTY_FUNCTION__ << "mapping index " << idx << "(" << onam << ") "<< "into object " << onam.section('/', 0, 0) <<
                " / property " << onam.section('/', 1, 1);
    QObject *o = parent()->findChild<QObject *>(onam.section('/', 0, 0));
    if(o) d->omap.insert(idx, opropinfo(o, onam.section('/', 1, 1)));
    else perr("CuMagic.map: object \"%s\" not found among children of \"%s\" type %s", qstoc(onam),
              qstoc(parent()->objectName()), parent()->metaObject()->className());
}

void CuMagic::map(size_t idx, QObject *obj, const QString& prop) {
    d->omap.insert(idx, opropinfo(obj, prop));
}

QObject *CuMagic::mapped(size_t idx) const {
    return d->omap.value(idx).obj;
}

void CuMagic::sendData(const CuData &da) {
    if(d->context) d->context->sendData(da);
}

void CuMagic::setSource(const QString &src) {
    CuControlsReaderA *r = d->context->replace_reader(src.toStdString(), this);
    if(r) r->setSource(src);
}

void CuMagic::unsetSource() {
    d->context->disposeReader(); // empty arg: dispose all
}

/** \brief returns a reference to this object, so that it can be used as a QObject
 *         to benefit from signal/slot connections.
 *
 */
const QObject *CuMagicPlugin::get_qobject() const {
    return this;
}

QString CuMagic::source() const {
    CuControlsReaderA *r = d->context->getReader();
    return  r != nullptr ? r->source() : "";
}

void CuMagic::onUpdate(const CuData &data) {
    QObject *t = parent(); // target object
    QString from = QString::fromStdString( data["src"].toString());
    bool err = data["err"].toBool();
    std::string msg = data["msg"].toString();
    if(!err) {
        const CuVariant &dv = data["value"];
        const CuVariant &v = dv.isValid() ? dv : d->on_error_value;
        printf("CuMagic.onUpdate: %s: %s\n", qstoc(from), v.toString().c_str());
        QStringList props;
        bool converted = false, unsupported_type = false;
        d->t_prop.isEmpty() ?  props << "value" << "text" : props << d->t_prop;
        foreach(const QString& qprop, props) {
            int pi = t->metaObject()->indexOfProperty(qprop.toLatin1().data());
            QVariant qva;
            if(pi > -1 && !converted)  {
                QMetaProperty mp = t->metaObject()->property(pi);
                if(strcmp(mp.typeName(), "QVector<double>") == 0) {
                    qva = m_convert<double>(v, Vector);
                }
                else if(strcmp(mp.typeName(), "QList<double>") == 0) {
                    qva = m_convert<double>(v, List);
                }
                else if(strcmp(mp.typeName(), "QVector<int>") == 0) {
                    qva = m_convert<int>(v, Vector);
                }
                else if(strcmp(mp.typeName(), "QList<int>") == 0) {
                    qva = m_convert<int>(v, List);
                }
                else {
                    switch(mp.userType()) {
                    case QMetaType::Int: {
                        qva = m_convert<int>(v);
                    }
                        break;
                    case QMetaType::LongLong:
                    case QMetaType::Long: {
                        qva = m_convert<long long int>(v);
                    } break;
                    case QMetaType::UInt:
                    case QMetaType::UShort:
                    case QMetaType::UChar: {
                        qva = m_convert<unsigned int>(v);
                    } break;
                    case QMetaType::ULongLong:
                    case QMetaType::ULong: {
                        qva = m_convert<unsigned long long>(v);
                    } break;
                    case QMetaType::Double:
                    case QMetaType::Float: {
                        qva = m_convert<double>(v);
                    } break;
                    case QMetaType::Bool: {
                        qva = m_convert<int>(v);
                    } break;
                    case QVariant::String: {
                        qva = QuString(v);
                    }
                        break;
                    case QVariant::StringList: {
                        qva = QuStringList(v);
                    } break;
                    default:
                        unsupported_type = true;
                        break;
                    } // switch


                }

                if(!converted && !unsupported_type && d->omap.isEmpty() && qva.isValid()) {
                    converted = t->setProperty(qprop.toLatin1().data(), qva);
                    qDebug() << __PRETTY_FUNCTION__ << "----- " << qva << "on " << qprop.toLatin1().data() << "success? " << converted;
                }
                else if(!converted && !unsupported_type && qva.isValid()) {
                    printf("\e[1;32mCuMagic.onUpdate: mappings defined for object %s\e[0m\n", qstoc(parent()->objectName()));
                    foreach(int k, d->omap.keys()) {
                        qDebug() << __PRETTY_FUNCTION__ << "- " << k;
                        for(int i = 0 ; i < d->omap.values(k).size(); i++)
                            qDebug() << __PRETTY_FUNCTION__ << "\t- " << d->omap.values()[i].obj << " ( prop" << d->omap.values()[i].prop << ")";
                    }
                    qDebug() << __PRETTY_FUNCTION__ << "---> QVariant " << qva << "type" << qva.userType() << "str type" << qva.typeName();
                }

            } // not converted
        } // foreach

        if((err = !converted) )
            msg = "CuMagic.onUpdate: conversion error in data {%s" + data.toString() + "}";
        bool disable_on_err = t->metaObject()->indexOfProperty("disable_on_err") < 0 || t->property("disable_on_err").toBool();
        QWidget *w = qobject_cast<QWidget *>(parent());
        if(w) {
            w->setDisabled(disable_on_err && err);
            w->setToolTip(msg.c_str());
        }
        emit newData(data);
    }
}

QObject *CuMagic::get_target_object() const {
    return parent();
}

CuContext *CuMagic::getContext() const {
    return d->context;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(cumbia-magic, CuMagic)
#endif // QT_VERSION < 0x050000
