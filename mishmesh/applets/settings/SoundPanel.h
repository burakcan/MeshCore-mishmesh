#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>

namespace mishmesh {

// Sound settings: Volume cycle + the two per-type default notification
// ringtones. Selecting a "* msgs" row pushes the shared SoundPickerApplet in
// global mode; Volume keeps its in-place cycle.
class SoundPanel : public SettingsPanel {
public:
  static const char* levelName(sound::VolumeLevel v);
  static sound::VolumeLevel nextLevel(sound::VolumeLevel v);

  const char* title() const override { return "Sound"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  // test seams
  int rowCountForTest() const { return _model.count(); }
  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  struct Model : ListModel {
    sound::SoundEngine* snd = nullptr;
    AppServices*        app = nullptr;
    int count() const override { return 3; }
    const char* label(int i) const override {
      if (i == 0) return "Volume";
      if (i == 1) return "Channel msgs";
      return "Direct msgs";
    }
    const char* value(int i) const override {
      if (i == 0) return levelName(snd ? snd->volume() : sound::VolumeLevel::Mute);
      bool channel = (i == 1);
      uint8_t e = app ? app->notifyTone(channel) : 0;
      return sound::notifyToneEncodedName(e, /*perChat*/false, sound::notifyTypeDefault(channel));
    }
  } _model;

  sound::SoundEngine* _snd  = nullptr;
  AppServices*        _app  = nullptr;
  AppletHost*         _host = nullptr;
  ListMenu            _list;
};

SoundPanel& soundSettings();   // shared singleton

}  // namespace mishmesh
