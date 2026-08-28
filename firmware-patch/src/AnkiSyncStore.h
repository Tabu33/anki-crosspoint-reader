#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

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
