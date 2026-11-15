#include "networkfactory.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QStandardPaths>

QNetworkAccessManager *CachingNetworkFactory::create(QObject *parent)
{
    auto *nam = new QNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/artwork"));
    cache->setMaximumCacheSize(512LL * 1024 * 1024);
    nam->setCache(cache);
    return nam;
}
