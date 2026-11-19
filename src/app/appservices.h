#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariant>

#include <memory>

// Version and build information for the About dialog (values from CMake).
class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildInfo READ buildInfo CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString qtBuildVersion READ qtBuildVersion CONSTANT)
    Q_PROPERTY(QUrl licenseFolder READ licenseFolder CONSTANT)
    Q_PROPERTY(QUrl qtLicenseFile READ qtLicenseFile CONSTANT)
    Q_PROPERTY(QUrl appLicenseFile READ appLicenseFile CONSTANT)

public:
    using QObject::QObject;

    QString name() const;
    QString version() const;
    QString buildInfo() const;
    QString qtVersion() const;      // runtime (the DLLs actually loaded)
    QString qtBuildVersion() const; // compile time
    // Files shipped next to the executable (release package); empty if missing.
    QUrl licenseFolder() const;
    QUrl qtLicenseFile() const;
    QUrl appLicenseFile() const;

private:
    static QUrl shipped(const QString &relative);
};

// Persistent preferences (QSettings). Disabled in the scripted demo and the
// layout check so captures are reproducible and never touch the user's
// settings: value() then returns the default and setValue() does nothing.
class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled CONSTANT)

public:
    explicit AppSettings(bool enabled, QObject *parent = nullptr);

    bool isEnabled() const { return m_settings != nullptr; }
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void remove(const QString &key);
    Q_INVOKABLE void sync();

private:
    std::unique_ptr<QSettings> m_settings;
};
