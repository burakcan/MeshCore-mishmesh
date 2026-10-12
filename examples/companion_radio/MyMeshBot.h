#pragma once
// MeshCore-mishmesh Remote Bot v6.4.
// Supports DM, a configured GroupChannel, or both. Channel is selected by its
// saved channel name (for example #test). Commands: !ping !batt !loc !time
// !temp !hops !status !info !where !help. Optional trigger/reply also works in the selected
// transport mode. Channel replies are sent through the existing mishmesh
// ConvoKey/channel send path, so normal channel storage is preserved.

static const uint32_t MISHMESH_BOT_COOLDOWN_MS = 10000UL;
static const uint8_t MISHMESH_BOT_LOG_SIZE = 8;

struct MishmeshBotReplyLog {
  uint8_t key[4];
  uint32_t sent_ms;
  bool used;
};

static MishmeshBotReplyLog mishmesh_bot_log[MISHMESH_BOT_LOG_SIZE] = {};
static uint16_t mishmesh_bot_reply_count = 0;
static uint32_t mishmesh_bot_channel_last_ms[16] = {};

static bool mishmeshBotIsSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void mishmeshBotLowerCopy(const char* src, char* dst, size_t cap) {
  if (!dst || cap == 0) return;
  size_t i = 0;
  if (src) {
    while (src[i] && i + 1 < cap) {
      dst[i] = (char)tolower((uint8_t)src[i]);
      i++;
    }
  }
  dst[i] = '\0';
}

static bool mishmeshBotAllowed(const uint8_t* pubkey) {
  uint32_t now = millis();
  for (uint8_t i = 0; i < MISHMESH_BOT_LOG_SIZE; i++) {
    if (mishmesh_bot_log[i].used &&
        memcmp(mishmesh_bot_log[i].key, pubkey, 4) == 0) {
      return (uint32_t)(now - mishmesh_bot_log[i].sent_ms) >= MISHMESH_BOT_COOLDOWN_MS;
    }
  }
  return true;
}

static void mishmeshBotRecord(const uint8_t* pubkey) {
  uint32_t now = millis();
  int target = -1;

  for (uint8_t i = 0; i < MISHMESH_BOT_LOG_SIZE; i++) {
    if (mishmesh_bot_log[i].used &&
        memcmp(mishmesh_bot_log[i].key, pubkey, 4) == 0) {
      mishmesh_bot_log[i].sent_ms = now;
      return;
    }
    if (!mishmesh_bot_log[i].used && target < 0) target = i;
  }

  if (target < 0) {
    target = 0;
    for (uint8_t i = 1; i < MISHMESH_BOT_LOG_SIZE; i++) {
      if ((int32_t)(now - mishmesh_bot_log[i].sent_ms) >
          (int32_t)(now - mishmesh_bot_log[target].sent_ms)) {
        target = i;
      }
    }
  }

  memcpy(mishmesh_bot_log[target].key, pubkey, 4);
  mishmesh_bot_log[target].sent_ms = now;
  mishmesh_bot_log[target].used = true;
}

static bool mishmeshBotChannelNameEqual(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a == '#') a++;
  while (*b == '#') b++;
  while (*a && *b) {
    if (tolower((uint8_t)*a) != tolower((uint8_t)*b)) return false;
    a++; b++;
  }
  return *a == '\0' && *b == '\0';
}

static int mishmeshBotFindChannel(MyMesh& mesh, const char* wanted) {
  if (!wanted || !wanted[0]) return -1;
#ifdef MAX_GROUP_CHANNELS
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails ch;
    if (!mesh.getChannel(i, ch)) continue;
    bool empty = true;
    for (size_t b = 0; b < sizeof(ch.channel.secret); b++) {
      if (ch.channel.secret[b]) { empty = false; break; }
    }
    if (!empty && mishmeshBotChannelNameEqual(ch.name, wanted)) return i;
  }
#endif
  return -1;
}

static void mishmeshBotHexHash(char* out, size_t out_len, const uint8_t* hash, uint8_t hash_size) {
  if (!out || out_len < 2 || !hash) return;
  size_t used = 0;
  out[0] = '\0';
  for (uint8_t i = 0; i < hash_size && used + 2 < out_len; i++) {
    int n = snprintf(out + used, out_len - used, "%02X", hash[i]);
    if (n != 2) break;
    used += 2;
  }
}

