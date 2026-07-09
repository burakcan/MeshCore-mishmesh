#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

static const int SYSSTATS_LINE_LEN  = 40;
static const int SYSSTATS_MAX_LINES = 9;

// Pure formatter: one display line per known metric into `out`, returning the
// count (<= maxLines). Unknown metrics render as "--"; RAM total and the two
// version lines are omitted entirely when unknown.
int formatSystemStats(const SystemStats& s, char out[][SYSSTATS_LINE_LEN], int maxLines);

// Device-health panel: a scrollable read-only stats block (rebuilt ~1s so free-heap
// stays live) with two factory-reset actions riding at the end of the same scroll as
// a footer (split by a 1px divider), so they scroll inline with the text rather than
// pinning to the viewport. Focus starts on the stats; a NavDown while the stats are
// scrolled to the bottom hands focus to the action rows, and NavUp on the first row
// hands it back. Selecting an action opens a confirm; confirming calls
// AppServices::factoryReset (which reboots) and never returns. Back bubbles to pop.
class SystemInfoPanel : public SettingsPanel {
public:
  const char* title() const override { return "System Info"; }
  void begin(AppletContext& ctx) override;
  void onShow() override { _built = false; _focus = Focus::Stats; }   // reset on (re)entry
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _confirming; }

  // test seams
  const char* lineForTest(int i) const { return _stats.lineForTest(i); }
  bool actionsFocusedForTest() const { return _focus == Focus::Actions; }
  int  selectedActionForTest() const { return _list.selected(); }

private:
  enum class Focus : uint8_t { Stats, Actions };
  void rebuild(uint32_t now, bool keepScroll);
  void openConfirm(int row);

  AppServices*    _app = nullptr;
  ScrollText      _stats;
  ListMenu        _list;
  StaticListModel _actions;
  ConfirmDialog   _confirm;
  Focus           _focus = Focus::Stats;
  uint32_t        _lastBuilt = 0;
  bool            _built = false;
  bool            _confirming = false;
  int             _pendingRow = -1;   // 0 = keep identity, 1 = full wipe
};

SystemInfoPanel& systemInfoSettings();   // shared singleton

}  // namespace mishmesh
