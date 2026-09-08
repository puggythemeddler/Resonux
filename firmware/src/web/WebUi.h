#pragma once
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class App;

class WebUi {
public:
  bool begin(App& app);
  void stop();
  void loop();

  const char* mode() const { return "ui"; }
  bool running() const { return _running; }

private:
  static void taskEntry(void* arg);

  void handleRoot();
  void handleStatic(const String& path);
  void sendStatus();
  void sendFrame();
  void sendConfig();
  void handleConfigPut();
  void handleReboot();
  void sendThemes();
  void handleThemesPut();
  void handleThemesDelete();
  void handleThemesSelect();
  void handleThemesReset();
  String mimeFor(const String& path);

  App* _app = nullptr;
  WebServer _server{80};
  TaskHandle_t _task = nullptr;
  bool _running = false;
  uint32_t _startMs = 0;
};