/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <filesystem>
#include "app.h"
#include "gfx_window.h"
#include "app_internal.h"

AppMain::AppMain() {
    app_internal_ = std::make_unique<AppInternal>();
}

AppMain::~AppMain() {
    // make unique_ptr work
}

int AppMain::Run() {
    return app_internal_->Run();
}