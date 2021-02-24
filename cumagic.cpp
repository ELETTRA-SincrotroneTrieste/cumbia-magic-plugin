#include "qumultireader.h"
#include <cucontext.h>
#include <cucontrolsreader_abs.h>
#include <cudata.h>
#include <QTimer>
#include <QMap>
#include <QtDebug>

class CuMagicPrivate
{
public:
    QMap<QString, CuControlsReaderA* > readersMap;
    int period, mode;
    CuContext *context;
    QTimer *timer;
    QMap<int, CuData> databuf;
    QMap<int, QString> idx_src_map;
};

CuMagic::CuMagic(QObject *parent) :
    QObject(parent)
{
    d = new CuMagicPrivate;
    d->period = 1000;
    d->mode = SequentialReads; // sequential reading
    d->timer = NULL;
    d->context = NULL;
}

CuMagic::~CuMagic()
{
    if(d->context)
        delete d->context;
    delete d;
}

void CuMagic::init(Cumbia *cumbia, const CuControlsReaderFactoryI &r_fac, int mode) {
    d->context = new CuContext(cumbia, r_fac);
    d->mode = mode;
    if(d->mode >= SequentialManual) d->period = -1;
}

void CuMagic::init(CumbiaPool *cumbia_pool, const CuControlsFactoryPool &fpool, int mode) {
    d->context = new CuContext(cumbia_pool, fpool);
    d->mode = mode;
    if(d->mode >= SequentialManual) d->period = -1;
}

void CuMagic::sendData(const QString &s, const CuData &da) {
    CuControlsReaderA *r = d->readersMap[s];
    if(r) r->sendData(da);
}

void CuMagic::sendData(int index, const CuData &da) {
    const QString& src = d->idx_src_map[index];
    if(!src.isEmpty())
        sendData(src, da);
}

void CuMagic::setSources(const QStringList &srcs)
{
    unsetSources();
    for(int i = 0; i < srcs.size(); i++)
        insertSource(srcs[i], i);
}

void CuMagic::unsetSources()
{
    d->context->disposeReader(); // empty arg: dispose all
    d->idx_src_map.clear();
    d->readersMap.clear();
}

/** \brief inserts src at index position i in the list. i must be >= 0
 *
 * @see setSources
 */
void CuMagic::insertSource(const QString &src, int i) {
    if(i < 0)
        perr("CuMagic.insertSource: i must be >= 0");
    else {
        cuprintf("\e[1;35mCuMagic.insertSource %s --> %d\e[0m\n", qstoc(src), i);
        CuData options;
        if(d->mode >= SequentialManual) {
            options["manual"] = true;
        }
        else if(d->mode == SequentialReads && d->period > 0)  {
            // readings in the same thread
            options["refresh_mode"] = 1; // CuTReader::PolledRefresh
            options["period"] = d->period;
        }
        if(d->mode >= SequentialReads) // manual or seq
            options["thread_token"] = QString("multi_reader_%1").arg(objectName()).toStdString();
        d->context->setOptions(options);
        printf("CuMagic.insertSource: options passed: %s\n", datos(options));
    }
    CuControlsReaderA* r = d->context->add_reader(src.toStdString(), this);
    if(r) {
        r->setSource(src); // then use r->source, not src
        d->readersMap.insert(r->source(), r);
        d->idx_src_map.insert(i, r->source());
    }
    if(d->idx_src_map.size() == 1 && d->mode == SequentialReads)
        m_timerSetup();

}

void CuMagic::removeSource(const QString &src) {
    if(d->context)
        d->context->disposeReader(src.toStdString());
    d->idx_src_map.remove(d->idx_src_map.key(src));
    d->readersMap.remove(src);
}

/** \brief returns a reference to this object, so that it can be used as a QObject
 *         to benefit from signal/slot connections.
 *
 */
const QObject *CuMagic::get_qobject() const {
    return this;
}

QStringList CuMagic::sources() const {
    return d->idx_src_map.values();
}

/*!
 * \brief Returns the period used by the multi reader if in *sequential* mode
 * \return The period in milliseconds used by the multi reader timer in *sequential* mode
 *
 * \note A negative period requires a manual update through the startRead *slot*.
 */
int CuMagic::period() const {
    return d->period;
}

