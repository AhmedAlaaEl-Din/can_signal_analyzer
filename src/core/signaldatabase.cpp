#include "signaldatabase.h"

#include <QFileInfo>

namespace can {

SignalDatabase::SignalDatabase(QObject *parent)
    : QObject(parent)
{}

void SignalDatabase::setDatabase(std::shared_ptr<const Database> db, bool demo, int warnings)
{
    m_db = std::move(db);
    m_isDemo = demo;
    m_warningCount = warnings;
    emit changed();
}

bool SignalDatabase::loadFile(const QUrl &fileUrl)
{
    return loadPath(fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString());
}

bool SignalDatabase::loadPath(const QString &path)
{
    ParseResult r = DbcParser::parseFile(path);
    const QString fileName = QFileInfo(path).fileName();
    if (!r.ok()) {
        constexpr int kMaxShown = 6;
        m_errorTitle = r.errors.size() == 1
                           ? QStringLiteral("“%1” could not be loaded").arg(fileName)
                           : QStringLiteral("“%1” could not be loaded (%2 problems)").arg(fileName).arg(r.errors.size());
        m_errorLines.clear();
        for (std::size_t i = 0; i < r.errors.size() && i < kMaxShown; ++i)
            m_errorLines << r.errors[i].toString();
        if (r.errors.size() > kMaxShown)
            m_errorLines << QStringLiteral("… and %1 more").arg(r.errors.size() - kMaxShown);
        emit errorChanged();
        return false;
    }
    dismissError();
    const int warnings = static_cast<int>(r.warnings.size());
    m_path = QFileInfo(path).absoluteFilePath();
    setDatabase(std::make_shared<const Database>(std::move(*r.database)), false, warnings);
    return true;
}

void SignalDatabase::loadDemo()
{
    dismissError();
    m_path.clear();
    setDatabase(demoDatabase(), true, 0);
}

void SignalDatabase::unload()
{
    if (!m_db)
        return;
    m_path.clear();
    setDatabase(nullptr, false, 0);
}

void SignalDatabase::dismissError()
{
    if (m_errorTitle.isEmpty() && m_errorLines.isEmpty())
        return;
    m_errorTitle.clear();
    m_errorLines.clear();
    emit errorChanged();
}

} // namespace can
