/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_APP_MAIN_H
#define INC_3DGS_ADVGI_APP_MAIN_H

#include <filesystem>
#include <memory>

class AppInternal;

class AppMain {
public:
    AppMain();
    ~AppMain();
    int Run ();
protected:
    std::unique_ptr<AppInternal> app_internal_;
};

#endif //INC_3DGS_ADVGI_APP_MAIN_H
