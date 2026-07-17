#pragma once

#include <stddef.h>
#include <stdint.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;
class AppletHost;
struct ContactsService;   // mishmesh/core/ContactsService.h
// [mishmesh]
namespace sound { class SoundEngine; }
class AirtimeHistory;     // mishmesh/core/AirtimeHistory.h
// [/mishmesh]

// Snapshot of device health for the System stats screen. Plain integers so the
// framework stays free of companion/platform types. 0 (or nullptr) means
// "unknown/unreported" - the applet renders those as "--".
struct SystemStats {
  uint32_t    heapFreeBytes    = 0;
  uint32_t    heapTotalBytes   = 0;   // 0 = unknown
  uint32_t    heapMinFreeBytes = 0;   // low watermark since boot; 0 = unknown
  uint16_t    contactsUsed     = 0;
  uint16_t    contactsMax      = 0;
  uint32_t    storageUsedKb    = 0;
  uint32_t    storageTotalKb   = 0;   // 0 = unknown
  uint32_t    uptimeSecs       = 0;
  uint16_t    batteryMv        = 0;
  const char* meshcoreVersion  = nullptr;   // upstream MeshCore release
  const char* mishmeshVersion  = nullptr;   // mishmesh UI version
};

// [mishmesh]
// Radio airtime / duty-cycle usage for the Airtime applet. Totals are lifetime
// (since boot) in ms; the budget fields describe the duty-cycle token bucket the
// Dispatcher enforces. `history` (may be null) is the loop-fed per-minute ring
// the graph reads. 0 fields render as "--".
struct AirtimeStats {
  uint32_t txTotalMs    = 0;   // cumulative TX airtime since boot
  uint32_t rxTotalMs    = 0;   // cumulative RX airtime since boot
  uint32_t txBudgetMs   = 0;   // remaining TX budget (token bucket)
  uint32_t txBudgetMax  = 0;   // budget capacity = window * dutyCycle
  uint32_t windowMs     = 0;   // duty-cycle window (default 1h)
  uint32_t sentFlood    = 0;
  uint32_t sentDirect   = 0;
  uint32_t recvFlood    = 0;
  uint32_t recvDirect   = 0;
  const AirtimeHistory* history = nullptr;
};
// [/mishmesh]

// LoRa radio configuration surfaced to the on-device UI. Units match NodePrefs:
// freq in MHz, bw in kHz.
struct RadioConfig {
  float   freqMhz;      // e.g. 910.525
  float   bwKhz;        // e.g. 62.5
  uint8_t sf;           // 5..12
  uint8_t cr;           // 5..8
  int8_t  txPowerDbm;   // -9..txPowerMax()
  bool    repeater;     // off-grid repeat mode (client_repeat); applied with the rest
};

