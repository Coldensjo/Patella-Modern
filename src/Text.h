// Text.h: CipSoft's table of user interface texts (the "text header").
//
// The header defines its texts with internal linkage, so every object that
// includes it has its own copy of the whole table in .data, 147 pointers in
// this order (142 here: 117 and 118, the error report dialog's texts, are
// gone since Patella-Modern issue #74, and 74-76, the update dialog's, since
// issue #75), and reads only its own copy.
// Sixteen copies are in the image, all with the same pointers
// (notes/decisions.md, "external.h removed (issue #14)"):
//
//   0x5c0150 Communication.obj   reads 126, 127
//   0x5c041c ConsoleWindow.obj   reads 12-19, 119, 120, 122, 146
//   0x5c0678 ContentWindows.obj  reads 0-11, 20-50, 52, 56, 57
//   0x5c08d4 ContextMenus.obj    reads none (link order)
//   0x5c0b34 Control.obj         reads 109-111, 114, 115, 145
//   0x5c0d90 Creatures.obj       reads none (link order)
//   0x5c0fe0 DialogElements.obj  reads none (link order)
//   0x5c123c Dialogs.obj         reads 53-55, 58-72, 74-76 (until #75), 77-89, 92-108, 112, 113, 117 and 118 (until
//                                #74), 121, 123, 128-133, 135-142
//   0x5c1498 DialogWindow.obj    reads none (link order)
//   0x5c1708 GUI.obj             reads none (link order; its DefaultFontColor follows)
//   0x5c1960 Help.obj            reads none (low: Help.obj or Inventory.obj)
//   0x5c1bb0 MainFrm.obj         reads none (low: one of Help, Inventory and MainFrm)
//   0x5c1e0c MainWindow.obj      reads 124, 143
//   0x5c2064 MapWindow.obj       reads 51, 125
//   0x5c22cc Player.obj          reads 116
//   0x5c258c Talk.obj            reads 134
//
// An entry of a copy is at the copy's address plus four times its number.
// The pointers are not const (the copies are in .data and the code reads
// them from memory). The texts are the image's, except 97 (the fork's own, the
// owner's choice, Patella-Modern issue #166), 116 (below) and the links 135-144, which are BuildConfig.h's (issue #169;
// by default the image's); the header's file
// name and every entry name are ours. Include this header only in .cpp files.

#pragma once

#include "BuildConfig.h" // the web links (issue #169)

// GCC and Clang (the golden runner on Linux, issue #91) warn about every text of the table that a file does not read
// (-Wunused-variable: internal linkage); each file reads some, as the original's objects do.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