static bool mishmeshBotChannelAllowed(uint8_t channel_idx) {
  if (channel_idx >= sizeof(mishmesh_bot_channel_last_ms) / sizeof(mishmesh_bot_channel_last_ms[0])) return true;
  if (mishmesh_bot_channel_last_ms[channel_idx] == 0) return true;
  return (uint32_t)(millis() - mishmesh_bot_channel_last_ms[channel_idx]) >= MISHMESH_BOT_COOLDOWN_MS;
}

static void mishmeshBotChannelRecord(uint8_t channel_idx) {
  if (channel_idx < sizeof(mishmesh_bot_channel_last_ms) / sizeof(mishmesh_bot_channel_last_ms[0]))
    mishmesh_bot_channel_last_ms[channel_idx] = millis();
}

static bool mishmeshBotReplyForCommand(const NodePrefs* prefs, const char* cmd, const char* arg, const char* arg2,
                                       uint8_t hops, const mesh::Packet* pkt, uint32_t now,
                                       char* out, size_t out_len) {
  (void)arg; (void)arg2;
  const bool show_location = prefs ? (prefs->bot_location_enabled != 0) : true;
  if (!strcmp(cmd, "ping")) { snprintf(out, out_len, "pong"); return true; }
  if (!strcmp(cmd, "batt")) {
    uint16_t mv = board.getBattMilliVolts();
    if (mv == 0) snprintf(out, out_len, "batt n/a");
    else snprintf(out, out_len, "batt %.2fV", (double)mv / 1000.0);
    return true;
  }
  if (!strcmp(cmd, "loc") || !strcmp(cmd, "where")) {
    if (!show_location) snprintf(out, out_len, "loc hidden");
    else if (sensors.node_lat == 0.0 && sensors.node_lon == 0.0) snprintf(out, out_len, "loc n/a");
    else snprintf(out, out_len, "loc %.5f,%.5f", sensors.node_lat, sensors.node_lon);
    return true;
  }
  if (!strcmp(cmd, "time")) {
    DateTime dt(now);
    snprintf(out, out_len, "%02u:%02u:%02u UTC %02u/%02u/%04u",
             (unsigned)dt.hour(), (unsigned)dt.minute(), (unsigned)dt.second(),
             (unsigned)dt.day(), (unsigned)dt.month(), (unsigned)dt.year());
    return true;
  }
  if (!strcmp(cmd, "temp")) {
    float t = board.getMCUTemperature();
    if (isnan(t)) snprintf(out, out_len, "temp n/a");
    else snprintf(out, out_len, "temp %.1fC", (double)t);
    return true;
  }
  if (!strcmp(cmd, "hops")) {
    if (hops == 0) snprintf(out, out_len, "direct");
    else snprintf(out, out_len, "%u hops", (unsigned)hops);
    return true;
  }
  if (!strcmp(cmd, "trace") || !strcmp(cmd, "tracer")) {
    // The actual trace is started asynchronously by MyMesh::handleMishmeshBot*().
    snprintf(out, out_len, "trace started");
    return true;
  }
  if (!strcmp(cmd, "path") || !strcmp(cmd, "route")) {
    // Show the route carried by the incoming packet. TRACE additionally
    // measures SNR per hop; PATH only reports the known hop hashes.
    if (!pkt) {
      snprintf(out, out_len, "path n/a");
      return true;
    }
    uint8_t hash_size = pkt->getPathHashSize();
    uint8_t path_bytes = pkt->getPathByteLen();
    uint8_t count = hash_size ? (uint8_t)(path_bytes / hash_size) : 0;
    if (hash_size == 0 || count == 0) {
      snprintf(out, out_len, "Path:direct");
      return true;
    }
    size_t used = 0;
    int n = snprintf(out, out_len, "Hops:%u|Path:", (unsigned)count);
    if (n < 0) return true;
    used = (size_t)n < out_len ? (size_t)n : out_len - 1;
    for (uint8_t i = 0; i < count && used + 3 < out_len; ++i) {
      char hx[9] = {};
      mishmeshBotHexHash(hx, sizeof(hx), &pkt->path[i * hash_size], hash_size);
      n = snprintf(out + used, out_len - used, "%s%s", i ? "->" : "", hx);
      if (n < 0) break;
      size_t add = (size_t)n;
      if (add >= out_len - used) { used = out_len - 1; break; }
      used += add;
    }
    return true;
  }
  if (!strcmp(cmd, "status") || !strcmp(cmd, "info")) {
    uint16_t mv = board.getBattMilliVolts();
    float t = board.getMCUTemperature();
    float snr = pkt ? pkt->getSNR() : 0.0f;
    const char* node = the_mesh.getNodePrefs() ? the_mesh.getNodePrefs()->node_name : "NONAME";
    bool has_loc = !(sensors.node_lat == 0.0 && sensors.node_lon == 0.0);
    if (!show_location) {
      if (isnan(t)) snprintf(out, out_len, "%s | GPS hidden | batt %.2fV | temp n/a | %u hops | SNR %.1fdB", node, (double)mv / 1000.0, (unsigned)hops, (double)snr);
      else snprintf(out, out_len, "%s | GPS hidden | batt %.2fV | temp %.1fC | %u hops | SNR %.1fdB", node, (double)mv / 1000.0, (double)t, (unsigned)hops, (double)snr);
    } else if (has_loc) {
      if (isnan(t)) snprintf(out, out_len, "%s | GPS %.5f,%.5f | batt %.2fV | temp n/a | %u hops | SNR %.1fdB", node, sensors.node_lat, sensors.node_lon, (double)mv / 1000.0, (unsigned)hops, (double)snr);
      else snprintf(out, out_len, "%s | GPS %.5f,%.5f | batt %.2fV | temp %.1fC | %u hops | SNR %.1fdB", node, sensors.node_lat, sensors.node_lon, (double)mv / 1000.0, (double)t, (unsigned)hops, (double)snr);
    } else {
      if (isnan(t)) snprintf(out, out_len, "%s | GPS n/a | batt %.2fV | temp n/a | %u hops | SNR %.1fdB", node, (double)mv / 1000.0, (unsigned)hops, (double)snr);
      else snprintf(out, out_len, "%s | GPS n/a | batt %.2fV | temp %.1fC | %u hops | SNR %.1fdB", node, (double)mv / 1000.0, (double)t, (unsigned)hops, (double)snr);
    }
    return true;
  }
  if (!strcmp(cmd, "help")) { snprintf(out, out_len, "cmds: !ping !batt !loc !where !time !temp !hops !trace !path !status !info !help"); return true; }
  return false;
}

