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


class AnkiConnectClient {
 public:
  enum Error {
    OK = 0,
    NO_SERVER_URL,
    NETWORK_ERROR,
    SERVER_ERROR,
    JSON_ERROR,
  };


  static Error checkConnection();

  // Fetch up to `limit` due cards (query: "is:due"). If `deckName` is
  // non-empty, restricted to that deck only.
  static Error getDueCards(std::vector<AnkiCardData>& outCards, int limit = 20, const std::string& deckName = "");

  // List all deck names.
  static Error getDeckNames(std::vector<std::string>& outNames);

  // Submit a rating for a card. ease: 1=Again, 2=Hard, 3=Good, 4=Easy.
  static Error answerCard(int64_t cardId, int ease);

  static Error syncCollection();

  // HTTP status of the most recent request (0 if none was made / transport failed).
  static int lastHttpCode;
};
