#include "AnkiDeckSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <memory>

#include "AnkiSyncStore.h"
#include "HalDisplay.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/AnkiConnectClient.h"

namespace fui = freeink::ui;

AnkiDeckSelectActivity::AnkiDeckSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("AnkiDeckSelect", renderer, mappedInput) {}

void AnkiDeckSelectActivity::onEnter() {
  UiListActivity::onEnter();
  state_ = State::CheckingWifi;
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    onWifiReady(true);
    return;
  }
  wifiActivated_ = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                          [this](const ActivityResult& result) { onWifiReady(!result.isCancelled); });
}

void AnkiDeckSelectActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_) {
    WiFi.disconnect(false);
  }
}

void AnkiDeckSelectActivity::onWifiReady(const bool connected) {
  if (!connected) {
    state_ = State::Error;
    errorMessage_ = "WiFi connection required";
    requestUpdate(true);
    return;
  }
  loadDecks();
}

void AnkiDeckSelectActivity::loadDecks() {
  state_ = State::Loading;
  requestUpdateAndWait();

  const AnkiConnectClient::Error err = AnkiConnectClient::getDeckNames(deckNames_);
  if (err != AnkiConnectClient::OK) {
    state_ = State::Error;
    errorMessage_ = "Could not reach AnkiConnect";
    requestUpdate(true);
    return;
  }

  rebuildRowItems();
  state_ = State::Ready;
  requestUpdate(true);
}

void AnkiDeckSelectActivity::rebuildRowItems() {
  rowItems_.clear();
  rowItems_.resize(deckNames_.size() + 1);

  rowItems_[0].label = "All decks";
  rowItems_[0].actionValue = 0;

  for (size_t i = 0; i < deckNames_.size(); i++) {
    rowItems_[i + 1].label = deckNames_[i].c_str();
    rowItems_[i + 1].actionValue = static_cast<int16_t>(i + 1);
  }
}

const char* AnkiDeckSelectActivity::headerTitle() const { return "Select Deck"; }

void AnkiDeckSelectActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    ANKI_SYNC_STORE.setSelectedDeck("");
  } else if (index - 1 < static_cast<int>(deckNames_.size())) {
    ANKI_SYNC_STORE.setSelectedDeck(deckNames_[index - 1]);
  }
  ANKI_SYNC_STORE.saveToFile();
  finish();
}

void AnkiDeckSelectActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                       static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  syncListViewport(screen, props);
  screen.list(props);
}

void AnkiDeckSelectActivity::render(RenderLock&& lock) {
  if (state_ != State::Ready) {
    renderer.clearScreen();
    const int centerY = renderer.getScreenHeight() / 2;
    const char* msg = state_ == State::CheckingWifi ? "Connecting to WiFi..."
                       : state_ == State::Loading    ? "Loading decks..."
                                                      : errorMessage_.c_str();
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, msg, true, EpdFontFamily::REGULAR);
    renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
    return;
  }
  UiListActivity::render(std::move(lock));
}