static int mishmeshBotScan(const NodePrefs* prefs, const char* text, uint8_t hops, const mesh::Packet* pkt, uint32_t now, char* out, size_t out_len) {
  if (!text || !out || out_len < 2) return 0;
  int matched = 0; size_t used = 0; const char* p = text;
  while (*p) {
    while (*p && mishmeshBotIsSpace(*p)) p++;
    if (!*p) break;
    if (*p != '!') { while (*p && !mishmeshBotIsSpace(*p)) p++; continue; }
    p++;
    char cmd[16] = {}; size_t ci = 0;
    while (*p && !mishmeshBotIsSpace(*p)) { if (ci + 1 < sizeof(cmd)) cmd[ci++] = (char)tolower((uint8_t)*p); p++; }
    cmd[ci] = '\0'; if (!cmd[0]) continue;
    char arg1[16] = {}, arg2[16] = {};
    while (*p && mishmeshBotIsSpace(*p)) p++;
    if (*p && *p != '!') {
      size_t ai = 0; while (*p && !mishmeshBotIsSpace(*p)) { if (ai + 1 < sizeof(arg1)) arg1[ai++] = (char)tolower((uint8_t)*p); p++; } arg1[ai] = '\0';
      while (*p && mishmeshBotIsSpace(*p)) p++;
      if (*p && *p != '!') { size_t aj = 0; while (*p && !mishmeshBotIsSpace(*p)) { if (aj + 1 < sizeof(arg2)) arg2[aj++] = (char)tolower((uint8_t)*p); p++; } arg2[aj] = '\0'; }
    }
    char seg[192];
    if (!mishmeshBotReplyForCommand(prefs, cmd, arg1, arg2, hops, pkt, now, seg, sizeof(seg))) continue;
    if (matched && used + 3 < out_len) { out[used++]=' '; out[used++]='|'; out[used++]=' '; }
    size_t slen = strlen(seg); if (used + slen >= out_len) slen = out_len - used - 1;
    if (slen) { memcpy(out + used, seg, slen); used += slen; }
    matched++;
  }
  out[used] = '\0'; return matched;
}

