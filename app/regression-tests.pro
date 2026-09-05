include(app.pro)
TARGET = linuxmis-regression-tests
QT += testlib
SOURCES -= main.cpp
SOURCES -= streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.cpp
SOURCES += tests/regressions.cpp

# Fault injection stays in the Linux test binary, not in production code.
WRAPPED_SYMBOLS = SDL_CreateThread SDL_GetNumVideoDisplays SDL_GetNumDisplayModes SDL_GetWindowWMInfo SDL_GL_MakeCurrent \
    _ZN11StreamUtils20getNativeDesktopModeEiP15SDL_DisplayModeP8SDL_Rect \
    _ZN11StreamUtils21getDisplayRefreshRateEP10SDL_Window \
    _ZN15ComputerManager12getComputersEv _ZN15ComputerManager12startPollingEv \
    _ZN15ComputerManager16stopPollingAsyncEv _ZN15ComputerManager18addNewHostManuallyE7QString \
    _ZN15ComputerManager14quitRunningAppEP10NvComputer _ZN6NvHTTP10getAppListEv \
    _ZN13CompatFetcher5startEv _ZN15SdlInputHandler19getUnmappedGamepadsEv
for(symbol, WRAPPED_SYMBOLS): QMAKE_LFLAGS += -Wl,--wrap=$$symbol
