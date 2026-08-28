#include "AnkiSyncSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <memory>
#include <string>

#include "AnkiDeckSelectActivity.h"
#include "AnkiSyncStore.h"
#include "MappedInputManager.h"
#include "activities/util/AnkiReviewActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

AnkiSyncSettingsActivity::AnkiSyncSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("AnkiSyncSettings", renderer, mappedInput) {
  rowItems_[0].label = tr(STR_ANKI_SERVER_URL);
  rowItems_[0].actionValue = 0;
  rowItems_[1].label = tr(STR_ANKI_API_KEY);
  rowItems_[1].actionValue = 1;
  rowItems_[2].label = tr(STR_ANKI_DECK);
  rowItems_[2].actionValue = 2;
  rowItems_[3].label = tr(STR_ANKI_START_REVIEW);
  rowItems_[3].actionValue = 3;
}

int AnkiSyncSettingsActivity::listCount() const { return MENU_ITEMS; }

const char* AnkiSyncSettingsActivity::headerTitle() const { return tr(STR_ANKI); }

void AnkiSyncSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    // Server URL - prefill with http:// if empty to save typing. Local
    // AnkiConnect is plain HTTP; a cloud/remote server should use https://.
    const std::string currentUrl = ANKI_SYNC_STORE.getServerUrl();
    const std::string prefillUrl = currentUrl.empty() ? "http://" : currentUrl;
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ANKI_SERVER_URL), prefillUrl, 128,
                                                 InputType::Url),
        [this](const ActivityResult& result) {
          if (!result.isCancelled) {
            const auto& kb = std::get<KeyboardResult>(result.data);
            const std::string urlToSave = (kb.text == "http://" || kb.text == "https://") ? "" : kb.text;
            ANKI_SYNC_STORE.setServerUrl(urlToSave);
            ANKI_SYNC_STORE.saveToFile();
          }
        });
  } else if (index == 1) {
    // API Key - only needed for authenticated (e.g. cloud/remote) servers.
    // Leave blank for a local, unauthenticated AnkiConnect instance.
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ANKI_API_KEY),
                                                 ANKI_SYNC_STORE.getApiKey(), 128, InputType::Password),
        [this](const ActivityResult& result) {
          if (!result.isCancelled) {
            const auto& kb = std::get<KeyboardResult>(result.data);
            ANKI_SYNC_STORE.setApiKey(kb.text);
            ANKI_SYNC_STORE.saveToFile();
          }
        });
  } else if (index == 2) {
    // Deck - browse and pick from decks fetched over AnkiConnect.
    startActivityForResult(std::make_unique<AnkiDeckSelectActivity>(renderer, mappedInput),
                            [](const ActivityResult&) {});
  } else if (index == 3) {
    // Start Review - launches the live review screen.
    startActivityForResult(std::make_unique<AnkiReviewActivity>(renderer, mappedInput), [](const ActivityResult&) {});
  }
}

void AnkiSyncSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                       static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  rowValues_[0] = ANKI_SYNC_STORE.getServerUrl();
  if (rowValues_[0].empty()) {
    rowValues_[0] = tr(STR_NOT_SET);
  }
  rowItems_[0].value = rowValues_[0].c_str();

  rowValues_[1] = ANKI_SYNC_STORE.getApiKey().empty() ? tr(STR_NOT_SET) : "******";
  rowItems_[1].value = rowValues_[1].c_str();

  rowValues_[2] = ANKI_SYNC_STORE.getSelectedDeck();
  if (rowValues_[2].empty()) {
    rowValues_[2] = tr(STR_ANKI_ALL_DECKS);
  }
  rowItems_[2].value = rowValues_[2].c_str();

  rowValues_[3] = "";
  rowItems_[3].value = nullptr;

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(MENU_ITEMS);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  syncListViewport(screen, props);
  screen.list(props);
}
