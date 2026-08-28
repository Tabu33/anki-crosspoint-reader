#include "AnkiConnectClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include "AnkiSyncStore.h"

int AnkiConnectClient::lastHttpCode = 0;

namespace {


std::string removeElementAndContents(std::string s, const std::string& tag) {
  const std::string openPrefix = "<" + tag;
  const std::string closeTag = "</" + tag + ">";
  size_t pos = 0;
  while (true) {
    const size_t openStart = s.find(openPrefix, pos);
    if (openStart == std::string::npos) break;
    const size_t openEnd = s.find('>', openStart);
    if (openEnd == std::string::npos) break;
    const size_t closeStart = s.find(closeTag, openEnd);
    const size_t removeEnd = (closeStart == std::string::npos) ? s.size() : closeStart + closeTag.size();
    s.erase(openStart, removeEnd - openStart);
    pos = openStart;
  }
  return s;
}


std::string extractAnswerOnly(const std::string& rawAnswerHtml) {
  const size_t markerPos = rawAnswerHtml.find("id=answer");
  if (markerPos == std::string::npos) {
    const size_t altPos = rawAnswerHtml.find("id=\"answer\"");
    if (altPos == std::string::npos) return rawAnswerHtml;
    const size_t tagEnd = rawAnswerHtml.find('>', altPos);
    if (tagEnd == std::string::npos) return rawAnswerHtml;
    return rawAnswerHtml.substr(tagEnd + 1);
  }
  const size_t tagEnd = rawAnswerHtml.find('>', markerPos);
  if (tagEnd == std::string::npos) return rawAnswerHtml;
  return rawAnswerHtml.substr(tagEnd + 1);
}

std::string stripHtml(const std::string& rawInput) {
  // Case-insensitive tag names are valid HTML; AnkiConnect's own templates
  // are consistently lowercase, so only that form needs handling here.
  std::string input = removeElementAndContents(rawInput, "style");
  input = removeElementAndContents(input, "script");

  std::string out;
  out.reserve(input.size());
  bool inTag = false;
  for (size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
    if (c == '<') {
      inTag = true;
      continue;
    }
    if (c == '>') {
      inTag = false;
      // <br> and </div> etc. act as line/word breaks so text doesn't run together.
      if (!out.empty() && out.back() != ' ') out += ' ';
      continue;
    }
    if (!inTag) out += c;
  }
  // Collapse the handful of entities Anki's default templates actually emit.
  auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.length(), to);
      pos += to.length();
    }
  };
  replaceAll(out, "&nbsp;", " ");
  replaceAll(out, "&amp;", "&");
  replaceAll(out, "&lt;", "<");
  replaceAll(out, "&gt;", ">");
  replaceAll(out, "&quot;", "\"");
  replaceAll(out, "&#39;", "'");

  // Collapse runs of whitespace left behind by stripped tags.
  std::string collapsed;
  collapsed.reserve(out.size());
  bool lastWasSpace = false;
  for (char c : out) {
    const bool isSpace = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
    if (isSpace && lastWasSpace) continue;
    collapsed += isSpace ? ' ' : c;
    lastWasSpace = isSpace;
  }
  while (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
  while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
  return collapsed;
}


constexpr uint32_t MIN_FREE_FOR_TLS = 35000;
constexpr uint32_t MIN_BLOCK_FOR_TLS = 20000;

bool insufficientHeapForTls() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
  if (freeHeap < MIN_FREE_FOR_TLS || maxAllocHeap < MIN_BLOCK_FOR_TLS) {
    LOG_ERR("ANKICONNECT", "Insufficient heap for TLS handshake: %u bytes free (need %u), %u max alloc (need %u)",
            freeHeap, MIN_FREE_FOR_TLS, maxAllocHeap, MIN_BLOCK_FOR_TLS);
    return true;
  }
  return false;
}


