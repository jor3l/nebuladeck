#include "store.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* PATH = "/agents.json";

bool Store::begin() {
  ok_ = LittleFS.begin(true);  // formats the data partition on first use
  if (!ok_) Serial.println("store: no filesystem, state won't survive restarts");
  return ok_;
}

void Store::poll(Ui& ui, uint32_t now) {
  if (!ui.dirty && now - lastSave_ >= RESTORED_SAVE_MS)
    for (const Agent& a : ui.agents)
      if (a.used && a.restored) { ui.dirty = true; break; }
  if (!ui.dirty || now - lastSave_ < SAVE_EVERY_MS) return;
  save(ui, now);
}

void Store::save(const Ui& ui, uint32_t now) {
  const_cast<Ui&>(ui).dirty = false;
  lastSave_ = now;
  if (!ok_) return;
  JsonDocument doc;
  doc["v"] = 1;
  JsonArray list = doc["agents"].to<JsonArray>();
  for (const Agent& a : ui.agents) {
    if (!a.used) continue;
    JsonObject o = list.add<JsonObject>();
    o["name"] = a.name;
    o["project"] = a.project;
    o["age"] = now - a.lastSeen;
    o["restored"] = a.restored;
    o["title"] = a.title;
    o["status"] = a.status;
    o["progress"] = a.progress;
    o["task_age"] = now - a.taskStart;
    o["presence"] = a.presence;
    o["subagents"] = a.subagents;
    o["attn"] = a.attn;
    o["completed"] = a.completed;
    o["summary"] = a.summary;
    o["tok_in"] = a.tokIn;
    o["tok_out"] = a.tokOut;
    o["duration"] = a.durationS;
    JsonArray msgs = o["msgs"].to<JsonArray>();
    for (int i = 0; i < a.nmsg; i++) {
      JsonArray m = msgs.add<JsonArray>();
      m.add(a.msgs[i].text); m.add(a.msgs[i].level); m.add(now - a.msgs[i].at);
      m.add(a.msgs[i].ttl); m.add(a.msgs[i].count);
    }
    if (a.askId.length()) {
      JsonObject q = o["ask"].to<JsonObject>();
      q["id"] = a.askId;
      q["prompt"] = a.askPrompt;
      JsonArray opts = q["opts"].to<JsonArray>();
      for (int i = 0; i < a.nopts; i++) opts.add(a.opts[i]);
      q["asking"] = a.asking;
      q["state"] = a.askState;
      q["before"] = a.beforeAsk;
      q["left"] = a.askDeadline ? (int32_t)(a.askDeadline - now) : 0;  // 0 = no deadline
      q["label"] = a.askLabel;
      q["choice"] = a.askChoice;
    }
  }
  // Write to a temp file and rename, so a power cut mid-write never leaves a broken file.
  File f = LittleFS.open("/agents.tmp", "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
  LittleFS.remove(PATH);
  LittleFS.rename("/agents.tmp", PATH);
}

void Store::load(Ui& ui, uint32_t now, uint32_t leaseMs) {
  if (!ok_ || !LittleFS.exists(PATH)) return;
  File f = LittleFS.open(PATH, "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err || (doc["v"] | 0) != 1) return;
  int n = 0;
  for (JsonObject o : doc["agents"].as<JsonArray>()) {
    if (n >= Ui::MAXAGENTS) break;
    Agent& a = ui.agents[n++];
    a.clear();
    a.used = true;
    a.name = o["name"] | "Agent";
    a.restored = true;  // until it sends something again
    a.project = o["project"] | "";
    // Pretend it was last seen long enough ago that only RESTORE_LEASE_MS of lease is left.
    uint32_t age = o["age"] | 0;
    uint32_t minAge = leaseMs > RESTORE_LEASE_MS ? leaseMs - RESTORE_LEASE_MS : 0;
    a.lastSeen = now - (age > minAge ? age : minAge);
    a.title = o["title"] | "";
    a.status = o["status"] | (int)ST_IDLE;
    a.progress = o["progress"] | -1.0f;
    a.taskStart = now - (uint32_t)(o["task_age"] | 0);
    a.presence = o["presence"] | "";
    a.subagents = o["subagents"] | 0;
    a.attn = o["attn"] | false;
    a.completed = o["completed"] | false;
    a.summary = o["summary"] | "";
    a.tokIn = o["tok_in"] | 0;
    a.tokOut = o["tok_out"] | 0;
    a.durationS = o["duration"] | -1;
    for (JsonArray m : o["msgs"].as<JsonArray>()) {
      if (a.nmsg >= Agent::MAXMSG) break;
      Msg& g = a.msgs[a.nmsg++];
      g.text = m[0] | ""; g.level = m[1] | 0; g.at = now - (uint32_t)(m[2] | 0);
      g.ttl = m[3] | 0; g.count = m[4] | 1;
    }
    JsonObject q = o["ask"];
    if (!q.isNull()) {
      a.askId = q["id"] | "";
      a.askPrompt = q["prompt"] | "";
      for (JsonVariant v : q["opts"].as<JsonArray>())
        if (a.nopts < Agent::MAXOPT) a.opts[a.nopts++] = v.as<String>();
      a.asking = q["asking"] | false;
      a.askState = q["state"] | (int)ASK_NONE;
      a.beforeAsk = q["before"] | (int)ST_IDLE;
      int32_t left = q["left"] | 0;
      a.askDeadline = left ? now + (uint32_t)max(left, (int32_t)1) : 0;
      a.askLabel = q["label"] | "";
      a.askChoice = q["choice"] | -1;
    }
  }
  ui.dirty = n > 0;  // save the shortened leases right away
  Serial.printf("store: restored %d agent(s)\n", n);
}
