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
    qRegisterMetaType<CuMatrix<double>>("CuMatrix<double>");
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
 * \li checked
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
    d->onetime = false;
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
    const QString &s = m_get_idxs(src); // s has "\[([\d,\-]+)\]" removed
    qDebug() << __PRETTY_FUNCTION__ << src << "-->" << s << "idxs" << d->v_idxs << d->omap.keys();
    // if indexes change but src is unchanged, do not d->context->replace_reader
    if(s != d->src) {
        CuControlsReaderA *r = d->context->replace_reader(s.toStdString(), this);
        if(r) {
            r->setSource(s);
            d->src = s; // bare src, not r->source
        }
    }
}

QString CuMagic::source() const {
    CuControlsReaderA *r = d->context->getReader();
    QString idx_selector = m_idxs_to_string();
    if(idx_selector.size()) idx_selector = "[" + idx_selector + "]";
    return  r != nullptr ? r->source() + idx_selector : "";
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

void CuMagic::onUpdate(const CuData &data) {
    bool err = data["err"].toBool();
    const std::string& m = data.s("msg");
    std::string msg = source().toStdString();
    if(m.length() > 0) msg += "\n" + data["msg"].toString();
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
        case CuVariant::Char:
            err = !m_v_split<char>(v, d->omap, vgroup);
            break;
        case CuVariant::UChar:
            err = !m_v_split<unsigned char>(v, d->omap, vgroup);
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
        case CuVariant::EndVariantTypes:
            err = true;
            msg = "CuMagic.onUpdate: unsupported type \"" + v.dataTypeStr(dt) + "\"";
            break;

        }
        foreach(const QString& onam, d->omap.keys()) {
            const opropinfo &opropi = d->omap[onam];
            if(!err) err = !m_prop_set(opropi.obj, vgroup[onam], opropi.prop);
            m_err_msg_set(opropi.obj, opropi.idxs, opropi.prop, msg.c_str(), err);
        }
    }
    else if(!err) {
        cuprintf("\e[0;33mcalling m_prop set wit v %s prop %s\e[0m\n", v.toString().c_str(), qstoc(d->t_prop));
        err = !m_prop_set(parent(), v, d->t_prop);
        m_err_msg_set(parent(), d->v_idxs, d->t_prop, msg.c_str(), err);
    }

    emit newData(data);
    if(d->onetime) {
        unsetSource();
        deleteLater();
    }
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
    const CuVariant::DataFormat fmt = v.getFormat();
    QString con_p = ""; // property name on which setProperty succeeded
    prop.isEmpty() ?  props <<  d->propmap.value("value", "value")
                             << d->propmap.value("checked", "checked")
                             << d->propmap.value("text", "text")
                                : props << prop;
    int pi = -1;
    for(int i = 0; i < props.size() && pi < 0; i++) {
        const QString& qprop = props[i];
        pi = t->metaObject()->indexOfProperty(qprop.toLatin1().data());
        if(fmt == CuVariant::Matrix) {
            QVariant var;
            switch (v.getType()) {
            case CuVariant::Double: {
                CuMatrix<double> md = v.toMatrix<double>();
                var.setValue(md);
                t->setProperty(qprop.toLatin1(), var);
            }break;
            case CuVariant::LongDouble: {
                CuMatrix<long double> mld = v.toMatrix<long double>();
                var.setValue(mld);
            }break;
            case CuVariant::Float: {
                CuMatrix<float>mf = v.toMatrix<float>();
                var.setValue(mf);
            }break;
            case CuVariant::Int: {
                CuMatrix<int>mi = v.toMatrix<int>();
                var.setValue(mi);
            }break;
            case CuVariant::Char: {
                CuMatrix<char>mch = v.toMatrix<char>();
                var.setValue(mch);
            }break;
            case CuVariant::UChar: {
                CuMatrix<unsigned char>much = v.toMatrix<unsigned char>();
                var.setValue(much);
            }break;
            case CuVariant::UShort: {
                CuMatrix<unsigned short int> mus = v.toMatrix<unsigned short int>();
                var.setValue(mus);
            }break;
            case CuVariant::Short: {
                CuMatrix<short>ms = v.toMatrix<short>();
                var.setValue(ms);
            }break;
            case CuVariant::UInt: {
                CuMatrix<unsigned int>mui = v.toMatrix<unsigned int>();
                var.setValue(mui);
            }break;
            case CuVariant::LongUInt: {
                CuMatrix<long unsigned int>muli = v.toMatrix<long unsigned int>();
                var.setValue(muli);
            }break;
            case CuVariant::LongLongUInt: {
                CuMatrix<long long unsigned int>mulli = v.toMatrix<long long unsigned int>();
                var.setValue(mulli);
            }break;
            case CuVariant::LongLongInt: {
                CuMatrix<long long int>mlli = v.toMatrix<long long int>();
                var.setValue(mlli);
            }break;
            case CuVariant::LongInt: {
                CuMatrix<long int>mli = v.toMatrix<long int>();
                var.setValue(mli);
            }break;
            case CuVariant::Boolean: {
                CuMatrix<bool> mabo = v.toMatrix<bool>();
                var.setValue(mabo);
            }break;
            case CuVariant::String: {
                CuMatrix<std::string> mas = v.toMatrix<std::string>();
                var.setValue(mas);
            }break;
            case CuVariant::TypeInvalid:
                break;
            default:
                perr("CuMagic::m_prop_set: cannot convert type %d (%s) to matrix", v.getType(), v.dataTypeStr(v.getType()).c_str());
                break;
            } // switch (v.getType())
            if(var.isValid())
                t->setProperty(qprop.toLatin1(), var);
        } // end matrix format
        else if(pi > -1 && !converted && (fmt == CuVariant::Scalar || fmt == CuVariant::Vector))  {
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
                    qva = m_str_convert(v);
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
                if(converted) con_p = qprop;
            }
        }
        else if(pi < 0 && !converted && !prop.isEmpty()) {
            cuprintf("CuMagic::m_prop_set \e[1;32m HAS NO PROPERY DEFINED SO SETTING FREEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\e[0m\n");
            CuVariant::DataType ty = v.getType();
            switch(v.getFormat()) {
            case CuVariant::Scalar: {
                converted = true; // cannot use the return value of setProperty: it is false for dynamic props
                switch(ty) {
                case CuVariant::Double:
                case CuVariant::LongDouble: {
                    double dou;
                    v.to<double>(dou);
                    cuprintf("CuMagic::m_prop_set %s DOUBLE! %f\n", qprop.toLatin1().data(),   dou);
                    t->setProperty(qprop.toLatin1(), dou);
                    cuprintf("CuMagic::m_prop_set DOUBLE readback %f\n", t->property(qprop.toLatin1()).toDouble());
                }break;
                case CuVariant::Float:
                    t->setProperty(qprop.toLatin1(), v.toFloat());
                    break;
                case CuVariant::Int:
                    t->setProperty(qprop.toLatin1(), v.toInt());
                    break;
                case CuVariant::Short:
                    t->setProperty(qprop.toLatin1(), v.toShortInt());
                    break;
                case CuVariant::UInt:
                    t->setProperty(qprop.toLatin1(), v.toUInt());
                    break;
                case CuVariant::LongUInt:
                case CuVariant::LongLongUInt: {
                    long long unsigned ll = 0;
                    v.to<long long unsigned>(ll);
                    t->setProperty(qprop.toLatin1(), ll);
                }break;
                case CuVariant::LongLongInt:
                case CuVariant::LongInt: {
                    long long int lli = 0;
                    v.to<long long int>(lli);
                    t->setProperty(qprop.toLatin1(), lli);
                } break;
                case CuVariant::UShort:
                    t->setProperty(qprop.toLatin1(), v.toUShortInt());
                    break;
                case CuVariant::Boolean: {
                    bool b;
                    v.to<bool>(b);
                    t->setProperty(qprop.toLatin1(), b);
                }break;
                case CuVariant::String:
                    t->setProperty(qprop.toLatin1(), QString::fromStdString(v.toString()));
                    break;

                default:
                    converted = false;
                    break;

                }break;
            case CuVariant::Vector: {
                    converted = true; // see scalar above
                    switch(ty) {
                    case CuVariant::Double:
                    case CuVariant::LongDouble: {
                        std::vector<double> vdou;
                        v.toVector<double>(vdou);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vdou.begin(), vdou.end());
#else
                        QVariantList vl;
                        foreach(double d, vdou)
                            vl << d;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::Float: {
                        std::vector<float> vf;
                        v.toVector<float>(vf);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vf.begin(), vf.end());
#else
                        QVariantList vl;
                        foreach(float f, vf)
                            vl << f;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::Int: {
                        std::vector<int> vi;
                        v.toVector<int>(vi);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vi.begin(), vi.end());
#else
                        QVariantList vl;
                        foreach(int i, vi)
                            vl << i;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::Short: {
                        std::vector<short> vsi;
                        v.toVector<short>(vsi);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vsi.begin(), vsi.end());
#else
                        QVariantList vl;
                        foreach(short si, vsi)
                            vl << si;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::UInt: {
                        std::vector<unsigned> vui;
                        v.toVector<unsigned>(vui);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vui.begin(), vui.end());
#else
                        QVariantList vl;
                        foreach(unsigned ui, vui)
                            vl << ui;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::LongUInt:
                    case CuVariant::LongLongUInt: {
                        std::vector<unsigned long long> vull;
                        v.toVector<unsigned long long>(vull);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vull.begin(), vull.end());
#else
                        QVariantList vl;
                        foreach(unsigned long long ulli, vull)
                            vl << ulli;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::LongLongInt:
                    case CuVariant::LongInt: {
                        std::vector< long long> vll;
                        v.toVector< long long>(vll);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vll.begin(), vll.end());
#else
                        QVariantList vl;
                        foreach(unsigned long long lli, vll)
                            vl << lli;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::UShort: {
                        std::vector<unsigned short> vus;
                        v.toVector<unsigned short>(vus);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        QVariantList vl (vus.begin(), vus.end());
#else
                        QVariantList vl;
                        foreach(unsigned short us, vus)
                            vl << us;
#endif
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::Boolean: {
                        std::vector<bool> vboo;
                        v.toVector<bool>(vboo);
                        QVariantList vl;
                        foreach(bool b, vboo)
                            vl.push_back(b);
                        converted = t->setProperty(qprop.toLatin1(), vl);
                    }break;
                    case CuVariant::String:{
                        QuStringList sl(v);
                        converted = t->setProperty(qprop.toLatin1(), sl);
                    }
                        break;

                    default:
                        converted = false;
                        break;
                    } break;
                case CuVariant::Matrix: {

                    }break;
                    default: // EndFormatTypes, FormatInvalid
                        break;
                    }
                }
            }
            if(!converted)
                perr("CuMagic.m_prop_set: failed to set value %s on any of properties {%s} on %s",
                     v.toString().c_str(), qstoc(props.join(",")), qstoc(t->objectName()));
            else {
                if(t->metaObject()->indexOfProperty("suffix") > -1 && !d->display_unit.isEmpty() &&
                        (t->metaObject()->indexOfProperty("displayUnitEnabled") < 0 || t->property("displayUnitEnabled").toBool() ) )
                    t->setProperty("suffix", " [" + d->display_unit + "]");
                else if(!d->display_unit.isEmpty() && t->metaObject()->property(t->metaObject()->indexOfProperty(con_p.toLatin1())).type() == QVariant::String)
                    t->setProperty(con_p.toLatin1(), t->property(con_p.toLatin1()).toString() + " [" + d->display_unit + "]");

            }
        }
    }
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
    // \[([\d,\-]+)\]
    QRegularExpression re("\\[([\\d,\\-]+)\\]");
    QRegularExpressionMatch m = re.match(src);
    QRegularExpression re2("(\\d+)\\s*\\-\\s*(\\d+)");
    bool ok = true;
    d->v_idxs.clear();
    if(m.capturedTexts().size() > 1) {
        QString s = m.capturedTexts().at(1);
        foreach(const QString &t, s.split(',') ) {
            if(!t.contains('-') && t.toInt(&ok) >= 0 && ok)
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

QVariant CuMagic::m_str_convert(const CuVariant &v, CuMagic::TargetDataType tdt) {
    int idx;
    QVariant qva;
    bool converted;
    d->v_idxs.size() > 0 ? idx = d->v_idxs[0] : idx = 0;
    QuStringList vi = v.toStringVector( d->format.toStdString().c_str(), &converted);
    if(converted && tdt == Scalar && idx < vi.size()) {
        qva = QVariant(vi[idx]);
    }
    else if(converted && ( tdt == Vector || tdt == List)  ) {
        QStringList out;
        if(d->v_idxs.isEmpty()) // the whole vector
            out = vi;
        else  { // pick desired indexes
            foreach(int i, d->v_idxs)
                if(vi.size() > i)
                    out << vi[i];
        }
        tdt == List ? qva = QVariant::fromValue(out) : qva = QVariant::fromValue(out.toVector());
    }
    return qva;
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
        if(da.containsKey("format")) {
            d->format = QuString(da, "format");
        }
        if(da.containsKey("display_unit"))
            d->display_unit = QuString(da, "display_unit");
        if(!d->format.isEmpty() && t->metaObject()->indexOfProperty("format") > -1)
            t->setProperty("format", d->format.toStdString().c_str());
    }
}

void CuMagic::m_err_msg_set(QObject *o, const QList<int> &idxs, const QString &prop, const QString &msg, bool err) {
    QWidget *w = qobject_cast<QWidget *>(o);
    if(w && (w->metaObject()->indexOfProperty("disable_on_error") > -1 || w->property("disable_on_error").toBool()) ) {
        w->setDisabled(err);
    }
    int i = 0;
    QString m(msg + " [");
    for(i = 0; i < idxs.size() - 1; i++) m += QString("%1, ").arg(idxs[i]);
    if(i < idxs.size()) m += QString("%1").arg(idxs[i]);
    m += "]";
    if(!prop.isEmpty()) m += " [ property: " + prop + "]";
    if(w) w->setToolTip(m);
    else if(err) perr("CuMagic: error: %s", qstoc(m));
}

QString CuMagic::m_idxs_to_string() const {
    QString s;
    int i = 0, j = 0;
    while(i < d->v_idxs.size()) {
        j = i;
        while(j + 1 < d->v_idxs.size() && d->v_idxs[j+1] - d->v_idxs[j] == 1)
            j++;
        if(j > i && j <= d->v_idxs.size()) {
            s += QString("%1-%2").arg(d->v_idxs[i]).arg(d->v_idxs[j]);
        }
        else
            s += QString("%1").arg(d->v_idxs[i]);
        if(j < d->v_idxs.size() - 1)
            s += ",";
        i = j + 1;
    }
    return s;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(cumbia-magic, CuMagic)
#endif // QT_VERSION < 0x050000
