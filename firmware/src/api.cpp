#include "api.h"
#include <WebServer.h>
#include <WiFi.h>
#include "version.h"

static WebServer server(80);

// The setup page may read /api/info to test the connection ("Test NebulaDeck"). Only that
// origin: other websites can still reach the device but can't read its answers.
static const char* PAGE_ORIGIN = "https://jor3l.github.io";

static void corsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", PAGE_ORIGIN);
  server.sendHeader("Access-Control-Allow-Private-Network", "true");  // Chrome's local-network preflight
}

static uint8_t parseStatus(const char* s) {
  if (!s) return ST_IDLE;
  if (!strcmp(s, "working")) return ST_WORKING;
  if (!strcmp(s, "waiting")) return ST_WAITING;
  if (!strcmp(s, "done")) return ST_DONE;
  if (!strcmp(s, "error")) return ST_ERROR;
  return ST_IDLE;
}

static uint8_t parseLevel(const char* s) {
  if (!s) return LV_INFO;
  if (!strcmp(s, "warn")) return LV_WARN;
  if (!strcmp(s, "error")) return LV_ERROR;
  return LV_INFO;
}

static void send(int code, JsonDocument& doc) {
  String s;
  serializeJson(doc, s);
  server.send(code, "application/json", s);
}

static void sendError(int code, const char* msg) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = msg;
  send(code, doc);
}

// Claude Code hook event -> presence text (see docs/PROTOCOL.md, "Hooks").
static const char* toolText(const String& t) {
  if (t.startsWith("Bash")) return "Running a command...";
  if (t == "Read" || t == "NotebookRead") return "Reading...";
  if (t == "Edit" || t == "Write" || t == "MultiEdit" || t == "NotebookEdit") return "Editing...";
  if (t == "Grep" || t == "Glob" || t == "LS") return "Searching...";
  if (t == "WebSearch" || t == "WebFetch") return "Browsing...";
  if (t == "Task" || t == "Agent") return "Delegating...";
  if (t.startsWith("TodoWrite") || t.startsWith("Plan")) return "Planning...";
  return "Working...";
}

// Finds the agent a request is about: same name and project, or (when the request names no
// project, like a skill's msg) the most recent agent with that name. Creates one if asked,
// reusing the least recently seen slot when all are taken.
Agent* Api::resolve(JsonDocument& doc, uint32_t now, bool create) {
  String name = doc["agent"].is<const char*>() ? doc["agent"].as<String>() : String();
  String project = doc["project"].is<const char*>() ? doc["project"].as<String>() : String();
  String sub = doc["sub"].is<const char*>() ? doc["sub"].as<String>() : String();
  if (name.length() > 32) name.remove(32);
  if (project.length() > 48) project.remove(48);
  if (sub.length() > 40) sub.remove(40);
  Agent* best = nullptr;
  for (Agent& a : ui_.agents) {
    if (!a.used) continue;
    if (a.sub != sub) continue;  // a session and its subagents are separate cards
    if (name.length() && !a.name.equalsIgnoreCase(name)) continue;
    bool exact = project.length() && a.project == project;
    if (project.length() && a.project.length() && !exact) continue;
    // Prefer an exact project match, then the most recently seen.
    if (!best || exact > (best->project == project) || (exact == (best->project == project) && (int32_t)(a.lastSeen - best->lastSeen) > 0)) best = &a;
  }
  if (best) {
    if (project.length() && !best->project.length()) best->project = project;  // adopt
    return best;
  }
  if (!create) return nullptr;
  Agent* slot = nullptr;
  for (Agent& a : ui_.agents)
    if (!a.used) { slot = &a; break; }
  if (!slot) {
    for (Agent& a : ui_.agents)
      if (!slot || (int32_t)(slot->lastSeen - a.lastSeen) > 0) slot = &a;
    cancelAsk(*slot, "evicted");
  }
  slot->clear();
  slot->used = true;
  slot->name = name.length() ? name : String("Agent");
  slot->project = project;
  slot->sub = sub;
  if (sub.length()) slot->role = doc["role"].is<const char*>() ? doc["role"].as<String>().substring(0, 24) : String("agent");
  slot->lastSeen = now;
  note("new " + slot->name + " / " + slot->project);
  return slot;
}

