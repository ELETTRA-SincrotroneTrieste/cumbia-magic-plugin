#ifndef QUMULTIREADER_H
#define QUMULTIREADER_H

#include <QObject>
#include <QList>
#include <qumultireaderplugininterface.h>
#include <cudata.h>
#include <cudatalistener.h>

class CuMagicPrivate;
class Cumbia;
class CumbiaPool;
class CuControlsReaderFactoryI;
class CuControlsFactoryPool;
class CuControlsReaderA;

/** \mainpage This plugin allows parallel and sequential reading from multiple sources
 *
 * Please read CuMagicPluginInterface documentation and the <em>multireader</em> example
 * under the examples subfolder of the plugin directory.
 *
 * \code
 *
void Multireader::m_loadMultiReaderPlugin() {
    QObject *plugin_qob;
    CuMagicPluginInterface *multi_r;
    if(!multi_r) { // initialize plugin once
        multi_r = pl.get<CuMagicPluginInterface>("libcumbia-magic-plugin.so", &plugin_qob);
        if(!multi_r)
            perr("Multireader: failed to load plugin \"libcumbia-magic-plugin.so\"");
        else {
            // configure multi reader
            // cu_t is a reference to CumbiaTango
            // cu_tango_r_fac is a CuTReaderFactory
            // CuTReader::Manual is the CuTReader::RefreshMode enum value that identifies
            // manual refresh mode for the Tango engine. It tells the CuMagic to
            // perform a sequential read of the sources, one after another, and emit the
            // onSeqReadComplete when each cycle is over.
            // A value of -1 instead of CuTReader::Manual would configure the CuMagic to
            // read the n sources concurrently. In this case, no onSeqReadComplete is emitted.
            //
            m_multir->init(cu_t, cu_tango_r_fac,  CuTReader::Manual);
            // get multi reader as qobject in order to connect signals to slots
            connect(m_multir->get_qobject(), SIGNAL(onNewData(const CuData&)), this, SLOT(newData(const CuData&)));
            connect(m_multir->get_qobject(), SIGNAL(onSeqReadComplete(const QList<CuData >&)), this, SLOT(seqReadComplete(const QList<CuData >&)));
            // set the sources
            m_multir->insertSource("src1", 0);
            m_multir->insertSource("src2", 1);

            // need another (sequential) multi reader
            CuMagic *r2 = m_multir->getMultiSequentialReader(parent);
            // connect(r2, SIGNAL(onSeqReadComplete(const QList<CuData >&)) ...
            // need another (sequential and manually refreshed) reader
            CuMagic *manual_r3 = m_multir->getMultiSequentialReader(parent, true);
        }
    }
}
 *
 * \endcode
 *
 * Another version of the *init* method accepts a pointer to CumbiaPool and a reference to CuControlsFactoryPool.
 *
 * \note
 * If the application needs only one multi reader, CuMagicPluginInterface and the object returned by get_qobject
 * for signal/slot connections can be used directly.
 * The object is *shared* across the entire application.
 * If you need more than one multi reader in your application, use getMultiSequentialReader or  getMultiConcurrentReader
 * to obtain new instances. Call them with a parent for automatic destruction (r2 and manual_r3 in the example above)
 *
 * \par Warning
 * Do not forget to call
 *
 * \code
 * m_multir->unsetSources
 * \endcode
 *
 * before the application exits. In fact, if the plugin is destroyed *after* Cumbia, the behavior is undefined.
 */
class CuMagic : public QObject, public CuMagicPluginInterface, public CuDataListener
{
    Q_OBJECT
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QGenericPluginFactoryInterface" FILE "cumbia-magic.json")
#endif // QT_VERSION >= 0x050000

    Q_INTERFACES(CuMagicPluginInterface)

public:
    CuMagic(QObject *parent = 0);

    virtual ~CuMagic();

    // CuMagicPluginInterface interface
public:

    void init(Cumbia *cumbia, const CuControlsReaderFactoryI &r_fac, int mode = CuMagicPluginInterface::SequentialReads);
    void init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool, int mode = CuMagicPluginInterface::SequentialReads);
    void sendData(const QString& s, const CuData& da);
    void sendData(int index, const CuData& da);
    void setSources(const QStringList &srcs);
    void unsetSources();
    void insertSource(const QString &src, int i);
    void removeSource(const QString &src);
    const QObject *get_qobject() const;
    QStringList sources() const;

    int period() const;
    void setPeriod(int ms);
    void setSequential(bool seq);
    bool sequential() const;

    CuMagicPluginInterface *getMultiSequentialReader(QObject *parent, bool manual_refresh);
    CuMagicPluginInterface *getMultiConcurrentReader(QObject *parent);
    CuContext *getContext() const;

public slots:
    void startRead();

signals:
    void onNewData(const CuData& da);
    void onSeqReadComplete(const QList<CuData >& data);

private:
    CuMagicPrivate *d;

    void m_timerSetup();
    int m_matchNoArgs(const QString& src) const;

    // CuDataListener interface
public:
    void onUpdate(const CuData &data);

};

#endif // QUMULTIREADER_H
