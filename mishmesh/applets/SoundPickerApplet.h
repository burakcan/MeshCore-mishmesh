#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// Shared notification-ringtone picker (radio list). Two modes:
//  - global:  set the default tone for a message type (channel vs direct);
//             rows are the tones then "Silent" (no "Default" - these ARE it).
//  - per-chat: override a single chat's tone; rows are "Default", the tones,
//             then "Silent".
// Select writes the choice (AppServices for global, MessagesService for
// per-chat) and plays a preview; Back bubbles -> host pops. Launch-only
// singleton, like the old ChatNotifyApplet / ContactPermissionsApplet.
class SoundPickerApplet : public Applet, public ListModel {
public:
  SoundPickerApplet();
  void setGlobal(bool channel, const char* title);
  void setChat(const ConvoKey& key, const char* title);

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // ListModel
  int count() const override;
  const char* label(int i) const override;
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

private:
  uint8_t encodedForRow(int row) const;   // row -> encoded byte
  int     rowForEncoded(uint8_t e) const;  // encoded byte -> row (for radio mark)
  uint8_t currentByte() const;             // stored selection for this target

  AppServices*     _app = nullptr;
  MessagesService* _svc = nullptr;
  sound::SoundEngine* _snd = nullptr;
  bool      _perChat = false;
  bool      _channel = false;
  ConvoKey  _key{};
  char      _title[44] = {0};
  ListMenu  _list;
  StatusBar _bar;
};

SoundPickerApplet& soundPickerApplet();

}  // namespace mishmesh