static bool mishmeshBotAppend(char* out, size_t out_len, size_t* used, const char* text, bool sep) {
  if (!out || !used || !text || !text[0] || *used >= out_len) return false;
  const char* prefix = sep ? "|" : "";
  size_t plen = strlen(prefix);
  size_t tlen = strlen(text);
  if (*used + plen + tlen >= out_len) {
    if (*used + plen >= out_len) return false;
    tlen = out_len - *used - plen - 1;
  }
  if (plen) { memcpy(out + *used, prefix, plen); *used += plen; }
  if (tlen) { memcpy(out + *used, text, tlen); *used += tlen; }
  out[*used] = '\0';
  return true;
}

static bool mishmeshBotMakeTriggerReply(const NodePrefs* prefs, const char* sender_name, const mesh::Packet* pkt,
                                         float rssi, char* reply, size_t reply_len) {
  if (!prefs || !reply || reply_len < 2) return false;
  const uint8_t fields = prefs->bot_reply_fields ? prefs->bot_reply_fields : 55;
  size_t used = 0;
  reply[0] = '\0';
  const char* prefix = prefs->bot_reply[0] ? prefs->bot_reply : "test";
  mishmeshBotAppend(reply, reply_len, &used, prefix, false);
  char part[128];
  const uint8_t hops = pkt ? pkt->getPathHashCount() : 0;
  if (fields & 1) {
    mishmeshBotAppend(reply, reply_len, &used,
        (sender_name && sender_name[0]) ? sender_name : "Unknown", used != 0);
  }
  if (fields & 16) {
    snprintf(part, sizeof(part), "SNR:%.1fdB", pkt ? (double)pkt->getSNR() : 0.0);
    mishmeshBotAppend(reply, reply_len, &used, part, used != 0);
  }
  if (fields & 2) {
    snprintf(part, sizeof(part), "RSSI:%.0fdBm", (double)rssi);
    mishmeshBotAppend(reply, reply_len, &used, part, used != 0);
  }
  if (fields & 32) {
    snprintf(part, sizeof(part), "Hops:%u", (unsigned)hops);
    mishmeshBotAppend(reply, reply_len, &used, part, used != 0);
  }
  if (fields & 4) {
    size_t path_used = 0;
    part[0] = '\0';
    if (pkt && hops) {
      const uint8_t hash_size = pkt->getPathHashSize();
      const uint8_t path_bytes = pkt->getPathByteLen();
      if (hash_size > 0) {
        const uint8_t byte_len = (uint8_t)(path_bytes - (path_bytes % hash_size));
        for (uint8_t offset = 0, hop = 0; offset < byte_len && path_used + 10 < sizeof(part); offset += hash_size, hop++) {
          char hx[9] = {};
          mishmeshBotHexHash(hx, sizeof(hx), &pkt->path[offset], hash_size);
          int n = snprintf(part + path_used, sizeof(part) - path_used, "%s%s", hop ? "->" : "", hx);
          if (n <= 0 || (size_t)n >= sizeof(part) - path_used) break;
          path_used += (size_t)n;
        }
      }
    }
    if (!path_used) snprintf(part, sizeof(part), "direct");
    char path_part[sizeof(part)];
    snprintf(path_part, sizeof(path_part), "Path:%s", part);
    mishmeshBotAppend(reply, reply_len, &used, path_part, used != 0);
  }
  return reply[0] != '\0';
}

static bool mishmeshBotMakeReply(const NodePrefs* prefs, const char* text, const char* sender_name, uint8_t hops, const mesh::Packet* pkt, uint32_t now,
                                  float rssi, char* reply, size_t reply_len) {
  char lower[192]; mishmeshBotLowerCopy(text, lower, sizeof(lower));
  if (strlen(lower) >= sizeof(lower) - 1) return false;
  int matched = 0;
  // Channel/transport implementations may deliver text with leading whitespace.
  // Commands should still be recognized when the first non-space character is '!'.
  const char* command_text = text;
  while (*command_text && mishmeshBotIsSpace(*command_text)) command_text++;
  // Channel messages may be decorated by the transport/UI (for example with a
  // sender prefix) before reaching the bot. Do not require '!' to be the first
  // byte of the delivered string. mishmeshBotScan() already skips ordinary
  // tokens and extracts every !command it finds.
  if (prefs->bot_commands_enabled) {
    matched = mishmeshBotScan(prefs, command_text, hops, pkt, now, reply, reply_len);
  }
  if (matched <= 0 && prefs->bot_trigger[0]) {
    char trigger[64]; mishmeshBotLowerCopy(prefs->bot_trigger, trigger, sizeof(trigger));
    if (trigger[0] && strstr(lower, trigger)) {
      matched = mishmeshBotMakeTriggerReply(prefs, sender_name, pkt, rssi, reply, reply_len) ? 1 : 0;
    }
  }
  return matched > 0;
}


