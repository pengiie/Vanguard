/*
 * Include Logger.h first to ensure that the LoggerRegistry is initialized first so it is destroyed last.
 */
#include "Logger.h"
#include "Application.h"

int main() {
    auto* app = new vanguard::Application();
    app->run();
    delete app;
    return 0;
}
