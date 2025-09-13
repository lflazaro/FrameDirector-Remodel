#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QFontDatabase>

#include "MainWindow.h"

class FrameDirectorApplication : public QApplication
{

public:
    FrameDirectorApplication(int& argc, char** argv)
        : QApplication(argc, argv)
    {
        setApplicationName("FrameDirector");
        setApplicationVersion("1.0.0");
        setApplicationDisplayName("FrameDirector");
        setOrganizationName("FrameDirector Team");
        setOrganizationDomain("framedirector.com");

        // Set application icon
        setWindowIcon(QIcon(":/icons/framedirector.png"));

        setupApplication();
    }

private:
    void setupApplication()
    {
        // Setup application directories
        setupDirectories();

        // Load custom fonts
        loadFonts();

        // Setup application style
        setupStyle();

        // Setup internationalization
        setupTranslations();
    }

    void setupDirectories()
    {
        // Create application data directories
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dataDir;

        dataDir.mkpath(dataPath);
        dataDir.mkpath(dataPath + "/projects");
        dataDir.mkpath(dataPath + "/exports");
        dataDir.mkpath(dataPath + "/templates");
        dataDir.mkpath(dataPath + "/cache");

        // Set working directory
        QDir::setCurrent(dataPath);
    }

    void loadFonts()
    {
        // Load custom fonts for icons and UI
        QFontDatabase::addApplicationFont(":/fonts/framedirector-icons.ttf");
        QFontDatabase::addApplicationFont(":/fonts/roboto-regular.ttf");
        QFontDatabase::addApplicationFont(":/fonts/roboto-bold.ttf");
    }

    void setupStyle()
    {
        // Set dark theme style
        setStyle(QStyleFactory::create("Fusion"));

        // Load and apply custom stylesheet
        QFile styleFile(":/styles/dark-theme.qss");
        if (styleFile.open(QFile::ReadOnly)) {
            QString styleSheet = QLatin1String(styleFile.readAll());
            setStyleSheet(styleSheet);
        }
    }

    void setupTranslations()
    {
        QTranslator* translator = new QTranslator(this);
        QString locale = QLocale::system().name();

        if (translator->load(QString("framedirector_%1").arg(locale), ":/translations")) {
            installTranslator(translator);
        }
    }
};

int main(int argc, char* argv[])
{
    // Create application
    FrameDirectorApplication app(argc, argv);

    // Show splash screen
    QPixmap splashPixmap(":/icons/splash.png");
    QSplashScreen splash(splashPixmap);
    splash.show();
    app.processEvents();

    // Initialize application components
    splash.showMessage("Loading FrameDirector...", Qt::AlignBottom | Qt::AlignCenter, Qt::darkGray);
    app.processEvents();

    // Create and show main window
    MainWindow window;

    splash.showMessage("Ready", Qt::AlignBottom | Qt::AlignCenter, Qt::darkGray);
    app.processEvents();

    // Close splash and show main window
    QTimer::singleShot(1500, &splash, &QWidget::close);
    QTimer::singleShot(1500, &window, &QWidget::show);

    return app.exec();
}