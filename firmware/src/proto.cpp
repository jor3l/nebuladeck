#include "proto.h"
#include <ArduinoJson.h>

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

void Proto::poll(uint32_t now) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (buf_.length()) handle(buf_, now);
      buf_ = "";
    } else if (c != '\r' && buf_.length() < 2048) {
      buf_ += c;
    }
  }
}

void Proto::handle(const String& line, uint32_t now) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) return;
  const char* t = doc["t"] | "";
  if (!strcmp(t, "hello")) {
    ui_.reset();
    lastPing_ = now;
    Serial.println("{\"t\":\"hello\",\"fw\":\"0.1.0\",\"w\":320,\"h\":480}");
  } else if (!strcmp(t, "ping")) {
    lastPing_ = now;
    Serial.println("{\"t\":\"pong\"}");
  } else if (!strcmp(t, "state")) {
    lastPing_ = now;
    if (doc["project"].is<const char*>()) ui_.project = doc["project"].as<const char*>();
    if (doc["title"].is<const char*>()) ui_.title = doc["title"].as<const char*>();
    if (doc["status"].is<const char*>()) ui_.status = parseStatus(doc["status"]);
    ui_.progress = doc["progress"].is<float>() ? doc["progress"].as<float>() : -1;
  } else if (!strcmp(t, "msg")) {
    ui_.pushMsg(doc["text"] | "", parseLevel(doc["level"]), doc["ttl"] | 0, now);
  } else if (!strcmp(t, "ask")) {
    ui_.askId = doc["id"] | "";
    ui_.askPrompt = doc["prompt"] | "";
    ui_.nopts = 0;
    for (JsonVariant o : doc["options"].as<JsonArray>())
      if (ui_.nopts < Ui::MAXOPT) ui_.opts[ui_.nopts++] = o.as<String>();
    ui_.asking = ui_.nopts > 0;
    if (ui_.asking) ui_.status = ST_WAITING;
  } else if (!strcmp(t, "clear")) {
    ui_.nmsg = 0;
  }
}

void Proto::sendTap(const String& id, int choice, const String& label) {
  JsonDocument doc;
  doc["t"] = "tap";
  doc["id"] = id;
  doc["choice"] = choice;
  doc["label"] = label;
  serializeJson(doc, Serial);
  Serial.println();
}
