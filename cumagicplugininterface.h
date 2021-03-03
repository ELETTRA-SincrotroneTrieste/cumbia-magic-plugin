#ifndef CUMAGICPLUGININTERFACE_H
#define CUMAGICPLUGININTERFACE_H

#include <QObject>
#include <cupluginloader.h>
#include <cumacros.h>

class Cumbia;
class CumbiaPool;
class CuControlsReaderFactoryI;
class CuControlsFactoryPool;
class QString;
class QStringList;
class CuData;
class CuContext;
class CuVariant;

class opropinfo {
public:
    opropinfo(QObject *o, const QString& p, int idx): obj(o), prop(p), idxs{idx} { }
    opropinfo() : obj(nullptr) {}
    virtual ~opropinfo() {}
    QObject *obj;
    QString prop;
    QList<int> idxs;
};

class CuMagicI {
public:
    virtual ~CuMagicI() {}

    /** \brief set the source to read from.
     *
     * \note Calling this method replaces the existing source with the new ones
     */
    virtual void setSource(const QString& src) = 0;

    /** \brief Remove the reader.
     *
     */
    virtual void unsetSource()= 0;

    /** \brief returns the configured source
     */
    virtual QString source() const = 0;

    /*!
     * \brief get_object returns the QObject used in CuMagicPluginInterface::getMagic
     * \return the Qt object (QObject, QWidget, ...) used in CuMagicPluginInterface::getMagic as the target
     *         where read values are displayed
     */
    virtual QObject *get_target_object() const = 0;

    /*!
     * \brief send data to the reader
     * \param da the data
     */
    virtual void sendData(const CuData& da) = 0;

    /*!
     * \brief get the context used by the multireader
     * \return a pointer to the CuContext in use, which is nullptr if init has not been called yet
     */
    virtual CuContext *getContext() const = 0;

    /*!
     * \brief setErrorValue in case of read error, display this value
     * \param v the value to show in case of error
     */
    virtual void setErrorValue(const CuVariant& v) = 0;

    virtual void map(size_t idx, const QString& onam) = 0;
    virtual void map(size_t idx, QObject *obj, const QString &prop = QString()) = 0;

    /*!
     * \brief mapProperty instruct CuMagic to use property *to* instead of *from*
     * \param from a property name known to the plugin (e.g. *value, text, min, max *)
     * \param to a property name known to the object (e.g. *min --> yLowerBound, max --> yUpperBound, text --> html* )
     *
     * This method can be used to tell CuMagic to operate on the property *to* instead of *from*.
     * *from* is one of the properties *known by CuMagic*, such as *min, max, value, text, ...*
     */
    virtual void mapProperty(const QString& from, const QString& to) = 0;

    /*!
     * \brief propMappedTo given the *from* property, returns the property *from is mapped to*
     * \param from the property name known to the CuMagic, f.e. min, max, value, text, format, display_unit
     * \return the property name to which *from* has been associated by mapProperty
     */
    virtual QString propMappedTo(const char *from) = 0;

    /*!
     * \brief propMappedFrom given the *to* property, returns the property *from* associated to *to* by mapProperty
     * \param to the name of the property known to the object, associated by the mapProperty method
     * \return the property name known to CuMagic that was mapped to *to* by mapProperty
     */
    virtual QString propMappedFrom(const char *to) = 0;

    /*!
     * \brief find find the opropinfo associated to a given object name
     * \param onam object name to search
     * \return a reference to an opropinfo, if found, an empty opropinfo otherwise
     */
    virtual  opropinfo& find(const QString& onam) = 0;

    /*!
     * \brief get the suggested format to use when displaying the number, if available
     *        from the configuration stage.
     * \return a string defining the format for numbers, e.g. "%.2f", "%d" or an empty
     *         string if the *format* key in the configuration data is not defined.
     */
    virtual QString format() const = 0;

    /*!
     * \brief get the measurement unit, if available from the configuration stage
     * \return the measurement unit, or an empty string if the *display_unit*
     *         key is not found within the configuration data.
     */
    virtual QString display_unit() const = 0;
};


