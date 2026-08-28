#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * One due card, ready to display: HTML already stripped to plain text.
 */
struct AnkiCardData {
  int64_t cardId = 0;
  std::string question;
  std::string answer;
};

/**
 * HTTP client for the AnkiConnect add-on (local network, plain HTTP).
 *
 * AnkiConnect listens on http://<host>:8765 by default and speaks a simple
 * JSON-RPC-like protocol: POST {"action": "...", "version": 6, "params": {}}
 * and read back {"result": ..., "error": null|"message"}.
 *
 * Docs: https://foosoft.net/projects/anki-connect/
 */
class AnkiConnectClient {
 public:
  enum Error {
    OK = 0,
    NO_SERVER_URL,
    NETWORK_ERROR,
    SERVER_ERROR,
    JSON_ERROR,
  };

  // Quick reachability check -- calls the "version" action.
  static Error checkConnection();

  // Fetch up to `limit` due cards (query: "is:due"). If `deckName` is
  // non-empty, restricted to that deck only.
  static Error getDueCards(std::vector<AnkiCardData>& outCards, int limit = 20, const std::string& deckName = "");

  // List all deck names.
  static Error getDeckNames(std::vector<std::string>& outNames);

  // Submit a rating for a card. ease: 1=Again, 2=Hard, 3=Good, 4=Easy.
  static Error answerCard(int64_t cardId, int ease);

  // Triggers a normal AnkiWeb sync, the same as clicking the Sync button in
  // the desktop app. Needed for headless setups specifically: Anki normally
  // syncs when a profile is closed, but a container running AnkiConnect
  // keeps the profile open indefinitely, so that trigger never happens on
  // its own. Call this after a review session to actually propagate
  // changes to AnkiWeb (and therefore other devices).
  static Error syncCollection();

  // HTTP status of the most recent request (0 if none was made / transport failed).
  static int lastHttpCode;
};
