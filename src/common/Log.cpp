#include "common/Log.h"

#include <QDir>
#include <QStandardPaths>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <memory>
#include <vector>

namespace pixora {

void initLogging() {
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs";
    QDir().mkpath(logDir);
    const std::string logFile = QDir(logDir).filePath("pixora.log").toStdString();

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile));
#ifdef _WIN32
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

    auto logger = std::make_shared<spdlog::logger>("pixora", sinks.begin(), sinks.end());
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [pid %P] [%l] %v");
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::info);
}

} // namespace pixora