/*!
 * \brief Change the period, in milliseconds
 * \param ms the new period in milliseconds
 *
 * In sequential mode, a negative period requires a manual call to startRead (*slot*) to
 * trigger an update cycle.
 * If not in sequential mode, a negative period is ignored.
 */
void CuMagic::setPeriod(int ms) {
    d->period = ms;
    if(d->mode == SequentialReads && ms > 0) {
        CuData per("period", ms);
        per["refresh_mode"] = 1;
        foreach(CuControlsReaderA *r, d->context->readers())
            r->sendData(per);
    }
}

void CuMagic::setSequential(bool seq) {
    seq ? d->mode = SequentialReads : d->mode = ConcurrentReads;
}

bool CuMagic::sequential() const {
    return d->mode >= SequentialReads;
}

void CuMagic::startRead() {
    if(d->idx_src_map.size() > 0) {
        // first: returns a reference to the first value in the map, that is the value mapped to the smallest key.
        // This function assumes that the map is not empty.
        const QString& src0 = d->idx_src_map.first();
        d->readersMap[src0]->sendData(CuData("read", ""));
        cuprintf("CuMagic.startRead: started cycle with read command for %s...\n", qstoc(d->idx_src_map.first()));
    }
}

void CuMagic::m_timerSetup() {
    printf("\e[1;31mCuMagic.mTimerSetup period: %d\e[0m\n", d->period);
    if(!d->timer) {
        d->timer = new QTimer(this);
        connect(d->timer, SIGNAL(timeout()), this, SLOT(startRead()));
        d->timer->setSingleShot(true);
        if(d->period > 0)
            d->timer->setInterval(d->period);
    }
}

// find the index that matches src, discarding args
int CuMagic::m_matchNoArgs(const QString &src) const {
    printf("\e[1;31mCuMagic.m_matchNoArgs\e[0m\n");
    foreach(int k, d->idx_src_map.keys()) {
        const QString& s = d->idx_src_map[k];
        printf("\e[1;36mCuMagic::m_matchNoArgs comparing %s with %s\e[0m\n", qstoc(s), qstoc(src));
        if(s.section('(', 0, 0) == src.section('(', 0, 0))
            return k;
    }
    return -1;
}

void CuMagic::onUpdate(const CuData &data) {
    QString from = QString::fromStdString( data["src"].toString());
    int pos;
    const QList<QString> &srcs = d->idx_src_map.values();
    srcs.contains(from) ? pos = d->idx_src_map.key(from) : pos = m_matchNoArgs(from);
    if(pos < 0)
        printf("\e[1;31mCuMagic::onUpdate idx_src_map DOES NOT CONTAIN \"%s\"\e[0m\n\n", qstoc(from));
    emit onNewData(data);
    if((d->mode >= SequentialReads) && pos >= 0) {
        d->databuf[pos] = data; // update or new
        const QList<int> &dkeys = d->databuf.keys();
        const QList<int> &idxli = d->idx_src_map.keys();
        if(dkeys == idxli) { // databuf complete
            emit onSeqReadComplete(d->databuf.values()); // Returns all the values in the map, in ascending order of their keys
            d->databuf.clear();
        }
    }
}

CuMagicPluginInterface *CuMagic::getMultiSequentialReader(QObject *parent, bool manual_refresh) {
    CuMagic *r = nullptr;
    if(!d->context)
        perr("CuMagic.getMultiSequentialReader: call CuMagic.init before getMultiSequentialReader");
    else {
        r = new CuMagic(parent);
        r->init(d->context->cumbiaPool(), d->context->getControlsFactoryPool(), manual_refresh ? SequentialManual : SequentialReads);
    }
    return r;
}

CuMagicPluginInterface *CuMagic::getMultiConcurrentReader(QObject *parent) {
    CuMagic *r = nullptr;
    if(!d->context)
        perr("CuMagic.getMultiSequentialReader: call CuMagic.init before getMultiSequentialReader");
    else {
        r = new CuMagic(parent);
        r->init(d->context->cumbiaPool(), d->context->getControlsFactoryPool(), ConcurrentReads);
    }
    return r;
}

CuContext *CuMagic::getContext() const {
    return d->context;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(cumbia-magic, CuMagic)
#endif // QT_VERSION < 0x050000
