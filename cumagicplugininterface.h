#ifndef QUMULTIREADERPLUGININTERFACE_H
#define QUMULTIREADERPLUGININTERFACE_H

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
class CuMagic;
class CuContext;


/** \brief Interface for a plugin implementing reader that connects to multiple quantities.
 *
 * \ingroup plugins
 *
 * \li Readings can be sequential or parallel (see the init method). Sequential readings must notify when a reading is performed
 *     and when a complete read cycle is over, providing the read data through two Qt signals: onNewData(const CuData& da) and
 *     onSeqReadComplete(const QList<CuData >& data). Parallel readings must notify only when a new result is available, emitting
 *     the onNewData signal.
 *
 * \li A multi reader must be initialised with the init method, that determines what is the engine used to read and whether the reading
 *     is sequential or parallel by means of the read_mode parameter. If the mode is negative, the reading is parallel and the
 *     refresh mode is determined by the controls factory, as usual. If the mode is non negative <em>it must correspond
 *     to the <strong>manual refresh mode</strong> of the underlying engine.</em>
 *     For example, CuTReader::Manual must be specified for the Tango control system engine in order to let the multi reader
 *     use an internal poller to read the attributes sequentially.
 *
 */
class CuMagicPluginInterface
{
public:

    enum Mode { ConcurrentReads = 0, SequentialReads, SequentialManual };

    virtual ~CuMagicPluginInterface() { }

    /** \brief Initialise the multi reader with the desired engine and the read mode.
     *
     * @param cumbia a reference to the cumbia  implementation
     * @param r_fac the engine reader factory
     * @param mode see the documentation of the CumbiaPool/CuControlsFactoryPool init method
     *
     * \par Note
     * To support *multi engine* in cumbia, please use the other version of init.
     */
    virtual void init(Cumbia *cumbia, const CuControlsReaderFactoryI &r_fac, int mode) = 0;

    /** \brief Initialise the multi reader with mixed engine mode and the read mode.
     *
     * @param cumbia a reference to the CumbiaPool engine chooser
     * @param r_fac the CuControlsFactoryPool factory chooser
     * @param mode configuration to apply to the reader:
     *     \li SequentialManual: manually triggered refresh, sequential readings in the same thread, notification
     *         on single reads and on reading complete
     *     \li ConcurrentReads: readings are performed concurrently and results notified asynchronously
     *     \li SequentialReads: readings are performed sequentially and results are notified on cycle complete
     *         as well as on each operation. Results are delivered in the order specified in insertSource, but
     *         the actual readings are not guaranteed to be performed in such order.
     *         Readings take place in the same thread.
     */
    virtual void init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool, int mode) = 0;

    /** \brief set the sources to read from.
     *
     * \note Calling this method replaces the existing sources with the new ones
     *
     * @see addSource
     */
    virtual void setSources(const QStringList& srcs) = 0;

    /** \brief Remove the readers.
     *
     */
    virtual void unsetSources()= 0;

    /** \brief adds a source to the multi reader.
     *
     * Inserts src at index position i in the list. If i <= 0, src is prepended to the list. If i >= size(),
     * src is appended to the list.
     *
     * @see setSources
     */
    virtual void insertSource(const QString& src, int i = -1) = 0;

    /** \brief removes the specified source from the reader
     *
     */
    virtual void removeSource(const QString& src) = 0;

    /** \brief returns the list of the configured sources
     */
    virtual QStringList sources() const = 0;

    /** \brief returns the polling period of the reader.
     *
     * @return the polling period, in milliseconds, of the multi reader.
     */
    virtual int period() const = 0;

    /** \brief Change the reading period, if the reading mode is sequential.
     *
     * \note If the reading mode is parallel, the request is forwarded to every single reader.
     */
    virtual void setPeriod(int ms) = 0;

    /** \brief To provide the necessary signals aforementioned, the implementation must derive from
     *         Qt QObject. This method returns the subclass as a QObject, so that the client can
     *         connect to the multi reader signals.
     *
     * @return The object implementing CuMagicPluginInterface as a QObject.
     */
    virtual const QObject* get_qobject() const = 0;

    /*!
     * \brief send data to the reader specified by the source
     * \param s the source to send data to
     * \param da the data
     * \par Example
     * This method can be used to change the input args of *s*, if s is a command.
     */
    virtual void sendData(const QString& s, const CuData& da) = 0;

    /*!
     * \brief send data to the reader specified by the index
     * \param index the index of the source to send data to
     * \param da the data
     * \par Example
     * This method can be used to change the input args of *s*, if s is a command.
     *
     * This is a convenience method equivalent to QString counterpart
     */
    virtual void sendData(int index, const CuData& da) = 0;

    /*!
     * \brief get an instance of a sequential multi reader
     * \param parent the parent object
     * \param manual_refresh if true, the reader will not automatically update
     * \return an instance of CuMagic
     */
    virtual CuMagicPluginInterface *getMultiSequentialReader(QObject *parent, bool manual_refresh = false) = 0;

    /*!
     * \brief get an instance of a multi reader where readings are performed concurrently
     * \param parent the parent object
     * \return an instance of CuMagic
     */
    virtual CuMagicPluginInterface *getMultiConcurrentReader(QObject *parent) = 0;

    /*!
     * \brief get the context used by the multireader
     * \return a pointer to the CuContext in use, which is nullptr if init has not been called yet
     */
    virtual CuContext *getContext() const = 0;


    // convenience method to get the plugin instance

    /*!
     * \brief CuMagicPluginInterface::get_instance returns a (singleton, see note) instance of the plugin
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
                                                      CuMagicPluginInterface::Mode mode,
                                                      QObject *plugin_qob){
        CuMagicPluginInterface *i;
        CuPluginLoader plo;
        i = plo.get<CuMagicPluginInterface>(file_name, &plugin_qob);
        if(!i)
            perr("CuMagicPluginInterface::get_instance: failed to load plugin \"%s\"", file_name);
        else
            i->init(cu_poo, fpoo, mode);
        return i;
    }

    static constexpr const char file_name[32] = "libcumbia-magic-plugin.so";
};

#define CuMagicPluginInterface_iid "eu.elettra.qutils.CuMagicPluginInterface"

Q_DECLARE_INTERFACE(CuMagicPluginInterface, CuMagicPluginInterface_iid)

#endif // QUMULTIREADERPLUGININTERFACE_H
