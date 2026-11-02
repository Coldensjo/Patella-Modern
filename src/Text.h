// Text.h: CipSoft's table of user interface texts (the "text header").
//
// The header defines its texts with internal linkage, so every object that
// includes it has its own copy of the whole table in .data, 147 pointers in
// this order, and reads only its own copy. Sixteen copies are in the image,
// all with the same pointers (notes/decisions.md, "external.h removed (issue
// #14)"):
//
//   0x5c0150 Communication.obj   reads 126, 127
//   0x5c041c ConsoleWindow.obj   reads 12-19, 119, 120, 122, 146
//   0x5c0678 ContentWindows.obj  reads 0-11, 20-50, 52, 56, 57
//   0x5c08d4 ContextMenus.obj    reads none (link order)
//   0x5c0b34 Control.obj         reads 109-111, 114, 115, 145
//   0x5c0d90 Creatures.obj       reads none (link order)
//   0x5c0fe0 DialogElements.obj  reads none (link order)
//   0x5c123c Dialogs.obj         reads 53-55, 58-72, 74-89, 92-108, 112, 113, 117, 118, 121, 123,
//                                128-133, 135-142
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
// them from memory). The texts are the image's; the header's file name and
// every entry name are ours. Include this header only in .cpp files.

#pragma once

static const char *TextPoisoned = "You are poisoned";                                                                              // 0
static const char *TextOnFire = "You are on fire";                                                                                 // 1
static const char *TextMagicShield = "You are protected by a magic shield";                                                        // 2
static const char *TextParalysed = "You are paralysed";                                                                            // 3
static const char *TextHasted = "You are hasted";                                                                                  // 4
static const char *TextNoLogoutInFight = "You may not logout during a fight";                                                      // 5
static const char *TextDrunk = "You are drunk";                                                                                    // 6
static const char *TextStruckByEnergy = "You are struck by magical energy";                                                        // 7
static const char *TextMayBeAttacked = "You may be attacked by other players";                                                     // 8
static const char *TextPartyMember = "You are a member of a party";                                                                // 9
static const char *TextAttackedUnmarked = "You have attacked an unmarked player";                                                  // 10
static const char *TextKilledTooMany = "You have killed too many unmarked players";                                                // 11
static const char *TextCloseChannel = "Close this Channel (Ctrl+E)";                                                               // 12
static const char *TextShowServerMessages = "Show server messages in this channel (Ctrl+M)";                                       // 13
static const char *TextHideServerMessages = "Hide server messages in this channel (Ctrl+M)";                                       // 14
static const char *TextIgnorePlayers = "Ignore and unignore other players (Ctrl+I)";                                               // 15
static const char *TextOpenNewChannel = "Open new channel (Ctrl+O)";                                                               // 16
static const char *TextAdjustVolume = "Adjust volume";                                                                             // 17
static const char *TextMoveChannelsRight = "Move channels to the right";                                                           // 18
static const char *TextMoveChannelsLeft = "Move channels to the left";                                                             // 19
static const char *TextMaximiseInventory = "Maximise inventory";                                                                   // 20
static const char *TextMinimiseInventory = "Minimise inventory";                                                                   // 21
static const char *TextCloseWindow = "Close this window";                                                                          // 22
static const char *TextMaximiseOrMinimise = "Maximise or minimise window";                                                         // 23
static const char *TextShowHigherContainer = "Show higher container";                                                              // 24
static const char *TextOpenSkillsWindow = "Open skills window (Ctrl+S)";                                                           // 25
static const char *TextCloseSkillsWindow = "Close skills window (Ctrl+S)";                                                         // 26
static const char *TextOpenBattleWindow = "Open battle window (Ctrl+B)";                                                           // 27
static const char *TextCloseBattleWindow = "Close battle window (Ctrl+B)";                                                         // 28
static const char *TextOpenVIPWindow = "Open VIP window (Ctrl+P)";                                                                 // 29
static const char *TextCloseVIPWindow = "Close VIP window (Ctrl+P)";                                                               // 30
static const char *TextLeaveGame = "Leave the game (Ctrl+Q or Ctrl+L)";                                                            // 31
static const char *TextCentreAroundPlayer = "Centre around player";                                                                // 32
static const char *TextShowHigherFloor = "Show higher floor (Alt+PageUp)";                                                         // 33
static const char *TextShowLowerFloor = "Show lower floor (Alt+PageDown)";                                                         // 34
static const char *TextZoomIn = "Zoom in (Alt+End)";                                                                               // 35
static const char *TextZoomOut = "Zoom out (Alt+Home)";                                                                            // 36
static const char *TextScrollAutomap = "Scroll automap (Alt+Cursor)";                                                              // 37
static const char *TextOffensiveFighting = "Offensive fighting";                                                                   // 38
static const char *TextBalancedFighting = "Balanced fighting";                                                                     // 39
static const char *TextDefensiveFighting = "Defensive fighting";                                                                   // 40
static const char *TextStandWhileFighting = "Stand while fighting";                                                                // 41
static const char *TextChaseOpponent = "Chase opponent";                                                                           // 42
static const char *TextCannotAttackUnmarked = "You cannot attack unmarked characters";                                             // 43
static const char *TextCanAttackAny = "You can attack any character";                                                              // 44
static const char *TextStopCurrentAction = "Stop current action (ESC)";                                                            // 45
static const char *TextOpenOptions = "Open options";                                                                               // 46
static const char *TextOpenHelpMenu = "Open help menu";                                                                            // 47
static const char *TextCancelOffer = "Cancel offer";                                                                               // 48
static const char *TextRejectOffer = "Reject offer";                                                                               // 49
static const char *TextAcceptOffer = "Accept offer";                                                                               // 50
static const char *TextReceiveHint = "Receive hint";                                                                               // 51
static const char *TextPercentToGo = "You have %d percent to go";                                                                  // 52
static const char *TextSwitchToDirectX5 = "Switch to DirectX 5 Engine";                                                            // 53
static const char *TextSwitchToDirectX9 = "Switch to DirectX 9 Engine";                                                            // 54
static const char *TextSwitchToOpenGL = "Switch to OpenGL Engine";                                                                 // 55
static const char *TextWaitForCounterOffer = "Please wait for a\ncounteroffer";                                                    // 56
static const char *TextWaitForPartner = "Please wait for \nyour partner to accept.";                                               // 57
static const char *TextAccountNumber = "Account number:";                                                                          // 58
static const char *TextPassword = "Password:";                                                                                     // 59
static const char *TextNoAccountYet = "If you don't have\nan account yet:";                                                        // 60
static const char *TextSelectCharacter = "Select character:";                                                                      // 61
static const char *TextTryingToReconnect = "Trying to reconnect in %d %s.";                                                        // 62
static const char *TextSetNumberOfItems = "Set the number of\nitems you want to move:";                                            // 63
static const char *TextChooseOutfit = "Choose an outfit and determine the colours of the various\nparts of your character's body"; // 64
static const char *TextSelectChannel = "Select a channel:";                                                                        // 65
static const char *TextOpenPrivateChannel = "Open a private message channel:";                                                     // 66
static const char *TextEnterCharacterName = "Please enter a character name:";                                                      // 67
static const char *TextStateRuleViolation =                                                                                        // 68
	"Please state the rule violation in one\n"
	"clear sentence and wait for a reply\n"
	"from a gamemaster.\n"
	"Please note that your message will\n"
	"disappear if you close this channel.";