String Api::working(int n) { return n > 1 ? String(n) + " agents working..." : String("An agent is working..."); }

// A subagent came or went: keep its session alive and its "agents working" line current.
void Api::refreshParent(JsonDocument& sub, uint32_t now) {
  JsonDocument p;
  p["agent"] = sub["agent"];
  if (sub["project"].is<const char*>()) p["project"] = sub["project"];
  Agent* parent = resolve(p, now, false);
  if (!parent) return;
  parent->lastSeen = now;
  int kids = ui_.children(*parent);
  bool showsWorking = parent->presence.endsWith("agents working...") || parent->presence == "An agent is working...";
  if (showsWorking) parent->presence = kids ? working(kids) : String("Ready");
  ui_.dirty = true;
}

Agent* Api::findAsk(const String& id) {
  if (!id.length()) return nullptr;
  for (Agent& a : ui_.agents)
    if (a.used && a.askId == id) return &a;
  return nullptr;
}

void Api::begin() {
  server.on("/api/info", HTTP_GET, [this] { info(); });
  server.on("/api/info", HTTP_OPTIONS, [] {
    corsHeaders();
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.send(204);
  });
  server.on("/api/ask", HTTP_GET, [this] { askStatus(); });
  server.on("/api/wifi/forget", HTTP_POST, [this] {
    JsonDocument doc;
    doc["ok"] = true;
    send(200, doc);
    delay(100);  // let the reply out before the radio drops
    if (onForgetWifi) onForgetWifi();
  });
  // Restart the device (state is saved first and restored afterwards; used by the tests).
  server.on("/api/restart", HTTP_POST, [this] {
    JsonDocument doc;
    doc["ok"] = true;
    send(200, doc);
    if (onRestart) onRestart();
  });
  server.on("/api/update", HTTP_POST, [this] {
    JsonDocument doc;
    doc["ok"] = ui_.update.length() > 0;
    if (ui_.update.length()) doc["installing"] = ui_.update;
    else doc["error"] = "up to date";
    send(ui_.update.length() ? 200 : 409, doc);
    if (ui_.update.length() && onUpdate) onUpdate();  // runs from the main loop, after this reply
  });
  // Plain-text ask result for shell loops: "pending", "cancelled" or "answered:<label>".
  server.on("/api/answer", HTTP_GET, [this] {
    Agent* a = findAsk(server.arg("id"));
    uint8_t st = a ? a->askState : ASK_CANCELLED;
    server.send(200, "text/plain", st == ASK_PENDING ? String("pending") : st == ASK_ANSWERED ? "answered:" + a->askLabel : String("cancelled"));
  });
  // Claude Code hooks: POST /api/hook?event=<hook_event_name>&tool=&kind=&project=&agent=.
  // The event body is never needed (it can hold whole files), so hooks send none.
  server.on("/api/hook", HTTP_POST, [this] {
    requests_++;
    String ev = server.arg("event"), tool = server.arg("tool"), sub = server.arg("sub");
    JsonDocument in, out;
    const char* t = "presence";
    in["agent"] = server.hasArg("agent") ? server.arg("agent") : "Claude";  // the hook format is Claude Code's
    if (server.arg("project").length()) in["project"] = server.arg("project");
    uint32_t now = millis();
    out["ok"] = true;

    // Inside a subagent (Claude Code adds agent_id / agent_type): its own card, under its session.
    if (sub.length()) {
      in["sub"] = sub;
      if (server.arg("role").length()) in["role"] = server.arg("role");
      if (ev == "SubagentStop") {
        if (Agent* c = resolve(in, now, false)) { cancelAsk(*c, "subagent done"); note("done " + Ui::displayName(*c)); c->clear(); ui_.dirty = true; }
        lastRequest = now;
        refreshParent(in, now);
        return send(200, out);
      }
      if (ev == "SubagentStart") in["text"] = "Starting...";
      else if (ev == "PreToolUse") in["text"] = toolText(tool);
      else if (ev == "Notification" && server.arg("kind") != "idle") { in["text"] = "Needs you"; in["attn"] = true; }
      else in["text"] = "Working...";
      apply("presence", in, out, now);
      refreshParent(in, now);
      return send(200, out);
    }

    // The session itself. Its subagents keep running after its own turn ends (Stop, then the
    // idle reminder): while any do, it reads as working, not "Ready" / "Waiting for you".
    Agent* known = resolve(in, now, false);
    int kids = known ? ui_.children(*known) : 0;
    if (ev == "SessionStart") { in["text"] = "Ready"; in["new_turn"] = true; }
    else if (ev == "UserPromptSubmit") { in["text"] = "Thinking..."; in["new_turn"] = true; }
    else if (ev == "PreToolUse") in["text"] = toolText(tool);
    // Claude Code notifies both for permission prompts (attn) and, a minute after a turn
    // ends, "waiting for your input" (kind=idle): calm, not an alarm.
    else if (ev == "Notification" && server.arg("kind") == "idle") in["text"] = kids ? working(kids) : String("Waiting for you");
    else if (ev == "Notification") { in["text"] = "Needs you"; in["attn"] = true; }
    else if (ev == "Stop") { in["text"] = kids ? working(kids) : String("Ready"); in["end_turn"] = true; }
    else if (ev == "SessionEnd") t = "bye";
    else if (ev == "SubagentStart" || ev == "SubagentStop") return send(200, out);  // no agent_id (older Claude Code)
    else return sendError(400, "unknown event");
    apply(t, in, out, now);  // SessionEnd -> bye, which also removes the session's subagents
    // During a task, tool steps also go into its activity list (without the "...").
    Agent* a = ev == "PreToolUse" ? resolve(in, now, false) : nullptr;
    if (a && !a->completed && a->status != ST_IDLE) {
      String step = in["text"].as<String>();
      if (step.endsWith("...")) step.remove(step.length() - 3);
      a->pushStep(step, now);
    }
    send(200, out);
  });
  server.onNotFound([this] { route(); });
  server.begin();
}