static bool mishmeshBotCommandIsTrace(const char* text) {
  if (!text) return false;
  const char* p = text;
  while (*p) {
    while (*p && mishmeshBotIsSpace(*p)) p++;
    if (*p == '!') {
      p++;
      char cmd[16] = {};
      size_t n = 0;
      while (*p && !mishmeshBotIsSpace(*p)) {
        if (n + 1 < sizeof(cmd)) cmd[n++] = (char)tolower((uint8_t)*p);
        p++;
      }
      cmd[n] = '\0';
      if (!strcmp(cmd, "trace") || !strcmp(cmd, "tracer")) return true;
    } else {
      while (*p && !mishmeshBotIsSpace(*p)) p++;
    }
  }
  return false;
}

bool MyMesh::mishmeshBotTraceRequested(const char* text) const {
  return mishmeshBotCommandIsTrace(text);
}

bool MyMesh::startMishmeshBotTrace(const uint8_t *requester_pubkey, uint8_t channel_idx, bool channel,
                                   const mesh::Packet *rx_pkt) {
  if (_mishmesh_bot_trace.pending) return false;

  uint8_t hash_size = 1;
  uint8_t count = 0;
  uint8_t route[64] = {};

  // For a DM, prefer the contact's stored OUT path. It is already in the
  // correct direction (this bot -> requester) and is the same path used by
  // normal direct messages. If no path is known, fall back to reversing the
  // incoming flood path.
  if (!channel && requester_pubkey) {
    ContactInfo* contact = lookupContactByPubKey((uint8_t*)requester_pubkey, 6);
    if (contact && contact->out_path_len != OUT_PATH_UNKNOWN && contact->out_path_len > 0) {
      hash_size = contact->out_path_len >> 6 ? (uint8_t)((contact->out_path_len >> 6) + 1) : 1;
      count = contact->out_path_len & 63;
      const uint8_t byte_len = contact->out_path_len & 63;
      const uint8_t actual_bytes = (uint8_t)(count * hash_size);
      if (actual_bytes > sizeof(route) || count == 0) return false;
      memcpy(route, contact->out_path, actual_bytes);
    }
  }

  // Fallback / channel route: incoming flood path is sender -> ... -> bot.
  // Reverse it so the TRACE travels bot -> ... -> sender-side path.
  if (count == 0 && rx_pkt && rx_pkt->isRouteFlood()) {
    hash_size = rx_pkt->getPathHashSize();
    count = rx_pkt->getPathHashCount();
    const uint8_t byte_len = rx_pkt->getPathByteLen();
    if (hash_size < 1 || hash_size > 2 || byte_len > sizeof(route)) return false;
    for (uint8_t i = 0; i < count; i++) {
      const uint8_t src_off = (uint8_t)((count - 1 - i) * hash_size);
      memcpy(&route[i * hash_size], &rx_pkt->path[src_off], hash_size);
    }
  }

  // TRACE packets have no encrypted destination key: include the target hash
  // after any repeater path so it can reach the requester before looping home.
  if (!channel && requester_pubkey) {
    if ((size_t)(count + 1) * hash_size > 31) return false;
    memcpy(&route[count * hash_size], requester_pubkey, hash_size);
    count++;
  }

  // TRACE is one-way; append our own hash so the packet makes a loop and the
  // result is delivered back to this bot's onTraceRecv().
  if ((size_t)(count + 1) * hash_size > 31) return false;
  self_id.copyHashTo(&route[count * hash_size], hash_size);
  count++;

  uint32_t tag = _ms->getMillis();
  uint8_t flags = (uint8_t)(hash_size - 1);
  mesh::Packet* trace = startTrace(tag, 0, flags, route, (uint8_t)(count * hash_size));
  if (!trace) return false;

  _mishmesh_bot_trace.pending = true;
  _mishmesh_bot_trace.channel = channel;
  _mishmesh_bot_trace.channel_idx = channel_idx;
  _mishmesh_bot_trace.tag = tag;
  _mishmesh_bot_trace.started_ms = millis();
  if (requester_pubkey) memcpy(_mishmesh_bot_trace.pubkey, requester_pubkey, PUB_KEY_SIZE);
  else memset(_mishmesh_bot_trace.pubkey, 0, PUB_KEY_SIZE);
  return true;
}

