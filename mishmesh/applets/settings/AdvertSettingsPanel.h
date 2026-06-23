#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class AppletHost;

// Advert settings: editable device (advert) name + a "Share position" toggle
// controlling whether self-adverts include this node's location. Source of
// truth is AppServices (persisted by the adapter).
class AdvertSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Advert"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  // Test seams (mirror TimeSettingsPanel::rowCountForTest).
  int rowCountForTest() const { return _model.count(); }
  const char* labelForTest(int i) const { return _model.label(i); }
  const char* valueForTest(int i) const { return _model.value(i); }

private:
  class Model : public ListModel {
    AppServices* _app = nullptr;
  public:
    enum Row : int { DeviceName, SharePosition, ROW_COUNT };
    void bind(AppServices* app) { _app = app; }
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override {
      return i == DeviceName ? "Device name" : "Share position";
    }
    bool isToggle(int i) const override { return i == SharePosition; }
    bool toggleState(int i) const override {
      return i == SharePosition && _app && _app->shareLocationInAdvert();
    }
    const char* value(int i) const override {
      return (i == DeviceName && _app) ? _app->nodeName() : nullptr;
    }
  } _model;

  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  char _nameBuf[32];                       // keypad scratch; applied only if valid
  static void onNameDone(void* ctx, const char* text);   // keypad confirm
  ListMenu     _list;
};

AdvertSettingsPanel& advertSettings();   // shared singleton

}  // namespace mishmesh
