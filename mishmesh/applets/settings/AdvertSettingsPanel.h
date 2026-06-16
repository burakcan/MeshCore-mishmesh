#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// "Share position" toggle controlling whether self-adverts include this node's
// location. Source of truth is AppServices (persisted by the adapter).
class AdvertSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Advert"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

private:
  class Model : public ListModel {
    AppServices* _app = nullptr;
  public:
    void bind(AppServices* app) { _app = app; }
    int count() const override { return 1; }
    const char* label(int) const override { return "Share position"; }
    bool isToggle(int) const override { return true; }
    bool toggleState(int) const override { return _app && _app->shareLocationInAdvert(); }
  } _model;
  AppServices* _app = nullptr;
  ListMenu     _list;
};

AdvertSettingsPanel& advertSettings();   // shared singleton

}  // namespace mishmesh