static const char *TextReportBugsOnly = "Please use this dialog only to report bugs.\nDo not report rule violations here!";        // 69
static const char *TextSelectType = "Select a type:";                                                                              // 70
static const char *TextEnterCharacterName2 = "Please enter a character name:";                                                     // 71
static const char *TextEnterShortDescription = "Enter a short description:";                                                       // 72
static const char *TextEnterFilename = "Please enter a filename:";                                                                 // 73
static const char *TextDownloadingPictures = "Downloading new pictures";                                                           // 74
static const char *TextDownloadingObjectData = "Downloading new object data";                                                      // 75
static const char *TextDownloadingObjectGraphics = "Downloading new object graphics";                                              // 76
static const char *TextClientInfo = "Click here to get\ninfo on the client.";                                                      // 77
static const char *TextShowPreviousHints = "Show previous hints";                                                                  // 78
static const char *TextAskTutor = "Ask a tutor for help";                                                                          // 79
static const char *TextReportRuleViolations = "Report rule violations";                                                            // 80
static const char *TextReadManual = "Read the manual on\nthe website";                                                             // 81
static const char *TextReadFAQ = "Read the FAQ on\nthe website.";                                                                  // 82
static const char *TextAvailableHints = "Available hints:";                                                                        // 83
static const char *TextChangeGeneralOptions = "Change general\ngame options";                                                      // 84
static const char *TextChangeGraphicsOptions = "Change graphics and\nperformance settings";                                        // 85
static const char *TextCustomiseConsole = "Customise the console";                                                                 // 86
static const char *TextChangeNetworkSettings = "Change network settings";                                                          // 87
static const char *TextEditHotkeys = "Edit your hotkey texts";                                                                     // 88
static const char *TextShowMotD = "Show the most recent\nMessage of the Day";                                                      // 89
static const char *TextServerAddress = "Server address:";                                                                          // 90
static const char *TextServerPort = "Server port:";                                                                                // 91
static const char *TextAdvancedSettings = "Advanced settings";                                                                     // 92
static const char *TextAvailableResolutions = "Available resolutions:";                                                            // 93
static const char *TextSelectEngine = "Select graphics engine:";                                                                   // 94
static const char *TextAvailableHotkeys = "Available hotkeys:";                                                                    // 95
static const char *TextEditHotkeyText = "Edit hotkey text:";                                                                       // 96
static const char *TextClientVersion = "Tibia Client\nVersion 7.72 \nCopyright (C) 2002-2006\nCipSoft GmbH\nAll rights reserved."; // 97
static const char *TextOfficialWebsite = "Official Website";                                                                       // 98
static const char *TextCharactersToIgnore = "Characters you wish to ignore:";                                                      // 99
static const char *TextGlobalIgnoreSettings = "Global settings for ignoring messages:";                                            // 100
static const char *TextCharactersToAllow = "Characters you allow to message you:";                                                 // 101
static const char *TextGlobalAllowSettings = "Global settings for allowing messages:";                                             // 102
static const char *TextEnterOneNamePerLine = "Enter one name per line.";                                                           // 103
static const char *TextItIsEmpty = "It is empty.";                                                                                 // 104
static const char *TextItIsCurrentlyEmpty = "It is currently empty.";                                                              // 105
static const char *TextYouReadTheFollowing = "You read the following.";                                                            // 106
static const char *TextYouCanEnterNewText = "You can enter new text.";                                                             // 107
static const char *TextYouReadWrittenBy = "You read the following, written by";                                                    // 108
static const char *TextCharacterListLoading = "Your character list is being loaded. Please wait.";                                 // 109
static const char *TextConnectingGameWorld = "Connecting to the game world. Please wait.";                                         // 110
static const char *TextWaitForCharacterList = "Please wait for your character list.";                                              // 111
static const char *TextSelectType2 = "Select a type:";                                                                             // 112
static const char *TextEnterShortDescription2 = "Enter a short description:";                                                      // 113
static const char *TextLinkCopyWarning =                                                                                           // 114
	"Warning!\n"
	"You are about to copy a link. Keep in mind that unofficial\n"
	"websites may be used to hack your account! Continue?";
