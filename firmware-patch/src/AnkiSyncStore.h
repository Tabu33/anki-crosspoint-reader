#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

/**
 * Singleton class for storing AnkiConnect sync settings on the SD card.
 * serverUrl and selectedDeck are plain settings. apiKey is present for
 * remote/cloud AnkiConnect servers that have API-key authentication enabled
 * (required for any AnkiConnect instance reachable outside a trusted local
 * network) -- left empty, it is simply omitted from requests, which is
 * correct for a local, unauthenticated AnkiConnect instance.
 *
 * DEFAULT_SERVER_URL / DEFAULT_API_KEY below only take effect on a device
 * with no saved settings file yet (a fresh flash, or after clearing the SD
 * card's /.crosspoint folder). Once a value has been saved -- whether by
 * the defaults below or by editing it on-device -- the saved file always
 * wins on subsequent boots; these are just the starting point, not a
 * permanent override. Editing the Settings screen still works normally.
 */
class AnkiSyncStore : public PersistableStore<AnkiSyncStore> {
 private:
  // EDIT THESE TWO LINES with your real values before building, or leave
  // blank (as they are now) to have the device prompt for them via
  // Settings instead.
  static constexpr const char* DEFAULT_SERVER_URL = "";
  static constexpr const char* DEFAULT_API_KEY = "";

  std::string serverUrl = DEFAULT_SERVER_URL;
  std::string selectedDeck;  // empty = all decks
  std::string apiKey = DEFAULT_API_KEY;  // empty = no key sent (local/unauthenticated server)

  AnkiSyncStore() = default;

  friend class PersistableStore<AnkiSyncStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/anki_sync.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  std::string getServerUrl() const { return serverUrl; }
  void setServerUrl(const std::string& url) { serverUrl = url; }
  bool hasServerUrl() const { return !serverUrl.empty(); }

  std::string getSelectedDeck() const { return selectedDeck; }
  void setSelectedDeck(const std::string& deck) { selectedDeck = deck; }

  std::string getApiKey() const { return apiKey; }
  void setApiKey(const std::string& key) { apiKey = key; }
};

#define ANKI_SYNC_STORE AnkiSyncStore::getInstance()
