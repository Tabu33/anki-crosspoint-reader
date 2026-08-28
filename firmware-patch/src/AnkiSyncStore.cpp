#include "AnkiSyncStore.h"

#include <ObfuscationUtils.h>

// Bumped whenever this store's saved JSON shape changes (fields added,
// meaning changed, etc.). A real incident: this store's format changed
// several times during development (server-url-only -> +deck -> +obfuscated
// API key) without the saved file ever being cleared between versions on a
// test device, and an old file's data being misinterpreted by a newer
// fromJson() -- specifically the API key deobfuscation step being handed
// data that was never obfuscated that way -- produced a garbage string of
// unpredictable length that fed into an unrelated crash much later. A
// version check makes that class of problem impossible going forward: an
// old or unrecognized file is treated as absent rather than guessed at.
constexpr int CURRENT_SCHEMA_VERSION = 2;

// Defensive upper bound having nothing to do with any real key's expected
// length -- purely a sanity check that a deobfuscation result isn't garbage
// of implausible size, as a second, independent layer beyond the version
// check above (in case a future format change ever reuses this field name
// with a different meaning).
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
    // Missing or from a different version of this store -- don't guess at
    // how to interpret its fields. Falling back to defaults (or the
    // compiled-in DEFAULT_SERVER_URL/DEFAULT_API_KEY, since those are the
    // member initializers this starts from) is safe; misreading stale data
    // is not.
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
