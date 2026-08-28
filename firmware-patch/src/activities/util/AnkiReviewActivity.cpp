#include "AnkiReviewActivity.h"

#include <Logging.h>
#include <WiFi.h>

#include "HalDisplay.h"
#include "AnkiSyncStore.h"
#include "activities/network/WifiSelectionActivity.h"

AnkiReviewActivity::AnkiReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("AnkiReview", renderer, mappedInput) {}

void AnkiReviewActivity::onEnter() {
  Activity::onEnter();
  showingBack = false;
  gradedAnyCardThisSession = false;
  state = State::CheckingWifi;
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    LOG_DBG("ANKI", "Already connected to WiFi");
    onWifiReady(true);
    return;
  }

  wifiActivated = true;
  LOG_DBG("ANKI", "Launching WifiSelectionActivity...");
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                          [this](const ActivityResult& result) { onWifiReady(!result.isCancelled); });
}

void AnkiReviewActivity::onExit() {
  Activity::onExit();
  // Anki normally syncs to AnkiWeb when a profile closes -- but a headless
  // server keeps the profile open indefinitely, so that trigger never
  // happens on its own. Explicitly sync here (while WiFi is still up) so
  // grades from this session actually reach AnkiWeb/other devices, not just
  // the server's local collection. Skipped if nothing was graded, to avoid
  // a pointless sync on sessions that never reviewed anything.
  if (gradedAnyCardThisSession) {
    LOG_DBG("ANKI", "Syncing collection to AnkiWeb...");
    const AnkiConnectClient::Error syncErr = AnkiConnectClient::syncCollection();
    if (syncErr != AnkiConnectClient::OK) {
      LOG_ERR("ANKI", "AnkiWeb sync failed (grades are still saved locally on the server)");
    }
  }
  if (wifiActivated) {
    WiFi.disconnect(false);
  }
}

void AnkiReviewActivity::onWifiReady(bool connected) {
  if (!connected) {
    state = State::Error;
    errorMessage = "WiFi connection required";
    requestUpdate(true);
    return;
  }
  loadDueCards();
}

void AnkiReviewActivity::loadDueCards() {
  state = State::Loading;
  requestUpdateAndWait();

  // Small batch size: each card's full HTML/CSS template can be sizeable,
  // and we only ever display one at a time -- a large batch risked exhausting
  // heap while holding both the raw JSON response and the parsed document in
  // memory simultaneously. Running low just triggers another fetch.
  const AnkiConnectClient::Error err =
      AnkiConnectClient::getDueCards(cards, 5, ANKI_SYNC_STORE.getSelectedDeck());

  if (err == AnkiConnectClient::NO_SERVER_URL) {
    state = State::NoServerUrl;
  } else if (err != AnkiConnectClient::OK) {
    state = State::Error;
    // Distinguish the failure instead of one generic message -- this is
    // exactly the kind of ambiguity that cost real time diagnosing the TLS
    // crash earlier; showing the real cause (and HTTP status, if any)
    // avoids repeating that.
    switch (err) {
      case AnkiConnectClient::NETWORK_ERROR:
        errorMessage = "Network error: could not connect to server";
        break;
      case AnkiConnectClient::SERVER_ERROR:
        errorMessage = "Server rejected request (HTTP " + std::to_string(AnkiConnectClient::lastHttpCode) + ")";
        break;
      case AnkiConnectClient::JSON_ERROR:
        errorMessage = "Got a response, but could not parse it";
        break;
      default:
        errorMessage = "Could not reach AnkiConnect";
        break;
    }
  } else if (cards.empty()) {
    state = State::NoCardsDue;
  } else {
    state = State::Ready;
    currentCard = 0;
    showingBack = false;
  }
  requestUpdate(true);
}

