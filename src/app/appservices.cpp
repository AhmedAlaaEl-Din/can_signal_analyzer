#include "appservices.h"

#include "version.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

QString AppInfo::name() const
{
    return QStringLiteral(CAN_ANALYZER_PRODUCT);
}

QString AppInfo::version() const
{
    return QStringLiteral(CAN_ANALYZER_VERSION);
}

QString AppInfo::buildInfo() const
{
    return QStringLiteral("%1 build · %2 · %3").arg(QStringLiteral(CAN_ANALYZER_BUILD_TYPE),
                                                    QStringLiteral(CAN_ANALYZER_COMPILER),
                                                    QSysInfo::buildCpuArchitecture());
}

QString AppInfo::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppInfo::qtBuildVersion() const
{
    return QStringLiteral(QT_VERSION_STR);
}

QUrl AppInfo::shipped(const QString &relative)
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(relative);
    return QFileInfo::exists(path) ? QUrl::fromLocalFile(path) : QUrl();
}

QUrl AppInfo::licenseFolder() const
{
    return shipped(QStringLiteral("THIRD_PARTY"));
}

QUrl AppInfo::qtLicenseFile() const
{
    return shipped(QStringLiteral("THIRD_PARTY/Qt/LGPL-3.0.txt"));
}

QUrl AppInfo::appLicenseFile() const
{
    return shipped(QStringLiteral("LICENSE.txt"));
}

AppSettings::AppSettings(bool enabled, QObject *parent)
    : QObject(parent)
{
    if (enabled)
        m_settings = std::make_unique<QSettings>(); // organisation / application names from main()
}

QVariant AppSettings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings ? m_settings->value(key, defaultValue) : defaultValue;
}

void AppSettings::setValue(const QString &key, const QVariant &value)
{
    if (m_settings)
        m_settings->setValue(key, value);
}

void AppSettings::remove(const QString &key)
{
    if (m_settings)
        m_settings->remove(key);
}

void AppSettings::sync()
{
    if (m_settings)
        m_settings->sync();
}
