#include "net.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "version.h"
#include <algorithm>
#include <mutex>
#include <vector>

// "nebuladeck" in hex, then the attribute number.
#define SVC_UUID "6e656275-6c61-6465-636b-000000000001"
#define NETS_UUID "6e656275-6c61-6465-636b-000000000002"
#define CTL_UUID "6e656275-6c61-6465-636b-000000000003"
#define STAT_UUID "6e656275-6c61-6465-636b-000000000004"

static const char* HOSTNAME = "nebuladeck";
static const char* SETUP_URL = "https://jor3l.github.io/nebuladeck/?setup=";
static const uint32_t JOIN_MS = 20000;  // give up on a network after this
static const uint32_t DONE_MS = 6000;   // keep BLE up so the page can read the IP

// BLE callbacks run on the NimBLE task; hand writes over to the main loop.
static NimBLECharacteristic *chNets, *chStat;
static std::mutex mtx;
static std::string pendingCmd;
static bool hasCmd = false;
static volatile bool bleClient = false;

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override { bleClient = true; }
  void onDisconnect(NimBLEServer*) override {
    bleClient = false;
    NimBLEDevice::startAdvertising();
  }
};
class CtlCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::lock_guard<std::mutex> lock(mtx);
    pendingCmd = c->getValue();
    hasCmd = true;
  }
};
static ServerCb serverCb;
static CtlCb ctlCb;

void Net::begin() {
  uint64_t mac = ESP.getEfuseMac();
  char id[5];
  snprintf(id, sizeof id, "%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  info_.bleName = String("NebulaDeck-") + id;
  info_.url = String(SETUP_URL) + id;
  info_.host = String(HOSTNAME) + ".local";

  WiFi.persistent(false);  // credentials live in our own Preferences namespace
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  Preferences p;
  p.begin("wifi", false);  // read-write: creates the namespace on first boot (no NOT_FOUND log)
  info_.ssid = p.getString("ssid", "");
  pass_ = p.getString("pass", "");
  p.end();
  if (info_.ssid.length()) join();
  else startSetup();
}

void Net::join() {
  setup_ = false;
  info_.setup = false;
  info_.online = false;
  WiFi.setAutoReconnect(true);
  WiFi.begin(info_.ssid.c_str(), pass_.c_str());
  Serial.printf("wifi: joining %s\n", info_.ssid.c_str());
}

void Net::online() {
  info_.online = true;
  // A link-local IPv6 address lets mDNS answer AAAA queries; without one, resolvers that ask
  // for both (browsers, curl without -4) wait ~5 s for an answer that never comes.
  WiFi.enableIpV6();
  info_.ip = WiFi.localIP().toString();
  if (!mdns_ && MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("nebuladeck", "tcp", 80);
    mdns_ = true;
  }
  WiFi.setSleep(false);  // snappier API; only allowed with BLE off
  Serial.printf("wifi: online at http://%s (%s)\n", info_.host.c_str(), info_.ip.c_str());
}

void Net::poll(uint32_t now) {
  if (!setup_) {
    if (now - lastCheck_ < 500) return;
    lastCheck_ = now;
    bool up = WiFi.isConnected();
    if (up) info_.rssi = WiFi.RSSI();
    if (up && (!info_.online || info_.ip != WiFi.localIP().toString())) online();
    else if (!up && info_.online) info_.online = false;  // auto-reconnect is on
    return;
  }

  if (client_ != bleClient) {
    client_ = bleClient;
    if (step_ == S_WAIT || (step_ == S_FAILED && !client_)) {
      step_ = S_WAIT;
      if (client_) show("Phone connected", "Pick your network on the page", T_BUSY);
      else show("Scan the code to begin", "Bluetooth: " + info_.bleName, T_IDLE);
    }
  }

  if (scanning_) {
    int n = WiFi.scanComplete();
    if (n >= 0 || n == WIFI_SCAN_FAILED) {
      scanning_ = false;
      publishNetworks(n);
      WiFi.scanDelete();
      if (pendingJoin_) startJoin(now);
      else publishStatus("ready");
    }
  }

  std::string cmd;
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (hasCmd) { cmd.swap(pendingCmd); hasCmd = false; }
  }
  if (!cmd.empty()) command(cmd, now);

  if (step_ == S_JOINING) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) {
      Preferences p;
      p.begin("wifi", false);
      p.putString("ssid", joinSsid_);
      p.putString("pass", joinPass_);
      p.end();
      info_.ssid = joinSsid_;
      pass_ = joinPass_;
      info_.ip = WiFi.localIP().toString();
      step_ = S_DONE;
      stepAt_ = now;
      show("Connected", info_.host + "  " + info_.ip, T_OK);
      publishStatus("connected", "ip", info_.ip);
    } else if (now - stepAt_ > JOIN_MS || (now - stepAt_ > 4000 && (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL))) {
      const char* why = s == WL_NO_SSID_AVAIL ? "Network not found" : s == WL_CONNECT_FAILED ? "Wrong password?" : "No answer from the network";
      WiFi.disconnect();
      step_ = S_FAILED;
      show("Could not connect", why, T_ERR);
      publishStatus("failed", "error", why);
    }
  } else if (step_ == S_DONE && now - stepAt_ > DONE_MS) {
    stopBle();
    setup_ = false;
    info_.setup = false;
    WiFi.setAutoReconnect(true);
    online();
  }
}