static const char *TextNeverGiveAccount = // 115
	"Warning!\n"
	"Never give your account number or your password to anybody!\n"
	"Not even gamemasters or CipSoft members will need this data.\n"
	"People asking you for this information are trying to hack you!";
static const char *TextCriticalError = // 116
	"The Tibia client encountered a critical error and had to be stopped.\r\n"
	"\r\n"
	"A detailed error report has been written to the error file which\r\n"
	"you are asked to submit at your next login.";
static const char *TextClientCrashed = // 117
	"At your last visit in Tibia your client crashed and created a detailed error report.\n"
	"Please help us to improve the game by sending us this report.\n"
	"\n"
	"Error report:";
static const char *TextTellUsWhatYouDid = "Please tell us what you were doing just before the crash:\n";                    // 118
static const char *TextWaitForGamemaster = "Please wait patiently for a gamemaster to reply.";                              // 119
static const char *TextRequestSubmitted = "You have already submitted a request. Please be patient.";                       // 120
static const char *TextRequestAlreadySubmitted = "You have already submitted a request. Please close the current request."; // 121
static const char *TextHelpChannelWelcome =                                                                                 // 122
	"Welcome to the help channel! Feel free to ask questions concerning client controls, "
	"general game play, use of accounts and the official homepage. In-depth questions about "
	"the content of the game will not be answered. Experienced players will be glad to help "
	"you to the best of their knowledge. Keep in mind that this is not a chat channel for "
	"general conversations. Therefore please limit your statements to relevant questions and "
	"answers.";