void Api::poll(uint32_t now) {
  // Frames take 60-100 ms, so one handleClient() per loop made each request wait ~3 frames
  // (accept, read, answer). Run a few passes, so a request usually completes in one turn.
  for (int i = 0; i < 6; i++) {
    server.handleClient();
    if (!server.client()) break;
  }
  for (Agent& a : ui_.agents) {
    if (!a.used) continue;
    // Signed: requests handled inside poll(now) stamp lastSeen with a slightly later
    // millis() than `now`; unsigned, that wrapped to ~49 days and dropped the agent.
    uint32_t lease = a.sub.length() ? SUB_LEASE_MS : LEASE_MS;  // a missed SubagentStop shouldn't linger
    if ((int32_t)(now - a.lastSeen) >= (int32_t)lease) { cancelAsk(a, "lease"); note("drop " + a.name); a.clear(); ui_.dirty = true; continue; }
    if (a.askState == ASK_PENDING && a.askDeadline && (int32_t)(now - a.askDeadline) >= 0) cancelAsk(a, "deadline");
  }
}

bool Api::connected(uint32_t) const { return ui_.count() > 0; }

void Api::info() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["fw"] = FW_VERSION;
  doc["board"] = FW_BOARD;
  doc["channel"] = FW_CHANNEL;
  doc["name"] = ui_.net.bleName;
  doc["host"] = ui_.net.host;
  doc["ip"] = ui_.net.ip;
  doc["ssid"] = ui_.net.ssid;
  doc["rssi"] = WiFi.RSSI();
  doc["w"] = 320;
  doc["h"] = 480;
  doc["session"] = ui_.count() > 0;
  doc["max_agents"] = Ui::MAXAGENTS;
  doc["uptime_s"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["wifi_sleep"] = WiFi.getSleep();  // modem power save: true adds latency
  doc["silent_clients"] = silentClients;  // connections that never sent a request
  doc["requests"] = requests_;
  doc["wifi_drops"] = ui_.net.drops;
  doc["frame_ms"] = ui_.frameMs;
  doc["frame_max_ms"] = ui_.frameMaxMs;
  JsonArray lg = doc["log"].to<JsonArray>();
  for (int i = 0; i < LOGN; i++) {
    const String& e = log_[(logNext_ + i) % LOGN];
    if (e.length()) lg.add(e);
  }
  // In card order (needs you first), with each card's label.
  JsonArray list = doc["agents"].to<JsonArray>();
  int idx[Ui::MAXAGENTS], n = ui_.cardOrder(idx);
  for (int k = 0; k < n; k++) {
    const Agent& a = ui_.agents[idx[k]];
    JsonObject o = list.add<JsonObject>();
    o["label"] = Ui::cardLabel(a);
    o["agent"] = a.name;
    o["project"] = a.project;
    o["line"] = a.line();
    o["status"] = a.status == ST_WORKING ? "working" : a.status == ST_WAITING ? "waiting" : a.status == ST_DONE ? "done"
                  : a.status == ST_ERROR ? "error" : "idle";
    o["asking"] = a.asking;
    o["restored"] = a.restored;
    o["attn"] = a.attn;
    o["sub"] = a.sub;
    o["role"] = a.role;
    o["subagents"] = ui_.children(a);
    o["completed"] = a.completed;
    o["activity"] = a.nmsg;
    if (a.nmsg) o["last"] = a.msgs[0].text;
  }
  if (ui_.update.length()) doc["update"] = ui_.update;
  corsHeaders();
  send(200, doc);
}

