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
#include <QRegularExpression>

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
    d->format = "%.2f";
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
    if(o) {
        if(d->omap.contains(onam))
            d->omap[onam].idxs.append(idx);
        else
            d->omap.insert(onam, opropinfo(o, onam.section('/', 1, 1), idx));
    }
    else perr("CuMagic.map: object \"%s\" not found among children of \"%s\" type %s", qstoc(onam),
              qstoc(parent()->objectName()), parent()->metaObject()->className());
}

void CuMagic::map(size_t idx, QObject *obj, const QString& prop) {
    if(obj->objectName().isEmpty())
        perr("CuMagic.map: error: object %p has no name", obj);
    else if(d->omap.contains(obj->objectName()))
        d->omap[obj->objectName()].idxs.append(idx);
    else
        d->omap.insert(obj->objectName(), opropinfo(obj, prop, idx));
}

opropinfo &CuMagic::find(const QString &onam) {
    return d->omap[onam];
}

void CuMagic::mapProperty(const QString &from, const QString &to) {
    d->propmap[from] = to;
}

QString CuMagic::propMappedFrom(const char *to) {
    return d->propmap.key(to, QString());
}

QString CuMagic::propMappedTo(const char *from) {
    return d->propmap.value(from, QString());
}

void CuMagic::sendData(const CuData &da) {
    if(d->context) d->context->sendData(da);
}

void CuMagic::setSource(const QString &src) {
    QString s = m_get_idxs(src);
    qDebug() << __PRETTY_FUNCTION__ << src << "-->" << s << "idxs" << d->v_idxs;
    CuControlsReaderA *r = d->context->replace_reader(s.toStdString(), this);
    if(r) r->setSource(s);
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
    bool err = data["err"].toBool();
    std::string msg = data["msg"].toString();
    const CuVariant &dv = data["value"];
    const CuVariant &v = dv.isValid() ? dv : d->on_error_value;

    if(data["type"].toString() == "property") {
        m_configure(data);
    }

    if(!err && d->omap.size() > 0) {
        CuVariant::DataType dt = v.getType();
        QMap<QString, CuVariant> vgroup;
        switch(dt) {
        case CuVariant::Double:
            err = !m_v_split<double>(v, d->omap, vgroup);
            break;
        case CuVariant::Float:
            err = !m_v_split<float>(v, d->omap, vgroup);
            break;
        case CuVariant::Int:
            err = !m_v_split<int>(v, d->omap, vgroup);
            break;
        case CuVariant::LongInt:
            err = !m_v_split<long int>(v, d->omap, vgroup);
            break;
        case CuVariant::LongLongInt:
            err = !m_v_split<long long int>(v, d->omap, vgroup);
            break;
        case CuVariant::UInt:
            err = !m_v_split<unsigned int>(v, d->omap, vgroup);
            break;
        case CuVariant::LongUInt:
            err = !m_v_split<unsigned long int>(v, d->omap, vgroup);
            break;
        case CuVariant::LongLongUInt:
            err = !m_v_split<unsigned long long int>(v, d->omap, vgroup);
            break;
        case CuVariant::Short:
            err = !m_v_split<short>(v, d->omap, vgroup);
            break;
        case CuVariant::UShort:
            err = !m_v_split<unsigned short>(v, d->omap, vgroup);
            break;
        case CuVariant::LongDouble:
            err = !m_v_split<long double>(v, d->omap, vgroup);
            break;
        case CuVariant::Boolean:
            err = !m_v_split<bool>(v, d->omap, vgroup);
            break;
        case CuVariant::String:
            err = !m_v_str_split(v, d->omap, vgroup);
            break;
        case CuVariant::VoidPtr:
        case CuVariant::TypeInvalid:
        case CuVariant::EndDataTypes:
            err = true;
            msg = "CuMagic.onUpdate: unsupported type \"" + v.dataTypeStr(dt) + "\"";
            break;

        }
        foreach(const QString& onam, d->omap.keys()) {
            const opropinfo &opropi = d->omap[onam];
            if(!err) err = !m_prop_set(opropi.obj, vgroup[onam], opropi.prop);
            m_err_msg_set(opropi.obj, msg.c_str(), err);
        }
    }
    else if(!err) {
        printf("\e[0;33mcalling m_prop set wit v %s prop %s\e[0m\n", v.toString().c_str(), qstoc(d->t_prop));
        err = !m_prop_set(parent(), v, d->t_prop);
        m_err_msg_set(parent(), msg.c_str(), err);
    }

    emit newData(data);
}

QObject *CuMagic::get_target_object() const {
    return parent();
}

CuContext *CuMagic::getContext() const {
    return d->context;
}

QString CuMagic::format() const {
    return d->format;
}

QString CuMagic::display_unit() const {
    return d->display_unit;
}

