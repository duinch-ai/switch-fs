#if defined(__SDL2__)
#include <SDL2/SDL_main.h>
#endif

#include <borealis.hpp>
#include "view/main_activity.hpp"
#include "view/home_tab.hpp"
#include "view/search_tab.hpp"
#include "view/romm_tab.hpp"
#include "view/library_tab.hpp"
#include "view/queue_tab.hpp"
#include "view/settings_tab.hpp"
#include "utils/http.hpp"

#include <unistd.h>

int main(int argc, char* argv[]) {
#if defined(__SWITCH__)
    FILE* log_file = fopen("/switch/SwitchFS/switchfs.log", "w");
    if (log_file) {
        setvbuf(log_file, nullptr, _IOLBF, 1024);
        brls::Logger::setLogOutput(log_file);
    } else {
        brls::Logger::setLogOutput(stdout);
    }
#else
    // Set stdout log output and debug level
    brls::Logger::setLogOutput(stdout);
#endif
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        brls::Logger::info("Current working directory: {}", cwd);
    }

    brls::Logger::info("Switch FS v0.1.0 starting on Desktop Simulator...");

    // Init Borealis
    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Compact icon-only sidebar
    brls::Application::getStyle().addMetric("brls/tab_frame/sidebar_width", 90.0f);
    brls::Application::getStyle().addMetric("brls/sidebar/padding_left", 12.0f);
    brls::Application::getStyle().addMetric("brls/sidebar/padding_right", 12.0f);
    brls::Application::getStyle().addMetric("brls/sidebar/item_font_size", 26.0f);

    if (getcwd(cwd, sizeof(cwd))) {
        brls::Logger::info("Current working directory after init: {}", cwd);
    }

    brls::Application::createWindow("Switch FS");

    if (auto* vg = brls::Application::getNVGContext()) {
        int regular_font = brls::Application::getFont(brls::FONT_REGULAR);
        int material_font = brls::Application::getFont(brls::FONT_MATERIAL_ICONS);
        if (regular_font != brls::FONT_INVALID && material_font != brls::FONT_INVALID) {
            nvgAddFallbackFontId(vg, regular_font, material_font);
        }
    }

    // Set global quit on ESC or controller + button
    brls::Application::setGlobalQuit(true);
    brls::Application::getWindowShouldCloseEvent()->subscribe([]() {
        switchfs::utils::HttpClient::setAbortAll(true);
    });
    brls::Application::getExitEvent()->subscribe([]() {
        switchfs::utils::HttpClient::setAbortAll(true);
    });

    // Register XML views
    brls::Application::registerXMLView("HomeTab", switchfs::view::HomeTab::create);
    brls::Application::registerXMLView("SearchTab", switchfs::view::SearchTab::create);
    brls::Application::registerXMLView("RommTab", switchfs::view::RommTab::create);
    brls::Application::registerXMLView("LibraryTab", switchfs::view::LibraryTab::create);
    brls::Application::registerXMLView("QueueTab", switchfs::view::QueueTab::create);
    brls::Application::registerXMLView("SettingsTab", switchfs::view::SettingsTab::create);

    // Push Main Activity
    brls::Application::pushActivity(new switchfs::view::MainActivity());

    // Main run loop
    while (brls::Application::mainLoop()) {
        // Run loop
    }

    switchfs::utils::HttpClient::setAbortAll(true);
#if defined(__SWITCH__)
    if (log_file) {
        brls::Logger::setLogOutput(stdout);
        fflush(log_file);
        fclose(log_file);
    }
#endif

    return EXIT_SUCCESS;
}