// GET /api/ask?id=...  Each agent keeps its newest ask; unknown or replaced ids read as cancelled.
void Api::askStatus() {
  JsonDocument doc;
  doc["ok"] = true;
  String id = server.arg("id");
  doc["id"] = id;
  Agent* a = findAsk(id);
  uint8_t st = a ? a->askState : ASK_CANCELLED;
  doc["state"] = st == ASK_PENDING ? "pending" : st == ASK_ANSWERED ? "answered" : "cancelled";
  if (st == ASK_ANSWERED) {
    doc["choice"] = a->askChoice;
    doc["label"] = a->askLabel;
  }
  send(200, doc);
}

// POST /api/<t> with a JSON body (may be empty).
void Api::route() {
  requests_++;
  String uri = server.uri();
  if (server.method() != HTTP_POST || !uri.startsWith("/api/")) return sendError(404, "not found");
  String body = server.arg("plain");
  // `curl -d` without a JSON content type arrives as a form field named after the body.
  if (!server.hasArg("plain") && server.args() > 0) body = server.argName(0);
  JsonDocument in, out;
  if (body.length() && deserializeJson(in, body)) return sendError(400, "bad json");
  out["ok"] = true;
  int code = apply(uri.c_str() + 5, in, out, millis());
  if (code != 200) return sendError(code, code == 404 ? "unknown command" : "bad request");
  send(200, out);
}