void MyMesh::handleMishmeshBotTraceResult(uint32_t tag, const uint8_t *path_snrs,
                                           const uint8_t *path_hashes, uint8_t path_len,
                                           uint8_t flags, float final_snr) {
  if (!_mishmesh_bot_trace.pending || tag != _mishmesh_bot_trace.tag) return;

  const uint8_t hash_size = (uint8_t)(1U << (flags & 0x03));
  const uint8_t hops = (uint8_t)(path_len / hash_size);
  const uint8_t snr_count = hops;
  char reply[220] = {};
  size_t used = 0;
  int n = snprintf(reply, sizeof(reply), "Hops:%u", (unsigned)hops);
  if (n > 0) used = (size_t)n;

  if (hops) {
    if (used + 8 < sizeof(reply)) {
      n = snprintf(reply + used, sizeof(reply) - used, "|Path:");
      if (n > 0) used += (size_t)n;
    }
    for (uint8_t i = 0; i < hops && used + 12 < sizeof(reply); i++) {
      char hx[9] = {};
      mishmeshBotHexHash(hx, sizeof(hx), &path_hashes[i * hash_size], hash_size);
      float snr = (i < snr_count) ? ((int8_t)path_snrs[i]) / 4.0f : 0.0f;
      n = snprintf(reply + used, sizeof(reply) - used, "%s%s(%.1f)",
                   i ? "->" : "", hx, (double)snr);
      if (n <= 0) break;
      used += (size_t)n;
    }
  }
  if (used + 18 < sizeof(reply)) {
    n = snprintf(reply + used, sizeof(reply) - used, "|Final:%.1fdB", (double)final_snr);
    if (n > 0) used += (size_t)n;
  }
  reply[sizeof(reply) - 1] = '\0';

  mishmesh::ConvoKey key = _mishmesh_bot_trace.channel
      ? mishmesh::channelKey(_mishmesh_bot_trace.channel_idx)
      : mishmesh::directKey(_mishmesh_bot_trace.pubkey);
  mishmeshSendTextImpl(key, reply, nullptr);
  _mishmesh_bot_trace.pending = false;
}

void MyMesh::checkMishmeshBotTraceTimeout() {
  if (!_mishmesh_bot_trace.pending) return;
  if ((uint32_t)(millis() - _mishmesh_bot_trace.started_ms) < 15000UL) return;

  const char* msg = "Trace timeout";
  mishmesh::ConvoKey key = _mishmesh_bot_trace.channel
      ? mishmesh::channelKey(_mishmesh_bot_trace.channel_idx)
      : mishmesh::directKey(_mishmesh_bot_trace.pubkey);
  mishmeshSendTextImpl(key, msg, nullptr);
  _mishmesh_bot_trace.pending = false;
}

