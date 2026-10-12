#pragma once
#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class AppletHost;

class BotSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Remote Bot"; }
  void begin(AppletContext& ctx) override;
  int renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  struct Model : ListModel {
    AppServices* app = nullptr;
    enum Row : int { Enabled, Commands, Location, Mode, Channel, Trigger, SenderName, SNR, RSSI, Hops, Path, Text, ROW_COUNT };
    enum Field : uint8_t { FieldSenderName = 1, FieldRSSI = 2, FieldPath = 4, FieldSNR = 16, FieldHops = 32 };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override {
      static const char* L[] = {
        "Bot enabled", "Commands", "Location", "Mode", "Channel", "Trigger",
        "Sender name", "SNR", "RSSI", "Hops", "Path", "Reply text"
      };
      return (i >= 0 && i < ROW_COUNT) ? L[i] : "";
    }
    bool isToggle(int i) const override {
      return i == Enabled || i == Commands || i == Location || fieldBit(i) != 0;
    }
    bool toggleState(int i) const override {
      if (!app) return false;
      if (i == Enabled) return app->botEnabled();
      if (i == Commands) return app->botCommandsEnabled();
      if (i == Location) return app->botLocationEnabled();
      uint8_t bit = fieldBit(i);
      if (bit) return (app->botReplyFields() & bit) != 0;
      return false;
    }
    const char* value(int i) const override {
      if (!app) return nullptr;
      if (i == Mode) {
        static const char* M[] = {"DM", "Channel", "DM + Channel"};
        int m = app->botMode(); return (m >= 0 && m <= 2) ? M[m] : "DM";
      }
      if (i == Channel) return app->botChannel();
      if (i == Trigger) return app->botTrigger();
      if (i == Text) return app->botReply()[0] ? app->botReply() : "test";
      return nullptr;
    }
    static uint8_t fieldBit(int i) {
      switch (i) {
        case SenderName: return FieldSenderName;
        case SNR: return FieldSNR;
        case RSSI: return FieldRSSI;
        case Hops: return FieldHops;
        case Path: return FieldPath;
        default: return 0;
      }
    }
  };

private:
  static void onChannelDone(void* ctx, const char* text);
  static void onTriggerDone(void* ctx, const char* text);
  static void onReplyDone(void* ctx, const char* text);
  Model _model;
  AppServices* _app = nullptr;
  AppletHost* _host = nullptr;
  ListMenu _list;
};

BotSettingsPanel& botSettings();

}  // namespace mishmesh
