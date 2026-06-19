#pragma once

#include <stdint.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;
class AppletHost;
struct ContactsService;   // mishmesh/core/ContactsService.h
// [mishmesh]
namespace sound { class SoundEngine; }
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
  const char* firmwareVersion  = nullptr;
};

// LoRa radio configuration surfaced to the on-device UI. Units match NodePrefs:
// freq in MHz, bw in kHz.
struct RadioConfig {
  float   freqMhz;      // e.g. 910.525
  float   bwKhz;        // e.g. 62.5
  uint8_t sf;           // 5..12
  uint8_t cr;           // 5..8
  int8_t  txPowerDbm;   // -9..txPowerMax()
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
};

}  // namespace mishmesh
