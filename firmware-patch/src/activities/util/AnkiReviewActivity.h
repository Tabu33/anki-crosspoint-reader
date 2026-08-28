#pragma once
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "fontIds.h"
#include "network/AnkiConnectClient.h"

// Live version: fetches due cards from AnkiConnect (desktop Anki app on the
// local network) instead of a hardcoded deck. Flow: ensure WiFi -> fetch due
// cards -> review loop (front/back flip, 4-way grade popup) -> submit each
// rating back to AnkiConnect -> move to next card.
class AnkiReviewActivity : public Activity {
 private:
  enum class State { CheckingWifi, Loading, Ready, NoServerUrl, NoCardsDue, Error };

  std::vector<AnkiCardData> cards;
  size_t currentCard = 0;
  bool showingBack = false;
  State state = State::CheckingWifi;
  std::string errorMessage;
  bool wifiActivated = false;
  bool gradedAnyCardThisSession = false;

  const int fontId = UI_10_FONT_ID;
  OptionPopup gradePopup;

  void nextCard();
  void onWifiReady(bool connected);
  void loadDueCards();
  std::vector<std::string> wrapToLines(const std::string& text, int maxWidth) const;
  void drawWrapped(const std::string& text, int centerY, EpdFontFamily::Style style);
  void drawStatusMessage(const char* message);

 public:
  AnkiReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