bool CuMagic::m_prop_set(QObject *t, const CuVariant &v, const QString &prop)
{
    QStringList props;
    bool converted = false, unsupported_type = false;
    QVariant qva;
    prop.isEmpty() ?  props <<  d->propmap.value("value", "value")
                             << d->propmap.value("checked", "checked")
                             << d->propmap.value("text", "text")
                                : props << prop;
    foreach(const QString& qprop, props) {
        int pi = t->metaObject()->indexOfProperty(qprop.toLatin1().data());
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
                } break;
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
                    qva = m_convert<bool>(v);
                } break;
                case QVariant::String: {
                    std::string s = v.toString(&converted, d->format.toStdString().c_str());
                    qva = QuString(s);
                } break;
                case QVariant::StringList: {
                    qva = QuStringList(v);
                } break;
                default:
                    unsupported_type = true;
                    break;
                } // switch
            }
            if(!unsupported_type && qva.isValid()) {
                converted |= t->setProperty(qprop.toLatin1().data(), qva);
            }
        }
    }
    if(!converted)
        perr("CuMagic.m_prop_set: failed to set value %s on any of properties {%s} on %s",
             v.toString().c_str(), qstoc(props.join(",")), qstoc(t->objectName()));
    return converted;
}

bool CuMagic::m_v_str_split(const CuVariant &in, const QMap<QString, opropinfo> &opropis, QMap<QString, CuVariant> &out) {
    bool ok = true;
    out.clear();
    std::vector<std::string> dv;
    dv = in.toStringVector(&ok);
    foreach(const opropinfo& opropi, opropis) {
        std::vector <std::string> subv;
        foreach(size_t i, opropi.idxs)
            if(dv.size() > i)
                subv.push_back(dv[i]);
        out[opropi.obj->objectName()] = CuVariant(subv);
    }
    return ok;
}

// a/b/c/d[1,2,4-8,10,12-20]
QString CuMagic::m_get_idxs(const QString &src) const {
    // (\[[\d,\-]+\])
    QRegularExpression re("(\\[[\\d,\\-]+\\])");
    QRegularExpressionMatch m = re.match(src);
    QRegularExpression re2("(\\d+)\\s*\\-\\s*(\\d+)");
    bool ok = true;
    d->v_idxs.clear();
    if(m.capturedTexts().size() > 1) {
        QString s = m.capturedTexts().at(1);
        foreach(const QString &t, s.split(',', Qt::SkipEmptyParts) ) {
            if(!t.contains('-') && t.toInt(&ok) && ok)
                d->v_idxs << t.trimmed().toInt(&ok);
            else if(t.contains(re2) && ok) {
                QRegularExpressionMatch m2 = re2.match(t);
                const QStringList& ct = m2.capturedTexts();
                int to = -1, from = ct[1].toInt(&ok);
                if(ok) to = ct[2].toInt(&ok);
                for( int q = from; ok && q <= to; q++)
                    d->v_idxs << q;
            }
            if(!ok ) break;
        }
    }
    if(!ok) {
        d->v_idxs.clear();
        perr("CuMagic.m_get_idxs: error in source syntax \"%s\": correct form: a/b/c/d[1,2,3,7-12,20]", qstoc(src));
    }
    QString s(src);
    return s.remove(re);
}

void CuMagic::m_configure(const CuData &da) {
    QList<QObject *>objs;
    if(d->omap.isEmpty() ) objs << parent();
    else {
        foreach(const opropinfo& oi, d->omap.values())
            objs << oi.obj;
    }
    foreach(QObject *t, objs) {
        if(da.containsKey("min") && da.containsKey("max")) {
            double m = -1.0, M = -1.0;
            da["min"].to<double>(m);
            da["max"].to<double>(M);
            if(m != M) {
                foreach(const QString& p, QStringList() << "minimum" << "min") {
                    if(t->metaObject()->indexOfProperty(qstoc(p)) > -1) {
                        t->setProperty(p.toStdString().c_str(), m);
                    }
                }
                foreach(const QString& p, QStringList() << "maximum" << "max") {
                    if(t->metaObject()->indexOfProperty(qstoc(p)) > -1) {
                        t->setProperty(qstoc(p), M);
                    }
                }
            }
        }
        if(da.containsKey("format"))
            d->format = QuString(da, "format");
        if(da.containsKey("display_unit"))
            d->display_unit = QuString(da, "display_unit");
    }
}

void CuMagic::m_err_msg_set(QObject *o, const QString &msg, bool err) {
    QWidget *w = qobject_cast<QWidget *>(o);
    if(w && (w->metaObject()->indexOfProperty("disable_on_error") > -1 || w->property("disable_on_error").toBool()) ) {
        w->setDisabled(err);
    }
    if(w) w->setToolTip(msg);
    else if(err) perr("CuMagic: error: %s", qstoc(msg));

}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(cumbia-magic, CuMagic)
#endif // QT_VERSION < 0x050000
