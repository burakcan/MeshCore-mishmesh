#pragma once
#include <cstdint> // For uint8_t, uint32_t
#include <helpers/ConfigSerializer.h>

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

// autoadd_config bitmask. Bit 0: overwrite oldest non-favourite when full;
// bits 1-4: which contact types to auto-add when manual_add_contacts = 0x01.
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - ADV_TYPE_CHAT
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - ADV_TYPE_REPEATER
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - ADV_TYPE_ROOM
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - ADV_TYPE_SENSOR

class NodePrefs : public ConfigSerializer {  // persisted to file
public:
  float airtime_factor = 0;
  char node_name[32];
  double node_lat = 0, node_lon = 0;
  float freq = 0;
  uint8_t sf = 0;
  uint8_t cr = 0;
  uint8_t multi_acks = 0;
  uint8_t manual_add_contacts = 0;
  float bw = 0;
  int8_t tx_power_dbm = 0;
  uint8_t telemetry_mode_base = 0;
  uint8_t telemetry_mode_loc = 0;
  uint8_t telemetry_mode_env = 0;
  float rx_delay_base = 0;
  uint32_t ble_pin = 0;
  uint8_t  advert_loc_policy = 0;
  uint8_t  buzzer_quiet = 0;
  uint8_t  vibe_quiet = 0;
  uint8_t  gps_enabled = 0;      // GPS enabled flag (0=disabled, 1=enabled)
  uint32_t gps_interval = 0;     // GPS read interval in seconds
  uint8_t autoadd_config = 0;    // bitmask for auto-add contacts config
  uint8_t rx_boosted_gain = 0; // SX126x RX boosted gain mode (0=power saving, 1=boosted)
  uint8_t radio_fem_rxgain = 0; // external LoRa FEM RX gain (LNA)
  uint8_t radio_fem_txgain = 0; // external LoRa FEM TX gain (low by default)
  uint8_t _client_repeat = 0;  // DEPRECATED -> use repeat.disable_fwd
  uint8_t path_hash_mode = 0;    // which path mode to use when sending
  uint8_t autoadd_max_hops = 0;  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
  char default_scope_name[31];
  uint8_t default_scope_key[16];
  // [mishmesh] mishmesh prefs. Serialized under the "mm" object; the binary
  // /new_prefs migration path in DataStore still reads them from the struct tail.
  uint8_t sound_volume = 2;      // mishmesh::sound::VolumeLevel (0=Mute..3=High)
  uint8_t sound_mute_mask = 0x0F; // SoundEngine category enable bits (1=enabled)
  uint8_t notify_tone_ch = 0;    // channel-msg default ringtone (encoded; 0 = firmware default)
  uint8_t notify_tone_dm = 0;    // direct-msg  default ringtone (encoded; 0 = firmware default)
  // Time settings. 0-defaults preserve pre-time-settings behavior (UTC/24h/auto).
  int8_t  tz_quarter_hours = 0;  // UTC offset in 15-min units (-48..+56 = -12:00..+14:00)
  uint8_t time_fmt_12h = 0;      // 0 = 24-hour, 1 = 12-hour
  uint8_t manual_time_set = 0;   // 0 = automatic (GPS/phone allowed), 1 = manual (suppress)
  uint8_t date_format = 0;       // mishmesh::DateFormat: 0=DMY, 1=MDY, 2=YMD
  uint8_t screen_sleep = 0;      // mishmesh screen auto-off; 0=unset(=30s), else option index+1 (see ScreenSleep.h)
  float   repeat_saved_freq = 0; // pre-repeat frequency (MHz) to restore when off-grid repeat is disabled; 0 = none
  uint8_t ble_enabled = 1;       // serial/BLE link enabled across reboots; 0 = off, else on (default on)
  uint8_t contacts_full_notify = 1; // mishmesh "contacts full" alert; 0 = off, else on (default on)
  int8_t  tz_city_index = -1;    // WorldClock city index (DST source of truth); -1 = custom/fixed (use tz_quarter_hours)
  uint8_t onboarding_state = 0;  // 0=not started, 1=in progress, 2=done (first-boot wizard)
  uint8_t screen_brightness = 0; // mishmesh screen brightness; 0=unset(=High), else level index+1 (Low/Med/High)
  uint8_t sleep_screen = 0;      // e-ink sleep face; index into mishmesh/core/SleepScreen.h (0 = Screen off)
  uint8_t sleep_rotation = 0;    // orientation for a face that reads either way; 0 = Auto (follow the screen)
  // Written as 1 by savePrefs; still 0 after a load means the file has no "mm"
  // object, i.e. it was written by stock MeshCore and this is a first mishmesh boot.
  uint8_t mm_ver = 0;
  // [/mishmesh]


private:
  class RadioPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("freq", _parent->freq);
      def("bw", _parent->bw);
      def("sf", _parent->sf);
      def("cr", _parent->cr);
      //def("cad", _parent->cad_enabled);
      //def("int_thr", _parent->interference_threshold);
      def("rxgain", _parent->rx_boosted_gain);
    #if 0
      // NOTE: these cannot be set (yet) so don't load/save until we can.
      //       also, fem_rxgain WAS mapped to wrong JSON property previously
      def("fem_rxgain", _parent->radio_fem_rxgain);
      def("fem_txgain", _parent->radio_fem_txgain);
    #endif
      def("tx", _parent->tx_power_dbm);
      def("af", _parent->airtime_factor);
      def("rxdelay", _parent->rx_delay_base);
      //def("f_txdelay", _parent->tx_delay_factor);   currently hard-coded
      //def("d_txdelay", _parent->direct_tx_delay_factor);  currently hard-coded
      //def("agc_int", _parent->agc_reset_interval);
      def("hash_mode", _parent->path_hash_mode);
      def("multi_ack", _parent->multi_acks);
    }
  public:
    RadioPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  RadioPrefs radio;

  class GPSPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("en", _parent->gps_enabled); // boolean
      def("int", _parent->gps_interval);   // interval in seconds
      def("adv_loc", _parent->advert_loc_policy);
    }
  public:
    GPSPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  GPSPrefs gps;

  class RepeatPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
  public:
    uint8_t disable_fwd = 1;
  protected:
    void structure() override {
      def("disable", disable_fwd);
      //def("f_max", flood_max);
      //def("f_max_uns", flood_max_unscoped);
      //def("f_max_adv", flood_max_advert);
      //def("loop", loop_detect);
    }
  };
  RepeatPrefs repeat;

  // [mishmesh]
  class MishmeshPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("v", _parent->mm_ver);
      def("vol", _parent->sound_volume);
      def("mute", _parent->sound_mute_mask);
      def("tone_ch", _parent->notify_tone_ch);
      def("tone_dm", _parent->notify_tone_dm);
      def("tz_q", _parent->tz_quarter_hours);
      def("tz_city", _parent->tz_city_index);
      def("t_ampm", _parent->time_fmt_12h);   // no digits: ConfigSerializer keys are [A-Za-z_] only
      def("t_manual", _parent->manual_time_set);
      def("datefmt", _parent->date_format);
      def("sleep", _parent->screen_sleep);
      def("bright", _parent->screen_brightness);
      def("slpscr", _parent->sleep_screen);
      def("slprot", _parent->sleep_rotation);
      def("rpt_freq", _parent->repeat_saved_freq);
      def("ble_en", _parent->ble_enabled);
      def("cfull_notif", _parent->contacts_full_notify);
      def("onboard", _parent->onboarding_state);
    }
  public:
    MishmeshPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  MishmeshPrefs mm;
  // [/mishmesh]

  class CompanionPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("auto_max", _parent->autoadd_max_hops);  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
      def("defs_nm", _parent->default_scope_name, sizeof(_parent->default_scope_name));
      def("defs_key", (void *) _parent->default_scope_key, sizeof(_parent->default_scope_key));
      def("pin", _parent->ble_pin);
      def("buzz_q", _parent->buzzer_quiet);
      def("vibe_q", _parent->vibe_quiet);
      def("auto_add", _parent->autoadd_config);    // bitmask for auto-add contacts config
      def("man_add", _parent->manual_add_contacts);
      def("tel_base", _parent->telemetry_mode_base);
      def("tel_loc", _parent->telemetry_mode_loc);
      def("tel_env", _parent->telemetry_mode_env);
    }
  public:
    CompanionPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  CompanionPrefs companion;

protected:
  void structure() override {
    def("name", node_name, sizeof(node_name));
    //def("adv_int", advert_interval);
    //def("f_adv_int", flood_advert_interval);
    def("lat", node_lat);
    def("lon", node_lon);
    def("radio", radio);
    def("gps", gps);
    def("repeat", repeat);
    def("comp", companion);
    def("mm", mm);   // [mishmesh]
  }
public:
  NodePrefs() : radio(this), gps(this), mm(this), companion(this) {
    node_name[0] = 0;
    default_scope_name[0] = 0;
    memset(default_scope_key, 0, sizeof(default_scope_key));
  }
  // new accessor methods
  bool isRepeatEn() const { return repeat.disable_fwd == 0; }
  void setRepeatEn(bool en) { repeat.disable_fwd = en ? 0 : 1; }
};
