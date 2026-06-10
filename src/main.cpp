#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "common/Log.h"

#include <QApplication>

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Pixora"));
    QApplication::setApplicationName(QStringLiteral("Pixora"));
    QApplication::setApplicationVersion(QStringLiteral(PIXORA_VERSION));
    app.setQuitOnLastWindowClosed(false);

    pixora::initLogging();
    spdlog::info("Pixora {} starting", PIXORA_VERSION);

    pixora::SingleInstanceGuard guard(QStringLiteral("pixora-single-instance"));
    if (!guard.tryAcquire()) {
        spdlog::info("another instance is already running, exiting");
        return 0;
    }
    QObject::connect(&guard, &pixora::SingleInstanceGuard::secondInstanceStarted, [] {
        spdlog::info("second instance attempted to start");
    });

    pixora::TrayService tray;
    tray.show();

    const int rc = QApplication::exec();
    spdlog::info("Pixora exiting with code {}", rc);
    return rc;
}
