#include "app/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {

/*! Load the translation whose locale best matches \a localeName.
 *  Returns true if a translation was installed. */
bool installTranslation(QApplication &app, QTranslator &translator, const QString &localeName)
{
    if (localeName.isEmpty())
        return false;

    // Try the exact locale first (e.g. "pt_BR"), then the base language ("pt").
    QStringList candidates;
    candidates << localeName;
    const int underscore = localeName.indexOf(QLatin1Char('_'));
    if (underscore > 0)
        candidates << localeName.left(underscore);

    for (const QString &candidate : candidates) {
        if (translator.load(QStringLiteral(":/i18n/papiro_%1.qm").arg(candidate))) {
            app.installTranslator(&translator);
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Papiro"));
    QCoreApplication::setApplicationName(QStringLiteral("Papiro"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PAPIRO_VERSION));

    // Language selection: explicit setting wins, otherwise follow the system.
    QSettings settings;
    QString language = settings.value(QStringLiteral("ui/language")).toString();
    if (language.isEmpty())
        language = QLocale::system().name();

    QTranslator translator;
    installTranslation(application, translator, language);

    papiro::MainWindow window;
    window.show();

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() > 1)
        window.openFile(arguments.at(1));

    return application.exec();
}
