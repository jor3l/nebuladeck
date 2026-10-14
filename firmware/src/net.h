#pragma once
#include <Arduino.h>
#include "ui.h"

// WiFi credentials, the BLE setup service and the setup flow; see docs/PROTOCOL.md.
// Writes what the screens need into a NetInfo the UI reads.
class Net {
 public:
  explicit Net(NetInfo& info) : info_(info) {}
  void begin();
  void poll(uint32_t now);
  void startSetup();   // welcome/QR screen + BLE (saved network is kept until a new one works)
  void cancelSetup();  // back to the saved network
  void forget();       // erase the saved network, then setup

 private:
  enum Step : uint8_t { S_WAIT, S_JOINING, S_FAILED, S_DONE };
  void join();
  void online();
  void startBle();
  void stopBle();
  void scan();
  void publishNetworks(int n);
  void publishStatus(const char* state, const char* key = nullptr, const String& val = String());
  void command(const std::string& json, uint32_t now);
  void startJoin(uint32_t now);
  void show(const String& status, const String& detail, uint8_t tone);

  NetInfo& info_;
  bool setup_ = false, ble_ = false, mdns_ = false, scanning_ = false, client_ = false, pendingJoin_ = false;
  uint8_t step_ = S_WAIT;
  uint32_t stepAt_ = 0, lastCheck_ = 0;
  String pass_, joinSsid_, joinPass_;
};
