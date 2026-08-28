#include "AnkiSyncStore.h"

#include <ObfuscationUtils.h>

constexpr int CURRENT_SCHEMA_VERSION = 2;

constexpr size_t MAX_PLAUSIBLE_API_KEY_LENGTH = 256;

void AnkiSyncStore::toJson(JsonDocument& doc) const {
  doc["schema_version"] = CURRENT_SCHEMA_VERSION;
  doc["server_url"] = serverUrl;
  doc["selected_deck"] = selectedDeck;
  doc["api_key_obf"] = obfuscation::obfuscateToBase64(apiKey);
}

bool AnkiSyncStore::fromJson(JsonVariantConst doc) {
  const int savedVersion = doc["schema_version"] | 0;
  if (savedVersion != CURRENT_SCHEMA_VERSION) {

    return true;
  }

  serverUrl = doc["server_url"] | "";
  selectedDeck = doc["selected_deck"] | "";

  if (doc["api_key_obf"].is<const char*>()) {
    bool ok = false;
    const std::string decoded = obfuscation::deobfuscateFromBase64(doc["api_key_obf"].as<const char*>(), &ok);
    apiKey = (ok && decoded.size() <= MAX_PLAUSIBLE_API_KEY_LENGTH) ? decoded : "";
  } else {
    apiKey = "";
  }
  return true;
}