bool MyMesh::handleMishmeshBot(const ContactInfo &from, mesh::Packet *pkt, const char *text) {
  if (from.type != ADV_TYPE_CHAT || !text) return false;
  NodePrefs *prefs = getNodePrefs();
  if (!prefs || !prefs->bot_enabled || (prefs->bot_mode == 1)) return false;
  char reply[256]; uint8_t hops = pkt ? pkt->getPathHashCount() : 0;
  uint32_t now = getRTCClock()->getCurrentTime();
  if (!mishmeshBotMakeReply(prefs, text, from.name, hops, pkt, now, _radio->getLastRSSI(), reply, sizeof(reply))) return false;
  if (!mishmeshBotAllowed(from.id.pub_key)) return true;
  if (mishmeshBotTraceRequested(text)) {
    if (!startMishmeshBotTrace(from.id.pub_key, 0, false, pkt)) {
      snprintf(reply, sizeof(reply), "trace unavailable");
      mishmesh::ConvoKey key = mishmesh::directKey(from.id.pub_key);
      mishmeshSendTextImpl(key, reply, nullptr);
    }
  } else {
    // A flood DM carries the actual inbound relay path. Use its reverse for
    // this immediate reply; the contact's cached out_path may be unknown or
    // stale, especially for a sender several hops away. sendMessage copies
    // the path into the outbound packet synchronously, so restore the cache
    // immediately after queuing to avoid replacing a better learned route.
    ContactInfo* route_contact = lookupContactByPubKey(from.id.pub_key, 6);
    const bool have_contact = route_contact != nullptr;
    uint8_t saved_path_len = 0;
    uint8_t saved_path[MAX_PATH_SIZE] = {};
    bool temporary_path = false;
    if (have_contact && pkt && pkt->isRouteFlood()) {
      const uint8_t hash_size = pkt->getPathHashSize();
      const uint8_t count = pkt->getPathHashCount();
      const uint8_t byte_len = pkt->getPathByteLen();
      if (hash_size > 0 && byte_len <= MAX_PATH_SIZE && (uint16_t)count * hash_size == byte_len) {
        saved_path_len = route_contact->out_path_len;
        memcpy(saved_path, route_contact->out_path, sizeof(saved_path));
        for (uint8_t i = 0; i < count; i++) {
          const uint8_t src = (uint8_t)((count - 1 - i) * hash_size);
          memcpy(&route_contact->out_path[(size_t)i * hash_size], &pkt->path[src], hash_size);
        }
        route_contact->out_path_len = count ? (uint8_t)(((hash_size - 1) << 6) | count) : 0;
        temporary_path = true;
      }
    }
    mishmesh::ConvoKey key = mishmesh::directKey(from.id.pub_key);
    uint32_t sender_time = 0;
    bool sent = mishmeshSendTextImpl(key, reply, &sender_time);
    if (temporary_path) {
      route_contact->out_path_len = saved_path_len;
      memcpy(route_contact->out_path, saved_path, sizeof(saved_path));
    }
    if (!sent) return false;
  }
  mishmeshBotRecord(from.id.pub_key); mishmesh_bot_reply_count++; return true;
}

bool MyMesh::handleMishmeshBotChannel(const mesh::GroupChannel &channel, mesh::Packet *pkt, const char *text) {
  if (!text) return false;
  NodePrefs *prefs = getNodePrefs();
  if (!prefs || !prefs->bot_enabled || prefs->bot_mode == 0 || !prefs->bot_channel[0]) return false;
  int channel_idx = findChannelIdx(channel);
  if (channel_idx < 0) return false;
  ChannelDetails configured;
  if (!getChannel(channel_idx, configured)) return false;
  if (!mishmeshBotChannelNameEqual(configured.name, prefs->bot_channel)) return false;

  uint8_t hops = pkt ? pkt->getPathHashCount() : 0;
  uint32_t now = getRTCClock()->getCurrentTime();
  char reply[256];
  char sender_name[32] = {};
  const char* msg_text = text;
  const char* colon = text ? strchr(text, ':') : nullptr;
  if (colon && colon > text) {
    size_t n = (size_t)(colon - text);
    while (n && mishmeshBotIsSpace(text[n - 1])) n--;
    if (n >= sizeof(sender_name)) n = sizeof(sender_name) - 1;
    memcpy(sender_name, text, n);
    sender_name[n] = '\0';
    msg_text = colon + 1;
    while (*msg_text && mishmeshBotIsSpace(*msg_text)) msg_text++;
  }
  if (!mishmeshBotMakeReply(prefs, msg_text, sender_name, hops, pkt, now, _radio->getLastRSSI(), reply, sizeof(reply))) return false;
  if (!mishmeshBotChannelAllowed((uint8_t)channel_idx)) return true;

  mishmesh::ConvoKey key = mishmesh::channelKey((uint8_t)channel_idx);
  if (mishmeshBotTraceRequested(msg_text)) {
    if (!startMishmeshBotTrace(nullptr, (uint8_t)channel_idx, true, pkt)) {
      snprintf(reply, sizeof(reply), "trace unavailable");
      if (!mishmeshSendTextImpl(key, reply, nullptr)) return false;
    }
  } else {
    if (!mishmeshSendTextImpl(key, reply, nullptr)) return false;
  }
  mishmeshBotChannelRecord((uint8_t)channel_idx);
  mishmesh_bot_reply_count++;
  return true;
}
