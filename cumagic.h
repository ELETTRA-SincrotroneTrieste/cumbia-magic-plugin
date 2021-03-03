#ifndef QUMULTIREADER_H
#define QUMULTIREADER_H

#include <QObject>
#include <QList>
#include <cumagicplugininterface.h>
#include <cudata.h>
#include <cudatalistener.h>


class CuMagicPluginPrivate;
class Cumbia;
class CumbiaPool;
class CuControlsReaderFactoryI;
class CuControlsFactoryPool;
class CuControlsReaderA;


class CuMagicPrivate
{
public:
    CuContext *context;
    CuVariant on_error_value;
    QList<int> v_idxs;
    QMap<QString, opropinfo> omap;
    QMap<QString, QString> propmap;
    QString t_prop;
    QString format, display_unit;
};

class CuMagic : public QObject, public CuMagicI, public CuDataListener {
    Q_OBJECT
public:

    enum TargetDataType { Scalar, Vector, List };

    CuMagic(QObject* target, CumbiaPool *cu_pool, const CuControlsFactoryPool &fpoo,
            const QString& source = QString(), const QString &property = QString());
    ~CuMagic();
    void setErrorValue(const CuVariant& v);

    QString &operator [](std::size_t idx);
    const QString& operator[](std::size_t idx) const;

    void map(size_t idx, const QString &onam);
    void map(size_t idx, QObject *obj, const QString &prop = QString());
    opropinfo& find(const QString& onam);

    void mapProperty(const QString& from, const QString& to);
    QString propMappedFrom(const char *to);
    QString propMappedTo(const char *from);

    // CuDataListener interface
public:
    void onUpdate(const CuData &data);

private slots:

signals:
    void newData(const CuData& da);

    // CuMagicI interface
public:
    void setSource(const QString &src);
    void unsetSource();
    QString source() const;
    QObject *get_target_object() const;
    void sendData(const CuData &da);
    CuContext *getContext() const;
    QString format() const;
    QString display_unit() const;

private:
    CuMagicPrivate *d;

    bool m_prop_set(QObject* t, const CuVariant& v, const QString& prop);
    bool m_v_str_split(const CuVariant& in, const QMap<QString, opropinfo> &opromap, QMap<QString, CuVariant> &out);
    QString m_get_idxs(const QString& src) const;

    template <typename T> bool m_v_split(const CuVariant& in, const QMap<QString, opropinfo>& opromap, QMap<QString, CuVariant> &out) {
        bool ok = true;
        out.clear();
        std::vector<T> dv;
        ok &= in.toVector<T>(dv);
        foreach(const opropinfo& opropi, opromap) {
            std::vector <double> subv;
            foreach(size_t i, opropi.idxs)
                if(dv.size() > i)
                    subv.push_back(dv[i]);
            out[opropi.obj->objectName()] = CuVariant(subv);
        }
        return ok;
    }

    template <typename T> QVariant m_convert(const CuVariant& v, TargetDataType tdt = Scalar) {
        size_t idx;

        QVariant qva;
        d->v_idxs.size() > 0 ? idx = d->v_idxs[0] : idx = 0;
        std::vector<T> vi; // convert to vector always
        bool converted = v.toVector<T>(vi) && vi.size() > idx;
        if(converted && tdt == Scalar) {
            qva = QVariant(vi[idx]);
        }
        else if(converted && tdt == Vector) {
            QVector<T> out;
            if(d->v_idxs.isEmpty()) // the whole vector
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                out = QVector<T>(vi.begin(), vi.end());
#else
                out = QVector<T>::fromStdVector(vi);
#endif
            else  { // pick desired indexes
                foreach( size_t i, d->v_idxs)
                    if(vi.size() > i)
                        out << vi[i];
            }
            qva = QVariant::fromValue(out);
        }
        else if(converted && tdt == List) {
            QList<T> out;
            if(d->v_idxs.isEmpty()) // the whole vector
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                out = QList<T>(vi.begin(), vi.end());
#else
                out = QVector<T>::fromStdVector(vi).toList();
#endif
            else  { // pick desired indexes
                foreach( size_t i, d->v_idxs)
                    if(vi.size() > i)
                        out << vi[i];
            }
            qva = QVariant::fromValue(out);
        }
        return qva;
    } // end template function m_convert

    void m_configure(const CuData& da);
    void m_err_msg_set(QObject* o, const QString& msg, bool err);
};

/** \mainpage This plugin allows magic
 *
 * \par Introduction
 * This plugin provides objects called CuMagic that can be *attached* to normal Qt widgets (or simple QObjects)
 * and display values read from cumbia on them. A Qt *property* determines how to show the data. A *custom property*
 * may be specified. Otherwise, *value*, *text* and other default ones are searched for.
 *
 * \section Use cases
 *
 * \subsection 1. CuMagic attached to one single object
 *
 * \li cumbia scalar data - scalar property: the scalar value is *set* on the property
 * \li cumbia vector data and no index mapping - scalar property: *the first element is set* on the property
 * \li cumbia vector data and one or more indexes specified - scalar property: *the first specified index is used*
 *
 * \li cumbia scalar data - vector property (QVariantList): the value is set on element 0 of the vector
 * \li cumbia spectrum data and no indexes specified: the whole vector is set on the property
 * \li cumbia spectrum data with indexes specified: the set of indexes determines which values are taken from the vector and
 *     *set* on the property.
 *
 * \subsection 2. CuMagic attached to a list of objects
 *
 * \li cumbia scalar data: can only be used in context number 1.
 * \li cumbia spectrum data: through index mapping, each element of the data array can be displayed in the specified object
 *
 * Please read CuMagicPluginInterface documentation and the <em>magicdemo</em> example
 * under the examples subfolder of the plugin directory.
 *
 * \code
 *
    QObject *magic_obj;
    CuMagicPluginInterface *magic_i = CuMagicPluginInterface* get_instance(cu_pool, fpool, &magic_obj);
    if(!magic_i) // initialize plugin once
            perr("CuMagicPluginInterface: failed to load plugin \"libcumbia-magic-plugin.so\"");
        else {
        QuLabel label = new QuLabel(this);
        magic_i->new_magic(label, "$1/double_scalar");
    }
}
 *
 * \endcode
 *
 *
 * \par Warning
 * Do not forget to call
 *
 * \code
 * m_multir->unsetSources
 * \endcode
 *
 * before the application exits. In fact, if the plugin is destroyed *after* Cumbia, the behavior is undefined.
 *
 *
 */
class CuMagicPlugin : public QObject, public CuMagicPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QGenericPluginFactoryInterface" FILE "cumbia-magic.json")
#endif // QT_VERSION >= 0x050000

    Q_INTERFACES(CuMagicPluginInterface)

public:

    CuMagicPlugin(QObject *parent = nullptr);
    virtual ~CuMagicPlugin();

    // CuMagicPluginInterface interface
public:
    CuMagicI *new_magic(QObject *target, const QString &source = QString(), const QString &property = QString()) const;
    void init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool);
    const QObject *get_qobject() const;

private:
    CuMagicPluginPrivate *d;
};

#endif // QUMULTIREADER_H
