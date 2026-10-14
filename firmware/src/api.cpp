#include "api.h"
#include <WebServer.h>
#include <WiFi.h>
#include "version.h"

static WebServer server(80);

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

void Api::begin() {
  server.on("/api/info", HTTP_GET, [this] { info(); });
  server.on("/api/ask", HTTP_GET, [this] { askStatus(); });
  server.on("/api/wifi/forget", HTTP_POST, [this] {
    JsonDocument doc;
    doc["ok"] = true;
    send(200, doc);
    delay(100);  // let the reply out before the radio drops
    if (onForgetWifi) onForgetWifi();
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
    String id = server.arg("id");
    bool mine = id.length() && id == ui_.askId;
    uint8_t st = mine ? askState_ : ASK_CANCELLED;
    server.send(200, "text/plain", st == ASK_PENDING ? String("pending") : st == ASK_ANSWERED ? "answered:" + askLabel_ : String("cancelled"));
  });
  // Claude Code hooks: POST /api/hook?event=<hook_event_name>&tool=<tool_name>[&agent=Name].
  // The event body is never needed (it can hold whole files), so hooks send none.
  server.on("/api/hook", HTTP_POST, [this] {
    String ev = server.arg("event");
    JsonDocument in, out;
    const char* t = "presence";
    if (ev == "SessionStart") { in["text"] = "Ready"; in["new_turn"] = true; }
    else if (ev == "UserPromptSubmit") { in["text"] = "Thinking..."; in["new_turn"] = true; }
    else if (ev == "PreToolUse") in["text"] = toolText(server.arg("tool"));
    else if (ev == "Notification") { in["text"] = "Needs you"; in["attn"] = true; }
    else if (ev == "Stop") { in["text"] = "Ready"; in["end_turn"] = true; }
    else if (ev == "SessionEnd") t = "bye";
    else return sendError(400, "unknown event");
    if (server.hasArg("agent")) in["agent"] = server.arg("agent");
    out["ok"] = true;
    apply(t, in, out, millis());
    send(200, out);
  });
  server.onNotFound([this] { route(); });
  server.begin();
}

void Api::poll(uint32_t now) {
  server.handleClient();
  if (askState_ == ASK_PENDING && (!connected(now) || (askDeadline_ && (int32_t)(now - askDeadline_) >= 0))) cancelAsk();
}

void Api::info() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["fw"] = FW_VERSION;
  doc["name"] = ui_.net.bleName;
  doc["host"] = ui_.net.host;
  doc["ip"] = ui_.net.ip;
  doc["ssid"] = ui_.net.ssid;
  doc["rssi"] = WiFi.RSSI();
  doc["w"] = 320;
  doc["h"] = 480;
  doc["session"] = connected(millis());
  if (ui_.update.length()) doc["update"] = ui_.update;
  send(200, doc);
}

// GET /api/ask?id=...  Only the newest ask is kept; any other id reads as cancelled.
void Api::askStatus() {
  JsonDocument doc;
  doc["ok"] = true;
  String id = server.arg("id");
  doc["id"] = id;
  bool mine = id.length() && id == ui_.askId;
  uint8_t st = mine ? askState_ : ASK_CANCELLED;
  doc["state"] = st == ASK_PENDING ? "pending" : st == ASK_ANSWERED ? "answered" : "cancelled";
  if (st == ASK_ANSWERED) {
    doc["choice"] = askChoice_;
    doc["label"] = askLabel_;
  }
  send(200, doc);
}

// POST /api/<t> with a JSON body (may be empty).
void Api::route() {
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

  if (!strcmp(t, "bye")) {
    cancelAsk();
    ui_.reset();
    lastSeen_ = 0;  // straight back to the nebula
    return 200;
  }
  if (!connected(now)) ui_.reset();  // a new session starts clean
  lastSeen_ = now;

  if (!strcmp(t, "state")) {
    ui_.completed = false;
    if (doc["project"].is<const char*>()) ui_.project = doc["project"].as<const char*>();
    if (doc["title"].is<const char*>()) ui_.title = doc["title"].as<const char*>();
    if (doc["status"].is<const char*>()) ui_.status = parseStatus(doc["status"]);
    ui_.progress = doc["progress"].is<float>() ? doc["progress"].as<float>() : -1;
  } else if (!strcmp(t, "msg")) {
    ui_.pushMsg(doc["text"] | "", parseLevel(doc["level"]), doc["ttl"] | 0, now);
  } else if (!strcmp(t, "ask")) {
    JsonArray opts = doc["options"].as<JsonArray>();
    if (opts.size() < 1 || opts.size() > Ui::MAXOPT) return 400;
    cancelAsk();
    char id[9];
    snprintf(id, sizeof id, "%08lx", (unsigned long)esp_random());
    // Callers may pick the id (handy in shell scripts); otherwise it's random.
    ui_.askId = doc["id"].is<const char*>() && strlen(doc["id"]) <= 32 ? doc["id"].as<const char*>() : id;
    ui_.askPrompt = doc["prompt"] | "";
    ui_.nopts = 0;
    for (JsonVariant o : opts) ui_.opts[ui_.nopts++] = o.as<String>();
    ui_.completed = false;
    ui_.asking = true;
    ui_.status = ST_WAITING;
    uint32_t timeout = doc["timeout"] | 0;
    askDeadline_ = timeout ? now + timeout * 1000 : 0;
    askState_ = ASK_PENDING;
    out["id"] = ui_.askId;
  } else if (!strcmp(t, "ask_cancel")) {
    const char* id = doc["id"] | "";
    if (!*id || ui_.askId == id) cancelAsk();
  } else if (!strcmp(t, "done")) {
    cancelAsk();
    ui_.completed = true;
    ui_.status = ST_DONE;
    ui_.summary = doc["summary"] | "";
    ui_.tokIn = doc["tokens_in"].as<uint32_t>();
    ui_.tokOut = doc["tokens_out"].as<uint32_t>();
    ui_.durationS = doc["duration"].is<int>() ? doc["duration"].as<int>() : -1;
  } else if (!strcmp(t, "presence")) {
    if (doc["agent"].is<const char*>()) ui_.agent = doc["agent"].as<const char*>();
    ui_.presence = doc["text"] | "";
    ui_.attn = doc["attn"] | false;
    if (doc["new_turn"] | false) { ui_.completed = false; ui_.status = ST_IDLE; }
    // End of a turn: a task card the agent never closed would stay up forever.
    if ((doc["end_turn"] | false) && ui_.status == ST_WORKING) ui_.status = ST_IDLE;
  } else if (!strcmp(t, "clear")) {
    ui_.nmsg = 0;
  }
  return 200;
}

void Api::cancelAsk() {
  if (askState_ == ASK_PENDING) askState_ = ASK_CANCELLED;
  if (ui_.asking && ui_.status == ST_WAITING) ui_.status = ST_WORKING;
  ui_.asking = false;
}

void Api::answer(int choice) {
  if (askState_ != ASK_PENDING) return;
  askState_ = ASK_ANSWERED;
  askChoice_ = choice;
  askLabel_ = ui_.opts[choice];
  ui_.asking = false;
  ui_.status = ST_WORKING;
}