std::vector<std::string> AnkiReviewActivity::wrapToLines(const std::string& text, int maxWidth) const {
  std::vector<std::string> lines;
  std::string currentLine;
  std::string word;

  auto flushWord = [&](const std::string& w) {
    if (w.empty()) return;
    std::string candidate = currentLine.empty() ? w : currentLine + " " + w;
    if (renderer.getTextWidth(fontId, candidate.c_str()) <= maxWidth || currentLine.empty()) {
      currentLine = candidate;
    } else {
      lines.push_back(currentLine);
      currentLine = w;
    }
  };

  for (char c : text) {
    if (c == ' ') {
      flushWord(word);
      word.clear();
    } else {
      word += c;
    }
  }
  flushWord(word);
  if (!currentLine.empty()) lines.push_back(currentLine);
  if (lines.empty()) lines.push_back("");
  return lines;
}

void AnkiReviewActivity::drawWrapped(const std::string& text, int centerY, EpdFontFamily::Style style) {
  const int margin = 24;
  const int maxWidth = renderer.getScreenWidth() - (margin * 2);
  const std::vector<std::string> lines = wrapToLines(text, maxWidth);
  const int lineHeight = renderer.getLineHeight(fontId);
  const int totalHeight = static_cast<int>(lines.size()) * lineHeight;
  int y = centerY - (totalHeight / 2);

  for (const auto& line : lines) {
    renderer.drawCenteredText(fontId, y, line.c_str(), true, style);
    y += lineHeight;
  }
}

void AnkiReviewActivity::drawStatusMessage(const char* message) {
  renderer.clearScreen();
  drawWrapped(message, renderer.getScreenHeight() / 2, EpdFontFamily::REGULAR);
  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void AnkiReviewActivity::render(RenderLock&& lock) {
  switch (state) {
    case State::CheckingWifi:
      drawStatusMessage("Connecting to WiFi...");
      return;
    case State::Loading:
      drawStatusMessage("Fetching due cards...");
      return;
    case State::NoServerUrl:
      drawStatusMessage("No AnkiConnect server set. Go to Settings > Anki Sync to set one.");
      return;
    case State::NoCardsDue:
      drawStatusMessage("No cards due right now.");
      return;
    case State::Error:
      drawStatusMessage(errorMessage.empty() ? "Something went wrong." : errorMessage.c_str());
      return;
    case State::Ready:
      break;
  }

  renderer.clearScreen();

  if (cards.empty()) {
    drawStatusMessage("No cards loaded");
    return;
  }

  const AnkiCardData& card = cards[currentCard];
  const int centerY = renderer.getScreenHeight() / 2 - 20;

  if (!showingBack) {
    drawWrapped(card.question, centerY, EpdFontFamily::BOLD);
  } else {
    drawWrapped(card.answer, centerY, EpdFontFamily::REGULAR);
  }

  if (gradePopup.processRender(renderer, mappedInput)) return;

  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}

void AnkiReviewActivity::loop() {
  if (state != State::Ready) {
    // Status screens (loading, errors, "no cards due", etc.) still need to
    // respond to Back -- otherwise landing on one of these leaves the only
    // way out being a full power cycle.
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (gradePopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!showingBack) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingBack = true;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const char* options[] = {"Again", "Hard", "Good", "Easy"};
    const int64_t cardId = cards[currentCard].cardId;
    gradePopup.show("How well did you know this?", options, 4, 2, [this, cardId](int idx) {
      const int ease = idx + 1;  // 0-based index -> AnkiConnect's 1-4 ease scale
      LOG_DBG("ANKI", "Card %lld graded: ease=%d", static_cast<long long>(cardId), ease);
      AnkiConnectClient::answerCard(cardId, ease);
      gradedAnyCardThisSession = true;
      nextCard();
    });
    requestUpdate();
  }
}

void AnkiReviewActivity::nextCard() {
  currentCard++;
  if (currentCard >= cards.size()) {
    // Reviewed everything we fetched -- go fetch what's due now (there may
    // be more, or nothing left).
    loadDueCards();
    return;
  }
  showingBack = false;
  requestUpdate(true);
}