static const char *TextNoPrivateChannelWithYourself = "You cannot set up a private message channel with yourself.";         // 123
static const char *TextNoBugReportRight = "You are not authorised to send bug reports.";                                    // 124
static const char *TextSelectPlayerToTrade = "Select a player to trade with.";                                              // 125
static const char *TextRequestClosed = "Your request has been closed.";                                                     // 126
static const char *TextChannelClosed = "The channel has been closed. You need to re-join the channel if you get invited.";  // 127
static const char *TextNoPlayerName = "No playername given.";                                                               // 128
static const char *TextNoReason = "No reason has been selected.";                                                           // 129
static const char *TextNoAction = "No action has been selected.";                                                           // 130
static const char *TextNoComment = "You must enter a comment.";                                                             // 131
static const char *TextNoStatement = "No statement has been selected.";                                                     // 132
static const char *TextCannotReportStatement = "You cannot report this statement.";                                         // 133
static const char *TextPermanentlyIgnoring = "You are permanently ignoring players.";                                       // 134
static const char *TextTibiaURL = "http://www.tibia.com/";                                                                  // 135
static const char *TextManualURL = "http://www.tibia.com/guide/?subtopic=manual";                                           // 136
static const char *TextFAQURL = "http://www.tibia.com/guide/?subtopic=faq";                                                 // 137
static const char *TextOptionsURL = "http://www.tibia.com/guide/?subtopic=manual&section=options";                          // 138
static const char *TextGraphicsOptionsURL = "http://www.tibia.com/guide/?subtopic=manual&section=options#graphicsoptions";  // 139
static const char *TextAdvancedGraphicsURL = "http://www.tibia.com/guide/?subtopic=manual&section=options#graphicsoptions"; // 140
static const char *TextConsoleOptionsURL = "http://www.tibia.com/guide/?subtopic=manual&section=options#consoleoptions";    // 141
static const char *TextNetworkOptionsURL = "http://www.tibia.com/guide/?subtopic=manual&section=options#networkoptions";    // 142
static const char *TextAccountURL = "http://www.tibia.com/home/?subtopic=account";                                          // 143
static const char *TextSignupURL = "http://www.tibia.com/home/?subtopic=signup";                                            // 144
static const char *TextRuleViolations = "Rule Violations";                                                                  // 145
static const char *TextReportRuleViolation = "Report Rule Violation";                                                       // 146
