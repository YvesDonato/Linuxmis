// Isolated compositor smoke client; no host connection or personal settings.
#include "streaming/mangonativesplit.h"
#include <QCoreApplication>
#include <QTextStream>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 5 || (args[1] != "native" && args[1] != "local")) return 2;
    const bool native = args[1] == "native";
    const int width = args[2].toInt(), height = args[3].toInt(), duration = args[4].toInt();
    if (width < 1 || height < 1 || width > 32767 || height > 32767 || duration < 1) return 2;
    SDL_SetHint("SDL_VIDEO_WAYLAND_WMCLASS", "com.linuxmis.linuxmis");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 3;
    auto window = SDL_CreateWindow(native ? MangoNativeSplit::WindowTitle : "Local Test",
                                   0, 0, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS);
    if (!window) return 3;
    std::unique_ptr<MangoNativeSplit> split;
    if (native) split.reset(new MangoNativeSplit(window, QSize(width, height)));
    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) return 3;
    SDL_SetRenderDrawColor(renderer, 24, 24, 24, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    if (split) split->begin();

    QElapsedTimer clock;
    clock.start();
    bool reported = false;
    int result = 0;
    while (clock.elapsed() < duration) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) goto cleanup;
            if (split && event.type == SDL_USEREVENT && event.user.code == MangoNativeSplit::RecheckEvent) {
                split->setSize(QSize(width, height));
                split->begin();
            }
        }
        if (split) {
            const auto error = split->poll();
            if (!error.isEmpty()) {
                QTextStream(stderr) << error << '\n';
                result = 1;
                goto cleanup;
            }
            if (!split->canPresentFrame(width, height)) {
                SDL_Delay(10);
                continue;
            }
        }
        if (!reported) {
            QTextStream(stdout) << "READY " << QCoreApplication::applicationPid() << ' '
                                << width << 'x' << height << '\n' << Qt::flush;
            reported = true;
        }
        // Alternating single-pixel columns expose compositor resampling.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (int x = 0; x < width; x += 2) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, x, 0, x, height - 1);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    if (!reported) result = 1;
cleanup:
    SDL_DestroyRenderer(renderer);
    split.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
