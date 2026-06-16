#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/sound/SoundEngine.h>

namespace mishmesh {

// Sound settings as a row list. v1: a single Volume row that cycles
// Mute->Low->Mid->High on Select, applied + persisted via AppServices (live-only
// fallback if no app seam), with a preview beep. Extensible to more rows later.
class SoundPanel : public SettingsPanel {
public:
  static const char* levelName(sound::VolumeLevel v);
  static sound::VolumeLevel nextLevel(sound::VolumeLevel v);

  const char* title() const override { return "Sound"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

private:
  struct Model : ListModel {
    sound::SoundEngine* snd = nullptr;
    int count() const override { return 1; }
    const char* label(int) const override { return "Volume"; }
    const char* value(int) const override {
      return levelName(snd ? snd->volume() : sound::VolumeLevel::Mute);
    }
  } _model;

  sound::SoundEngine* _snd  = nullptr;
  AppServices*        _app  = nullptr;
  AppletHost*         _host = nullptr;
  ListMenu            _list;
};

SoundPanel& soundSettings();   // shared singleton

}  // namespace mishmesh
