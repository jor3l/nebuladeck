#pragma once
#include <Arduino.h>
#include "ui.h"

// Saves the agents (status, task, activity, question, summary) to flash and restores them
// after a restart: power loss, a crash, flashing or an update. Times are stored as ages and
// rebased on the new clock. The device has no wall clock, so it can't tell a quick reboot
// from a night unplugged: restored agents get at most RESTORE_LEASE_MS more unless they
// send something.
class Store {
 public:
  static constexpr uint32_t SAVE_EVERY_MS = 5000;       // at most this often while things change
  static constexpr uint32_t RESTORE_LEASE_MS = 600000;  // 10 min for restored agents to check in

  bool begin();
  void load(Ui& ui, uint32_t now, uint32_t leaseMs);
  void poll(Ui& ui, uint32_t now);  // saves when ui.dirty, rate-limited
  void save(const Ui& ui, uint32_t now);

 private:
  bool ok_ = false;
  uint32_t lastSave_ = 0;
};
