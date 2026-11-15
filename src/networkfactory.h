#pragma once

#include <QQmlNetworkAccessManagerFactory>

// Gives QML's image loader a persistent disk cache so artwork isn't refetched.
class CachingNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
};