namespace {

const char *TextPoisoned = "You are poisoned";                                                                              // 0
const char *TextOnFire = "You are on fire";                                                                                 // 1
const char *TextMagicShield = "You are protected by a magic shield";                                                        // 2
const char *TextParalysed = "You are paralysed";                                                                            // 3
const char *TextHasted = "You are hasted";                                                                                  // 4
const char *TextNoLogoutInFight = "You may not logout during a fight";                                                      // 5
const char *TextDrunk = "You are drunk";                                                                                    // 6
const char *TextStruckByEnergy = "You are struck by magical energy";                                                        // 7
const char *TextMayBeAttacked = "You may be attacked by other players";                                                     // 8
const char *TextPartyMember = "You are a member of a party";                                                                // 9
const char *TextAttackedUnmarked = "You have attacked an unmarked player";                                                  // 10
const char *TextKilledTooMany = "You have killed too many unmarked players";                                                // 11
const char *TextCloseChannel = "Close this Channel (Ctrl+E)";                                                               // 12
const char *TextShowServerMessages = "Show server messages in this channel (Ctrl+M)";                                       // 13
const char *TextHideServerMessages = "Hide server messages in this channel (Ctrl+M)";                                       // 14
const char *TextIgnorePlayers = "Ignore and unignore other players (Ctrl+I)";                                               // 15
const char *TextOpenNewChannel = "Open new channel (Ctrl+O)";                                                               // 16
const char *TextAdjustVolume = "Adjust volume";                                                                             // 17
const char *TextMoveChannelsRight = "Move channels to the right";                                                           // 18
const char *TextMoveChannelsLeft = "Move channels to the left";                                                             // 19
const char *TextMaximiseInventory = "Maximise inventory";                                                                   // 20
const char *TextMinimiseInventory = "Minimise inventory";                                                                   // 21
const char *TextCloseWindow = "Close this window";                                                                          // 22
const char *TextMaximiseOrMinimise = "Maximise or minimise window";                                                         // 23
const char *TextShowHigherContainer = "Show higher container";                                                              // 24
const char *TextOpenSkillsWindow = "Open skills window (Ctrl+S)";                                                           // 25
const char *TextCloseSkillsWindow = "Close skills window (Ctrl+S)";                                                         // 26
const char *TextOpenBattleWindow = "Open battle window (Ctrl+B)";                                                           // 27
const char *TextCloseBattleWindow = "Close battle window (Ctrl+B)";                                                         // 28
const char *TextOpenVIPWindow = "Open VIP window (Ctrl+P)";                                                                 // 29
const char *TextCloseVIPWindow = "Close VIP window (Ctrl+P)";                                                               // 30
const char *TextLeaveGame = "Leave the game (Ctrl+Q or Ctrl+L)";                                                            // 31
const char *TextCentreAroundPlayer = "Centre around player";                                                                // 32
const char *TextShowHigherFloor = "Show higher floor (Alt+PageUp)";                                                         // 33
const char *TextShowLowerFloor = "Show lower floor (Alt+PageDown)";                                                         // 34
const char *TextZoomIn = "Zoom in (Alt+End)";                                                                               // 35
const char *TextZoomOut = "Zoom out (Alt+Home)";                                                                            // 36
const char *TextScrollAutomap = "Scroll automap (Alt+Cursor)";                                                              // 37
const char *TextOffensiveFighting = "Offensive fighting";                                                                   // 38
const char *TextBalancedFighting = "Balanced fighting";                                                                     // 39
const char *TextDefensiveFighting = "Defensive fighting";                                                                   // 40
const char *TextStandWhileFighting = "Stand while fighting";                                                                // 41
const char *TextChaseOpponent = "Chase opponent";                                                                           // 42
const char *TextCannotAttackUnmarked = "You cannot attack unmarked characters";                                             // 43
const char *TextCanAttackAny = "You can attack any character";                                                              // 44
const char *TextStopCurrentAction = "Stop current action (ESC)";                                                            // 45
const char *TextOpenOptions = "Open options";                                                                               // 46
const char *TextOpenHelpMenu = "Open help menu";                                                                            // 47
const char *TextCancelOffer = "Cancel offer";                                                                               // 48
const char *TextRejectOffer = "Reject offer";                                                                               // 49
const char *TextAcceptOffer = "Accept offer";                                                                               // 50
const char *TextReceiveHint = "Receive hint";                                                                               // 51
const char *TextPercentToGo = "You have %d percent to go";                                                                  // 52
const char *TextSwitchToDirectX5 = "Switch to DirectX 5 Engine";                                                            // 53
const char *TextSwitchToDirectX9 = "Switch to DirectX 9 Engine";                                                            // 54
const char *TextSwitchToOpenGL = "Switch to OpenGL Engine";                                                                 // 55
const char *TextWaitForCounterOffer = "Please wait for a\ncounteroffer";                                                    // 56
const char *TextWaitForPartner = "Please wait for \nyour partner to accept.";                                               // 57
const char *TextAccountNumber = "Account number:";                                                                          // 58
const char *TextPassword = "Password:";                                                                                     // 59
const char *TextNoAccountYet = "If you don't have\nan account yet:";                                                        // 60
const char *TextSelectCharacter = "Select character:";                                                                      // 61
const char *TextTryingToReconnect = "Trying to reconnect in %d %s.";                                                        // 62
const char *TextSetNumberOfItems = "Set the number of\nitems you want to move:";                                            // 63
const char *TextChooseOutfit = "Choose an outfit and determine the colours of the various\nparts of your character's body"; // 64
const char *TextSelectChannel = "Select a channel:";                                                                        // 65
const char *TextOpenPrivateChannel = "Open a private message channel:";                                                     // 66
const char *TextEnterCharacterName = "Please enter a character name:";                                                      // 67
const char *TextStateRuleViolation =                                                                                        // 68
	"Please state the rule violation in one\n"
	"clear sentence and wait for a reply\n"
	"from a gamemaster.\n"
	"Please note that your message will\n"
	"disappear if you close this channel.";
const char *TextReportBugsOnly = "Please use this dialog only to report bugs.\nDo not report rule violations here!"; // 69
const char *TextSelectType = "Select a type:";                                                                       // 70
const char *TextEnterCharacterName2 = "Please enter a character name:";                                              // 71
const char *TextEnterShortDescription = "Enter a short description:";                                                // 72
const char *TextEnterFilename = "Please enter a filename:";                                                          // 73
const char *TextClientInfo = "Click here to get\ninfo on the client.";                                               // 77
const char *TextShowPreviousHints = "Show previous hints";                                                           // 78
const char *TextAskTutor = "Ask a tutor for help";                                                                   // 79
const char *TextReportRuleViolations = "Report rule violations";                                                     // 80
const char *TextReadManual = "Read the manual on\nthe website";                                                      // 81
const char *TextReadFAQ = "Read the FAQ on\nthe website.";                                                           // 82
const char *TextAvailableHints = "Available hints:";                                                                 // 83
const char *TextChangeGeneralOptions = "Change general\ngame options";                                               // 84
const char *TextChangeGraphicsOptions = "Change graphics and\nperformance settings";                                 // 85
const char *TextCustomiseConsole = "Customise the console";                                                          // 86
const char *TextChangeNetworkSettings = "Change network settings";                                                   // 87
const char *TextEditHotkeys = "Edit your hotkey texts";                                                              // 88
const char *TextShowMotD = "Show the most recent\nMessage of the Day";                                               // 89
const char *TextServerAddress = "Server address:";                                                                   // 90
const char *TextServerPort = "Server port:";                                                                         // 91
const char *TextAdvancedSettings = "Advanced settings";                                                              // 92
const char *TextAvailableResolutions = "Available resolutions:";                                                     // 93
const char *TextSelectEngine = "Select graphics engine:";                                                            // 94
const char *TextAvailableHotkeys = "Available hotkeys:";                                                             // 95
const char *TextEditHotkeyText = "Edit hotkey text:";                                                                // 96
const char *TextClientVersion = "Patella\nVersion 7.72";                                                             // 97
const char *TextOfficialWebsite = "Official Website";                                                                // 98
const char *TextCharactersToIgnore = "Characters you wish to ignore:";                                               // 99
const char *TextGlobalIgnoreSettings = "Global settings for ignoring messages:";                                     // 100
const char *TextCharactersToAllow = "Characters you allow to message you:";                                          // 101
const char *TextGlobalAllowSettings = "Global settings for allowing messages:";                                      // 102
const char *TextEnterOneNamePerLine = "Enter one name per line.";                                                    // 103
const char *TextItIsEmpty = "It is empty.";                                                                          // 104
const char *TextItIsCurrentlyEmpty = "It is currently empty.";                                                       // 105
const char *TextYouReadTheFollowing = "You read the following.";                                                     // 106
const char *TextYouCanEnterNewText = "You can enter new text.";                                                      // 107
const char *TextYouReadWrittenBy = "You read the following, written by";                                             // 108
const char *TextCharacterListLoading = "Your character list is being loaded. Please wait.";                          // 109
const char *TextConnectingGameWorld = "Connecting to the game world. Please wait.";                                  // 110
const char *TextWaitForCharacterList = "Please wait for your character list.";                                       // 111
const char *TextSelectType2 = "Select a type:";                                                                      // 112
const char *TextEnterShortDescription2 = "Enter a short description:";                                               // 113
const char *TextLinkCopyWarning =                                                                                    // 114
	"Warning!\n"
	"You are about to copy a link. Keep in mind that unofficial\n"
	"websites may be used to hack your account! Continue?";
const char *TextNeverGiveAccount = // 115
	"Warning!\n"
	"Never give your account number or your password to anybody!\n"
	"Not even gamemasters or CipSoft members will need this data.\n"
	"People asking you for this information are trying to hack you!";
// 116: the first sentence of the original's text (Patella-Modern issue #74, decision D2: there is no error file).
// The original went on: "\r\n\r\nA detailed error report has been written to the error file which\r\nyou are asked
// to submit at your next login." 117 and 118, the error report dialog's texts, are gone with that dialog.
const char *TextCriticalError = "The Tibia client encountered a critical error and had to be stopped.";
const char *TextWaitForGamemaster = "Please wait patiently for a gamemaster to reply.";                              // 119
const char *TextRequestSubmitted = "You have already submitted a request. Please be patient.";                       // 120
const char *TextRequestAlreadySubmitted = "You have already submitted a request. Please close the current request."; // 121
const char *TextHelpChannelWelcome =                                                                                 // 122
	"Welcome to the help channel! Feel free to ask questions concerning client controls, "
	"general game play, use of accounts and the official homepage. In-depth questions about "
	"the content of the game will not be answered. Experienced players will be glad to help "
	"you to the best of their knowledge. Keep in mind that this is not a chat channel for "
	"general conversations. Therefore please limit your statements to relevant questions and "
	"answers.";
const char *TextNoPrivateChannelWithYourself = "You cannot set up a private message channel with yourself.";        // 123
const char *TextNoBugReportRight = "You are not authorised to send bug reports.";                                   // 124
const char *TextSelectPlayerToTrade = "Select a player to trade with.";                                             // 125
const char *TextRequestClosed = "Your request has been closed.";                                                    // 126
const char *TextChannelClosed = "The channel has been closed. You need to re-join the channel if you get invited."; // 127
const char *TextNoPlayerName = "No playername given.";                                                              // 128
const char *TextNoReason = "No reason has been selected.";                                                          // 129
const char *TextNoAction = "No action has been selected.";                                                          // 130
const char *TextNoComment = "You must enter a comment.";                                                            // 131
const char *TextNoStatement = "No statement has been selected.";                                                    // 132
const char *TextCannotReportStatement = "You cannot report this statement.";                                        // 133
const char *TextPermanentlyIgnoring = "You are permanently ignoring players.";                                      // 134
const char *TextTibiaURL = patella::buildconfig::LinkWebsite;                                                       // 135
const char *TextManualURL = patella::buildconfig::LinkManual;                                                       // 136
const char *TextFAQURL = patella::buildconfig::LinkFAQ;                                                             // 137
const char *TextOptionsURL = patella::buildconfig::LinkOptionsHelp;                                                 // 138
const char *TextGraphicsOptionsURL = patella::buildconfig::LinkGraphicsHelp;                                        // 139
const char *TextAdvancedGraphicsURL = patella::buildconfig::LinkGraphicsHelp;                                       // 140
const char *TextConsoleOptionsURL = patella::buildconfig::LinkConsoleHelp;                                          // 141
const char *TextNetworkOptionsURL = patella::buildconfig::LinkNetworkHelp;                                          // 142
const char *TextAccountURL = patella::buildconfig::LinkAccount;                                                     // 143
const char *TextSignupURL = patella::buildconfig::LinkSignup;                                                       // 144
const char *TextRuleViolations = "Rule Violations";                                                                 // 145
const char *TextReportRuleViolation = "Report Rule Violation";                                                      // 146

} // namespace

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
