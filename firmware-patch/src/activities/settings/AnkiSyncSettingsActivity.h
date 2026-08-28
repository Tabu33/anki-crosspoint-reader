#pragma once

#include <string>

#include "activities/UiListActivity.h"

/**
 * Settings screen for AnkiConnect sync: server URL, optional API key (for
 * authenticated/remote servers), deck filter, and a way to launch a review
 * session. Modeled on KOReaderSettingsActivity.
 */
class AnkiSyncSettingsActivity final : public UiListActivity {
 public:
  explicit AnkiSyncSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  static constexpr int MENU_ITEMS = 4;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::string rowValues_[MENU_ITEMS];
  freeink::ui::ListItem rowItems_[MENU_ITEMS]{};
};