/*!
 * \mainpage Add some cumbia magic to simple Qt widgets
 *
 * \section Introduction
 *
 * \code
   #include <cumagicplugininterface.h>
   // load magic plugin
   QObject *magic_plo;
   CuMagicPluginInterface *plugin_i = CuMagicPluginInterface::get_instance(cumbia_pool, m_ctrl_factory_pool, magic_plo);
   if(!plugin_i)
        perr("MyMagicApp: failed to load plugin \"%s\"", CuMagicPluginInterface::file_name);
    else { // magic here
    }
 * \endcode
 *
 * The cumbia-magic-plugin allows to read data through the cumbia engine and display it on a generic widget
 * (or even a simple QObject) through its properties. For example, a scalar number can be displayed as text
 * on either a QLineEdit or QLabel, as a number in a QDoubleSpinBox or a QLCDNumber or set on a progress bar.
 *
 * \code
   CuMagicI *ma0 = plugin_i->new_magic(ui->lcdNumber, "$1/double_scalar");
   CuMagicI *ma1 = plugin_i->new_magic(ui->progressBar, "$1/short_scalar");
 * \endcode
 *
 * Likewise, an element of an array can be picked and used as a scalar:
 *
 * \code
 * CuMagicI *ma2 = plugin_i->new_magic(ui->x0_2, "$1/double_spectrum[0]");
 * \endcode
 *
 * A subset of elements of a vectorial quantity can be defined and displayed on a spectrum oriented widget,
 * such as a plot:
 *
 * \code
    // Let a custom plot be defined as this:
    class MyDisplayVector : public QuPlotBase {
        Q_OBJECT
        Q_PROPERTY(QList<double> myData READ myData WRITE setMyData) // property to access myData
    public:
        MyDisplayVector(QWidget *parent); // constructor
        void setMyData(const QList<double> &y);  // update plot curve
        QList<double> myData() const; // return the curve y values
    // ...
    };

   // ui->plot is a MyDisplayVector, derived from QuPlotBase
   // pick values at indexes 1 to 10, then other three times the element 10, elements 15,16 and finally 20 to 26
   // use the "myData" property on the plot for updates
   CuMagicI *ma3 = plugin_i->new_magic(ui->plot, "$1/double_spectrum_ro[1-10,10,10,10,15,16,20-26]", "myData");
   // if min and max configuration properties are available, use them to set yLowerBound and yUpperBound on the
   // plot, respectively
   ma3->mapProperty("min", "yLowerBound");
   ma3->mapProperty("max", "yUpperBound");
 * \endcode
 *
 * \subsection Default properties
 *
 * Elements of a vectorial quantity can be displayed each on dedicated widgets:
 *
 * \code
    CuMagicI *ma_v_spinb = plugin_i->new_magic(this, "$1/double_spectrum");
    // "x0", ... "x4" are the object names of five QDoubleSpinBox
    ma_v_spinb->map(0, "x0"); // element 0 of double_spectrum is displayed on QDoubleSpinBox "x0"
    ma_v_spinb->map(1, "x1");
    ma_v_spinb->map(2, "x2");
    ma_v_spinb->map(3, "x3");
    ma_v_spinb->map(4, "x4");
 * \endcode
 */
class CuMagicPluginInterface
{
public:

    virtual ~CuMagicPluginInterface() { }

    /** \brief Initialise the multi reader with mixed engine mode and the read mode.
     *
     * @param cumbia a reference to the CumbiaPool engine chooser
     * @param r_fac the CuControlsFactoryPool factory chooser
     *
     */
    virtual void init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool) = 0;

    /** \brief To provide the necessary signals aforementioned, the implementation must derive from
     *         Qt QObject. This method returns the subclass as a QObject, so that the client can
     *         connect to the multi reader signals.
     *
     * @return The object implementing CuMagicI as a QObject.
     */
    virtual const QObject* get_qobject() const = 0;

    /*!
     * \brief get_magic returns a new CuMagicI object with target the given QObject and source
     * \param parent the object used as target to display the read values
     * \param source the source of the readings
     * \return a *new* instance of an object implementing CuMagicI interface
     */
    virtual CuMagicI *new_magic(QObject* target, const QString& source = QString(), const QString& property = QString()) const = 0;

    // convenience method to get the plugin instance

    /*!
     * \brief CuMagicPluginInterface::get_instance returns a new instance of the plugin
     * \param cu_poo pointer to a previously allocated CumbiaPool
     * \param fpoo const reference to CuControlsFactoryPool
     * \param mode one of CuMagicPluginInterface::Mode modes
     * \param plugin_qob a pointer to a QObject, will contain this plugin *as QObject* (for signals and slots)
     * \return an instance of the plugin or nullptr in case of failure.
     *
     * \note Repeated calls will return the same plugin instance (by Qt plugin nature)
     *       Use either getMultiSequentialReader or getMultiConcurrentReader to get new instances of multi readers instead
     */
    static CuMagicPluginInterface* get_instance(CumbiaPool *cu_poo,
                                                      const CuControlsFactoryPool& fpoo,
                                                      QObject *plugin_qob){
        CuMagicPluginInterface *i;
        CuPluginLoader plo;
        i = plo.get<CuMagicPluginInterface>(file_name, &plugin_qob);
        if(!i)
            perr("CuMagicPluginInterface::get_instance: failed to load plugin \"%s\"", file_name);
        else
            i->init(cu_poo, fpoo);
        return i;
    }

    static constexpr const char file_name[32] = "libcumbia-magic-plugin.so";
};

#define CuMagicPluginInterface_iid "eu.elettra.qutils.CuMagicPluginInterface"

Q_DECLARE_INTERFACE(CuMagicPluginInterface, CuMagicPluginInterface_iid)

#endif // CUMAGICPLUGININTERFACE_H