// Live app/device state applets read. Implemented by the adapter and queried on
// demand, since battery/time/connection change over an applet's lifetime. Keeps
// the framework free of companion-specific types (NodePrefs, RTCClock, board).
struct AppServices {
  virtual ~AppServices() {}
  virtual const char* nodeName() const = 0;
  virtual uint16_t    batteryMillivolts() const = 0;
  virtual uint32_t    epochSeconds() const = 0;   // UNIX seconds; 0 if unknown
  // Fill device-health stats; return false if unavailable. Default: no stats.
  virtual bool systemStats(SystemStats& out) const { (void)out; return false; }
  // [mishmesh]
  // BLE/companion link state. Defaults keep the framework companion-agnostic;
  // the adapter (UITask) overrides these on BLE builds.
  virtual bool     bleSupported() const { return false; }
  virtual bool     bleEnabled()   const { return false; }   // radio enabled/advertising
  virtual bool     bleConnected() const { return false; }   // a client is paired
  virtual uint32_t blePin()       const { return 0; }       // 0 = hide PIN
  virtual void     setBleEnabled(bool) {}
  // Broadcast a self-advert now. false = zero hop (neighbours only, no relay),
  // true = flood routed (propagates multi-hop across the mesh). Returns false if
  // it could not be sent (e.g. packet pool exhausted). Default: no-op.
  virtual bool sendAdvert(bool flood) { (void)flood; return false; }
  // Whether self-adverts include this node's location. Persisted by the adapter.
  // Defaults keep the framework companion-agnostic (off, not settable).
  virtual bool shareLocationInAdvert() const { return false; }
  virtual void setShareLocationInAdvert(bool) {}
  // Path-hash size this node stamps on floods it originates: 0/1/2 = 1/2/3 bytes
  // per hop (NodePrefs.path_hash_mode). Higher sizes disambiguate nodes but are
  // dropped by repeaters on firmware < 1.14 and cut the max flood hop count.
  // Defaults keep the framework companion-agnostic (mode 0, not settable).
  virtual uint8_t pathHashMode() const { return 0; }
  virtual void    setPathHashMode(uint8_t mode) { (void)mode; }
  // Set + persist the global sound volume (0=Mute,1=Low,2=Mid,3=High). The adapter
  // applies it to the engine and writes it to NodePrefs. Default no-op.
  virtual void setSoundVolume(uint8_t level) { (void)level; }
  // Global default notification ringtone per message type (channel vs direct),
  // encoded as in mishmesh/sound/Sounds.h. The adapter persists it to NodePrefs.
  // Defaults keep the framework companion-agnostic (Default/unset, not settable).
  virtual uint8_t notifyTone(bool channel) const { (void)channel; return 0; }
  virtual void setNotifyTone(bool channel, uint8_t encoded) { (void)channel; (void)encoded; }
  // On-device radio configuration. Defaults keep the framework companion-agnostic;
  // the adapter (UITask) overrides these to read/write NodePrefs and apply live.
  virtual bool   radioConfig(RadioConfig& out) const { (void)out; return false; }
  virtual int8_t txPowerMax() const { return 22; }        // board ceiling; floor is -9
  virtual void   setRadioConfig(const RadioConfig&) {}    // persist + apply live (no reboot)
  // Time settings. Defaults keep the framework companion-agnostic (UTC/24h/auto).
  virtual int16_t tzOffsetMinutes() const { return 0; }
  virtual void    setTzOffsetMinutes(int16_t) {}
  virtual bool    timeFormat12h() const { return false; }
  virtual void    setTimeFormat12h(bool) {}
  virtual bool    autoTimeSync() const { return true; }
  virtual void    setAutoTimeSync(bool) {}
  virtual void    setEpochSeconds(uint32_t) {}    // manual clock set (writes RTC)
  // Timezone as a WorldClock city index (source of truth): -1 = custom/fixed
  // (use the raw offset). tzOffsetMinutes() above resolves this DST-aware.
  virtual int  tzCityIndex() const { return -1; }
  virtual void setTzCity(int cityIndex) { (void)cityIndex; }
  virtual uint8_t dateFormat() const { return 0; }   // mishmesh::DateFormat (0=DMY)
  virtual void    setDateFormat(uint8_t) {}
  // GPS power. Defaults keep the framework companion-agnostic (unsupported);
  // the adapter overrides these against the SensorManager "gps" setting.
  virtual bool gpsSupported() const { return false; }
  virtual bool gpsEnabled()   const { return false; }
  virtual void setGpsEnabled(bool) {}
  // Fix state, from the LocationProvider. Both report "no" while GPS is off
  // so a stale last fix never shows. Satellites: 0 = none/unknown.
  virtual bool gpsHasFix() const { return false; }
  virtual int  gpsSatellites() const { return 0; }
  // Screen auto-off timeout, as an index into the mishmesh SCREEN_SLEEP options
  // (mishmesh/core/ScreenSleep.h). Default index 1 = 30s. The adapter persists
  // it to NodePrefs and applies it live to the AppletHost.
  virtual uint8_t screenSleepIndex() const { return 1; }
  virtual void    setScreenSleepIndex(uint8_t) {}
  virtual bool    screenBrightnessSupported() const { return false; }
  virtual uint8_t screenBrightnessIndex() const { return 2; }
  virtual void    setScreenBrightnessIndex(uint8_t) {}
  // Apply a brightness index to the panel live without persisting it, so the
  // stepper can preview each level; Cancel re-applies the saved index.
  virtual void    previewScreenBrightnessIndex(uint8_t) {}
  // Set + persist the device (advert) name. Rejects invalid/empty names
  // (isValidNodeName). Returns true if applied. Save only - no advert is sent.
  // Defaults keep the framework companion-agnostic (not settable).
  virtual bool    setNodeName(const char*) { return false; }
  // Off-grid repeat (client_repeat). repeaterMode() drives the home indicator.
  // savedRepeatFreq persists the pre-repeat frequency so disabling restores it.
  // Defaults keep the framework agnostic.
  virtual bool  repeaterMode() const { return false; }
  virtual float savedRepeatFreq() const { return 0.0f; }
  virtual void  setSavedRepeatFreq(float) {}
  // Firmware-permitted off-grid repeat frequencies, for manual selection.
  virtual int   repeatFreqCount() const { return 0; }
  virtual float repeatFreqMhz(int i) const { (void)i; return 0.0f; }
  // Radio airtime / duty-cycle usage for the Airtime applet. Returns false if
  // unavailable. Default keeps the framework companion-agnostic.
  virtual bool airtimeStats(AirtimeStats& out) const { (void)out; return false; }
  // Wipe all persisted state (settings, contacts, channels, messages) and reboot.
  // keepIdentity preserves the node keypair; false yields a fresh key on boot. Does
  // not return. Default no-op keeps the framework companion-agnostic.
  virtual void factoryReset(bool keepIdentity) { (void)keepIdentity; }
  // Onboarding wizard support. selfPublicKeyHex writes the node's public-key prefix
  // as hex (bytes = how many key bytes to render; out must hold 2*bytes+1). Default
  // writes an empty string. markOnboardingComplete persists "onboarding done".
  virtual void selfPublicKeyHex(char* out, size_t cap, int bytes) const {
    (void)bytes; if (out && cap) out[0] = 0;
  }
  virtual void markOnboardingComplete() {}
  // Dev tool: re-trigger the first-boot onboarding wizard (sets onboarding_state to
  // IN_PROGRESS and reboots). Default no-op. Only wired/surfaced in dev builds.
  virtual void resetOnboarding() {}
  // [/mishmesh]
};

