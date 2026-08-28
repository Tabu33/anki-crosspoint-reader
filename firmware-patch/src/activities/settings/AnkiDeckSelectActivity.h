#pragma once
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Fetches deck names from AnkiConnect (ensuring WiFi first, same as
// AnkiReviewActivity) and shows them as a pickable list, with "All decks"
// as the first entry. Selecting one saves it to AnkiSyncStore and finishes.
class AnkiDeckSelectActivity final : public UiListActivity {
 public:
  explicit AnkiDeckSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void render(RenderLock&&) override;

 private:
  enum class State { CheckingWifi, Loading, Ready, Error };
  State state_ = State::CheckingWifi;
  std::string errorMessage_;
  bool wifiActivated_ = false;

  std::vector<std::string> deckNames_;
  std::vector<freeink::ui::ListItem> rowItems_;

  void onWifiReady(bool connected);
  void loadDecks();
  void rebuildRowItems();

  int listCount() const override { return static_cast<int>(rowItems_.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
};