AnkiConnectClient::Error sendRequest(const std::string& action, const JsonDocument& params, JsonDocument& outDoc) {
  AnkiConnectClient::lastHttpCode = 0;

  const std::string serverUrl = ANKI_SYNC_STORE.getServerUrl();
  if (serverUrl.empty()) {
    LOG_DBG("ANKICONNECT", "No server URL configured");
    return AnkiConnectClient::NO_SERVER_URL;
  }

  const bool isHttps = serverUrl.rfind("https://", 0) == 0;
  if (isHttps && insufficientHeapForTls()) {
    return AnkiConnectClient::NETWORK_ERROR;
  }

  JsonDocument reqDoc;
  reqDoc["action"] = action;
  reqDoc["version"] = 6;
  const std::string apiKey = ANKI_SYNC_STORE.getApiKey();
  if (!apiKey.empty()) {
    reqDoc["key"] = apiKey;
  }
  if (!params.isNull()) {
    reqDoc["params"] = params;
  }

  std::string body;
  serializeJson(reqDoc, body);

  freeink::SecureHttpClient http;
  if (!http.begin(serverUrl)) {
    LOG_ERR("ANKICONNECT", "Bad server URL: %s", serverUrl.c_str());
    return AnkiConnectClient::NETWORK_ERROR;
  }

  http.setInsecure();
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  const int status = http.POST(body);
  AnkiConnectClient::lastHttpCode = status;
  if (status < 0) {
    LOG_ERR("ANKICONNECT", "Request failed (action=%s): transport error", action.c_str());
    return AnkiConnectClient::NETWORK_ERROR;
  }
  if (status != 200) {
    LOG_ERR("ANKICONNECT", "Request failed (action=%s): HTTP %d", action.c_str(), status);
    return AnkiConnectClient::SERVER_ERROR;
  }

  const DeserializationError parseErr = deserializeJson(outDoc, http.getString());
  if (parseErr) {
    LOG_ERR("ANKICONNECT", "JSON parse error (action=%s): %s", action.c_str(), parseErr.c_str());
    return AnkiConnectClient::JSON_ERROR;
  }

  if (!outDoc["error"].isNull()) {
    LOG_ERR("ANKICONNECT", "AnkiConnect error (action=%s): %s", action.c_str(),
            outDoc["error"].as<const char*>());
    return AnkiConnectClient::SERVER_ERROR;
  }

  return AnkiConnectClient::OK;
}

}  // namespace

AnkiConnectClient::Error AnkiConnectClient::checkConnection() {
  JsonDocument params;  // no params needed for "version"
  JsonDocument resultDoc;
  return sendRequest("version", params, resultDoc);
}

AnkiConnectClient::Error AnkiConnectClient::getDueCards(std::vector<AnkiCardData>& outCards, int limit,
                                                         const std::string& deckName) {
  outCards.clear();

  // Step 1: find due card IDs, optionally restricted to one deck.
  JsonDocument findParams;
  std::string query = "is:due";
  if (!deckName.empty()) {
    query = "deck:\"" + deckName + "\" is:due";
  }
  findParams["query"] = query;
  JsonDocument findResult;
  Error err = sendRequest("findCards", findParams, findResult);
  if (err != OK) return err;

  JsonArrayConst ids = findResult["result"].as<JsonArrayConst>();
  if (ids.isNull() || ids.size() == 0) {
    LOG_DBG("ANKICONNECT", "No due cards found");
    return OK; 
  }

 
  JsonDocument infoParams;
  JsonArray cardIdsArr = infoParams["cards"].to<JsonArray>();
  int count = 0;
  for (JsonVariantConst id : ids) {
    if (count >= limit) break;
    cardIdsArr.add(id.as<int64_t>());
    count++;
  }

  JsonDocument infoResult;
  err = sendRequest("cardsInfo", infoParams, infoResult);
  if (err != OK) return err;

  JsonArrayConst cardsArr = infoResult["result"].as<JsonArrayConst>();
  for (JsonObjectConst card : cardsArr) {
    AnkiCardData data;
    data.cardId = card["cardId"].as<int64_t>();
    data.question = stripHtml(card["question"].as<const char*>());
    data.answer = stripHtml(extractAnswerOnly(card["answer"].as<const char*>()));
    outCards.push_back(std::move(data));
  }

  LOG_DBG("ANKICONNECT", "Loaded %zu due cards", outCards.size());
  return OK;
}

AnkiConnectClient::Error AnkiConnectClient::answerCard(int64_t cardId, int ease) {
  JsonDocument params;
  JsonArray answers = params["answers"].to<JsonArray>();
  JsonObject answer = answers.add<JsonObject>();
  answer["cardId"] = cardId;
  answer["ease"] = ease;

  JsonDocument resultDoc;
  return sendRequest("answerCards", params, resultDoc);
}

AnkiConnectClient::Error AnkiConnectClient::getDeckNames(std::vector<std::string>& outNames) {
  outNames.clear();
  JsonDocument params;  // no params needed for "deckNames"
  JsonDocument resultDoc;
  const Error err = sendRequest("deckNames", params, resultDoc);
  if (err != OK) return err;

  JsonArrayConst names = resultDoc["result"].as<JsonArrayConst>();
  for (JsonVariantConst name : names) {
    outNames.emplace_back(name.as<const char*>());
  }
  return OK;
}

AnkiConnectClient::Error AnkiConnectClient::syncCollection() {
  JsonDocument params;  // no params needed for "sync"
  JsonDocument resultDoc;
  return sendRequest("sync", params, resultDoc);
}