// Handle through which an applet reaches host/app services. Grows as features land.
struct AppletContext {
  AppletHost*      host = nullptr;
  AppServices*     app = nullptr;
  ContactsService* contacts = nullptr;   // [new] contacts/mesh seam
  // [mishmesh]
  struct MessagesService* messages = nullptr;
  const InputState* inputState = nullptr;   // host-owned; updated once per loop
  AppletStorage* storage = nullptr;   // generic key->blob persistence (may be null)
  sound::SoundEngine* sound = nullptr;   // buzzer/sound subsystem (may be null)
  // Live held-button snapshot for real-time applets. Safe before the host wires
  // it up: returns an all-released state.
  const InputState& input() const {
    static const InputState kEmpty;
    return inputState ? *inputState : kEmpty;
  }
  // [/mishmesh]
};

class Applet {
  const char* _name;
protected:
  explicit Applet(const char* name) : _name(name) {}
public:
  virtual ~Applet() {}

  const char* name() const { return _name; }

  virtual void onStart(AppletContext&) {}   // pushed onto the stack
  virtual void onForeground() {}            // became the top
  virtual void onBackground() {}            // covered by another applet
  virtual void onStop() {}                  // popped off

  // Draw, returning the number of milliseconds until the next wanted render.
  virtual int onRender(Canvas& c) = 0;

  // Return true if the event was consumed; otherwise it bubbles up to the host.
  virtual bool onInput(InputEvent) { return false; }

  // Whether the Back button should auto-repeat while held during this applet.
  // Default false: most screens must NOT repeat Back, or one hold would pop
  // through several of them. A text editor overrides this to delete on hold.
  virtual bool wantsBackRepeat() const { return false; }

  // Opt into real-time "game mode": while this applet is foreground the host
  // calls onRender every main-loop pass (no dirty/delay gating) and the returned
  // delay is ignored. Default false: normal applets stay dirty/event-driven.
  virtual bool wantsExclusive() const { return false; }

  // Overlays (notification banner, popups) return true: each frame the host
  // renders the applet beneath first, so the overlay paints only its own card
  // (plus a scrim) and the covered screen stays visible around it. One level
  // only - an overlay under another overlay is not re-rendered.
  virtual bool isOverlay() const { return false; }

  // The panel just blanked (auto-off). Fires on the foreground applet regardless
  // of how long it stays asleep - shed transient UI here (close a drawer/popup).
  virtual void onSleep() {}

  // Stay on this screen when the user wakes the device after a long sleep instead
  // of resetting to home. Default false: most screens return home. May be dynamic
  // (e.g. only while a session is in progress).
  virtual bool keepOnWake() const { return false; }

  // Suppress auto-off entirely while this applet is foreground and returns true
  // (e.g. a running stopwatch the user is watching). May be dynamic. Costs battery
  // and, on OLED, risks burn-in - return true only while it genuinely matters.
  virtual bool blocksSleep() const { return false; }
};

}  // namespace mishmesh