int Api::apply(const char* t, JsonDocument& doc, JsonDocument& out, uint32_t now) {
  static const char* KNOWN[] = {"ping", "state", "msg", "ask", "ask_cancel", "done", "presence", "clear", "bye"};
  bool known = false;
  for (const char* k : KNOWN) known |= !strcmp(t, k);
  if (!known) return 404;
  lastRequest = now;
  ui_.dirty = true;

  if (!strcmp(t, "bye")) {  // this agent leaves (with its subagents, if it's a session); the others stay
    // A session's subagents go with it, even if the session itself never got a card.
    if (!doc["sub"].is<const char*>()) {
      String name = doc["agent"] | "", project = doc["project"] | "";
      for (Agent& c : ui_.agents)
        if (c.used && c.sub.length() && (!name.length() || c.name.equalsIgnoreCase(name)) && (!project.length() || c.project == project)) {
          cancelAsk(c, "session ended");
          c.clear();
        }
    }
    if (Agent* a = resolve(doc, now, false)) {
      cancelAsk(*a, "bye");
      note("bye " + a->name);
      a->clear();
    }
    return 200;
  }
  if (!strcmp(t, "ask_cancel")) {
    const char* id = doc["id"] | "";
    Agent* a = *id ? findAsk(id) : resolve(doc, now, false);
    if (a) cancelAsk(*a, "ask_cancel");
    return 200;
  }
  if (!strcmp(t, "ask")) {
    JsonArray opts = doc["options"].as<JsonArray>();
    if (opts.size() < 1 || opts.size() > Agent::MAXOPT) return 400;
  }
  Agent* ap = resolve(doc, now, true);
  Agent& a = *ap;
  a.lastSeen = now;
  a.restored = false;  // heard from again

  if (!strcmp(t, "state")) {
    // A task starts when we leave the presence/done screens (or the first state of a session).
    if (a.completed || a.status == ST_IDLE || !a.taskStart) a.taskStart = now;
    a.completed = false;
    if (doc["title"].is<const char*>()) a.title = doc["title"].as<const char*>();
    if (doc["status"].is<const char*>()) a.status = parseStatus(doc["status"]);
    a.progress = doc["progress"].is<float>() ? doc["progress"].as<float>() : -1;
  } else if (!strcmp(t, "msg")) {
    a.pushMsg(doc["text"] | "", parseLevel(doc["level"]), doc["ttl"] | 0, now);
  } else if (!strcmp(t, "ask")) {
    JsonArray opts = doc["options"].as<JsonArray>();
    cancelAsk(a, "new ask");
    char id[9];
    snprintf(id, sizeof id, "%08lx", (unsigned long)esp_random());
    // Callers may pick the id (handy in shell scripts); otherwise it's random.
    a.askId = doc["id"].is<const char*>() && strlen(doc["id"]) <= 32 ? doc["id"].as<const char*>() : id;
    a.askPrompt = doc["prompt"] | "";
    a.nopts = 0;
    for (JsonVariant o : opts) a.opts[a.nopts++] = o.as<String>();
    // A question asked outside a task returns to the presence screen afterwards, not to an
    // empty "Working" card.
    if (a.status != ST_WAITING) a.beforeAsk = a.completed ? ST_IDLE : a.status;
    a.completed = false;
    a.asking = true;
    a.status = ST_WAITING;
    uint32_t timeout = doc["timeout"] | 0;
    a.askDeadline = timeout ? now + timeout * 1000 : 0;
    a.askState = ASK_PENDING;
    out["id"] = a.askId;
  } else if (!strcmp(t, "done")) {
    cancelAsk(a, "done");
    a.completed = true;
    a.status = ST_DONE;
    a.summary = doc["summary"] | "";
    a.tokIn = doc["tokens_in"].as<uint32_t>();
    a.tokOut = doc["tokens_out"].as<uint32_t>();
    a.durationS = doc["duration"].is<int>() ? doc["duration"].as<int>() : -1;
  } else if (!strcmp(t, "presence")) {
    a.presence = doc["text"] | "";
    a.attn = doc["attn"] | false;
    if (doc["new_turn"] | false) { a.completed = false; a.status = ST_IDLE; }
    // End of a turn: a task card the agent never closed would stay up forever.
    if ((doc["end_turn"] | false) && a.status == ST_WORKING) a.status = ST_IDLE;
  } else if (!strcmp(t, "clear")) {
    a.nmsg = 0;
  }
  return 200;
}

void Api::note(const String& s) {
  log_[logNext_] = String(millis() / 1000) + "s " + s;
  logNext_ = (logNext_ + 1) % LOGN;
}

void Api::cancelAsk(Agent& a, const char* why) {
  if (a.askState == ASK_PENDING) { a.askState = ASK_CANCELLED; note("ask " + a.askId + " cancelled: " + why); ui_.dirty = true; }
  if (a.asking && a.status == ST_WAITING) a.status = a.beforeAsk;
  a.asking = false;
}

void Api::dismiss(Agent& a) {
  if (!a.used) return;
  cancelAsk(a, "dismissed");
  note("dismissed " + a.name);
  a.clear();
  ui_.dirty = true;
}

void Api::answer(Agent& a, int choice) {
  if (a.askState != ASK_PENDING) return;
  a.askState = ASK_ANSWERED;
  a.askChoice = choice;
  a.askLabel = a.opts[choice];
  ui_.dirty = true;
  note("ask " + a.askId + " answered: " + a.askLabel);
  a.asking = false;
  if (a.status == ST_WAITING) a.status = a.beforeAsk;
}