void Net::command(const std::string& json, uint32_t now) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return;
  const char* c = doc["cmd"] | "";
  if (step_ == S_JOINING || step_ == S_DONE) return;  // busy
  if (!strcmp(c, "scan")) {
    scan();
  } else if (!strcmp(c, "join")) {
    joinSsid_ = doc["ssid"] | "";
    joinPass_ = doc["pass"] | "";
    if (!joinSsid_.length() || joinSsid_.length() > 32 || joinPass_.length() > 64) {
      publishStatus("failed", "error", "Bad network name or password");
      return;
    }
    if (scanning_) pendingJoin_ = true;  // can't join mid-scan
    else startJoin(now);
  }
}

void Net::startJoin(uint32_t now) {
  pendingJoin_ = false;
  step_ = S_JOINING;
  stepAt_ = now;
  WiFi.disconnect();
  WiFi.begin(joinSsid_.c_str(), joinPass_.c_str());
  show("Connecting to " + joinSsid_, "This takes a few seconds", T_BUSY);
  publishStatus("connecting", "ssid", joinSsid_);
}

void Net::startSetup() {
  if (setup_) return;
  setup_ = true;
  step_ = S_WAIT;
  pendingJoin_ = false;
  info_.setup = true;
  info_.online = false;
  info_.canCancel = info_.ssid.length() > 0;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.setSleep(true);  // required while BLE shares the radio
  startBle();
  client_ = false;
  show("Scan the code to begin", "Bluetooth: " + info_.bleName, T_IDLE);
  scan();
  Serial.printf("setup: advertising as %s\n", info_.bleName.c_str());
}

void Net::cancelSetup() {
  if (!setup_ || !info_.ssid.length()) return;
  stopBle();
  WiFi.scanDelete();
  scanning_ = false;
  join();
}

void Net::forget() {
  Preferences p;
  p.begin("wifi", false);
  p.clear();
  p.end();
  info_.ssid = "";
  pass_ = "";
  if (setup_) info_.canCancel = false;
  else startSetup();
}

void Net::show(const String& status, const String& detail, uint8_t tone) {
  info_.status = status;
  info_.detail = detail;
  info_.tone = tone;
}

void Net::scan() {
  if (scanning_) return;
  WiFi.scanDelete();
  scanning_ = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
  publishStatus(scanning_ ? "scanning" : "ready");
}

// [[ssid, rssi, secured], ...] strongest first, deduplicated, under one 512-byte read.
void Net::publishNetworks(int n) {
  struct Ap { String ssid; int rssi; bool secure; };
  std::vector<Ap> aps;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    auto it = std::find_if(aps.begin(), aps.end(), [&](const Ap& a) { return a.ssid == s; });
    int r = WiFi.RSSI(i);
    if (it != aps.end()) { it->rssi = std::max(it->rssi, r); continue; }
    aps.push_back({s, r, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  std::sort(aps.begin(), aps.end(), [](const Ap& a, const Ap& b) { return a.rssi > b.rssi; });
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& a : aps) {
    JsonArray e = arr.add<JsonArray>();
    e.add(a.ssid); e.add(a.rssi); e.add(a.secure ? 1 : 0);
    if (measureJson(doc) > 500) { arr.remove(arr.size() - 1); break; }
  }
  std::string out;
  serializeJson(doc, out);
  if (chNets) chNets->setValue(out);
}

// Notifications only say "changed": they're cut at the MTU, so the page reads the value.
void Net::publishStatus(const char* state, const char* key, const String& val) {
  if (!chStat) return;
  JsonDocument doc;
  doc["state"] = state;
  if (key) doc[key] = val;
  if (!strcmp(state, "connected")) doc["host"] = info_.host;
  if (info_.ssid.length()) doc["saved"] = info_.ssid;
  doc["fw"] = FW_VERSION;
  std::string out;
  serializeJson(doc, out);
  chStat->setValue(out);
  chStat->notify();
}

void Net::startBle() {
  if (ble_) return;
  NimBLEDevice::init(info_.bleName.c_str());
  NimBLEDevice::setMTU(517);
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(&serverCb, false);
  NimBLEService* svc = srv->createService(SVC_UUID);
  chNets = svc->createCharacteristic(NETS_UUID, NIMBLE_PROPERTY::READ);
  chNets->setValue("[]");
  NimBLECharacteristic* ctl = svc->createCharacteristic(CTL_UUID, NIMBLE_PROPERTY::WRITE);
  ctl->setCallbacks(&ctlCb);
  chStat = svc->createCharacteristic(STAT_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);  // the name goes in the scan response
  adv->start();
  ble_ = true;
  publishStatus("idle");
}

void Net::stopBle() {
  if (!ble_) return;
  chNets = chStat = nullptr;
  NimBLEDevice::deinit(true);
  ble_ = false;
  bleClient = false;
  client_ = false;
}
