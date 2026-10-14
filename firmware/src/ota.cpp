#include "ota.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include "roots.h"
#include "version.h"

static const char* RELEASES = "https://api.github.com/repos/jor3l/nebuladeck/releases/";
static const char* ASSET = "nebuladeck-sc01plus-ota.bin";  // app only; the merged image is for USB
static const uint32_t EVERY_MS = 12UL * 3600 * 1000;

// stable: "v1.2.3" > "1.2.0"? dev: "dev-124-abc" > "dev-123-def"? (by CI run number)
static bool newer(const char* a, const char* b) {
  if (!strncmp(a, "dev-", 4)) return !strncmp(b, "dev-", 4) && atoi(a + 4) > atoi(b + 4);
  int va[3] = {}, vb[3] = {};
  sscanf(a + (*a == 'v'), "%d.%d.%d", &va[0], &va[1], &va[2]);
  sscanf(b + (*b == 'v'), "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
  for (int i = 0; i < 3; i++)
    if (va[i] != vb[i]) return va[i] > vb[i];
  return false;
}

static const bool DEV = !strcmp(FW_CHANNEL, "dev");
static const bool ENABLED = DEV || !strcmp(FW_CHANNEL, "stable");

void Ota::begin() {
  Preferences p;
  p.begin("ota", false);
  skipped_ = p.getString("skip", "");
  p.end();
}

void Ota::poll(uint32_t now, bool online) {
  if (found_) {
    found_ = false;
    url_ = foundUrl_;
    ui_.update = foundTag_;
    ui_.updatePrompt = foundTag_ != skipped_;
    Serial.printf("ota: %s available\n", foundTag_.c_str());
  }
  if (want_) {
    want_ = false;
    install();
  }
  if (ENABLED && online && !checking_ && (int32_t)(now - nextCheck_) >= 0) {
    nextCheck_ = now + EVERY_MS;
    checking_ = true;
    ui_.updateCheck = UC_CHECKING;
    if (xTaskCreate(checkTask, "ota", 8192, this, 1, nullptr) != pdPASS) checking_ = false;
  }
}

void Ota::checkTask(void* self) {
  Ota* o = static_cast<Ota*>(self);
  o->ui_.updateCheck = o->check() ? UC_CURRENT : UC_FAILED;
  o->checking_ = false;
  vTaskDelete(nullptr);
}

// False when GitHub couldn't be asked; true otherwise (found_ says whether there's an update).
bool Ota::check() {
  WiFiClientSecure tls;
  tls.setCACert(GITHUB_ROOTS);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the body can be stream-parsed
  String url = String(RELEASES) + (DEV ? "tags/dev" : "latest");
  if (!http.begin(tls, url)) return false;
  http.setUserAgent("nebuladeck/" FW_VERSION);
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("ota: check failed (%d)\n", code);
    http.end();
    return false;
  }
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return false;
  // The rolling dev prerelease keeps its tag; its name carries the build.
  const char* tag = DEV ? (doc["name"] | "") : (doc["tag_name"] | "");
  if (!newer(tag, FW_VERSION)) return true;
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    if (strcmp(a["name"] | "", ASSET)) continue;
    foundUrl_ = a["browser_download_url"] | "";
    foundTag_ = tag;
    found_ = true;  // published last: poll() reads the strings after this
  }
  return true;
}

void Ota::skip() {
  skipped_ = ui_.update;
  ui_.updatePrompt = false;
  Preferences p;
  p.begin("ota", false);
  p.putString("skip", skipped_);
  p.end();
}

// Blocks until done; reboots on success.
void Ota::install() {
  if (!url_.length()) return;
  if (onInstall) onInstall();
  ui_.updatePrompt = false;
  ui_.updateError = "";
  ui_.updatePct = 0;
  ui_.drawUpdating();
  Serial.printf("ota: installing %s from %s\n", ui_.update.c_str(), url_.c_str());

  WiFiClientSecure tls;
  tls.setCACert(GITHUB_ROOTS);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // github.com -> asset CDN
  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onProgress([this](int done, int total) {
    int pct = total > 0 ? (int)(done * 100LL / total) : 0;
    if (pct != ui_.updatePct) {
      ui_.updatePct = pct;
      ui_.drawUpdating();
    }
  });
  t_httpUpdate_return r = httpUpdate.update(tls, url_);
  // Only failures get here.
  ui_.updateError = r == HTTP_UPDATE_NO_UPDATES ? "No update found" : httpUpdate.getLastErrorString();
  Serial.printf("ota: failed: %s\n", ui_.updateError.c_str());
  ui_.drawUpdating();
  delay(4000);
  ui_.updatePct = -1;
  ui_.updatePrompt = true;  // let them retry or skip
}
