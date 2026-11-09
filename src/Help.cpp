// Help.cpp: reconstructed from Tibia.exe 7.72 (0x4a2a00-0x4a444f).
//
// The help system: the help bubble of the help mode (the area and handle of
// the window under the magnifying glass, and its text), and the event helps
// ("hints") shown at certain places and NPCs. The functions without line
// evidence come first, in binary order: the gap after GUI.cpp
// (0x4a2a00-0x4a2b88, Help.cpp's data 0x5ce520-0x5ce538), then the asserted
// functions in line order, then GetCurrentHelpText (0x4a4440, the gap before
// Inventory.cpp). The vector<int> constructor at 0x4a2b90 is Container.h
// code kept in this object.

#include "Help.h"
#include "BuildConfig.h"
#include "Preferences.h"
#include "Text.h" // IWYU pragma: keep (the object's own copy of the text table, see Text.h)
#include "common.h"
#include "types/FixedString.h"
#include <cstring>


// The data (0x5ce520-0x5ced5f) is THelp's (Help.h, issue #107).

// 0x4a2a00
void patella::THelp::SetHelpPosition(int x, int y)
{
	HelpX = x;
	HelpY = y;
}

// 0x4a2a20
void patella::THelp::MoveHelpPosition(int dx, int dy)
{
	HelpX += dx;
	HelpY += dy;
}

// 0x4a2a50
void patella::THelp::SetHelpSize(int Width, int Height)
{
	HelpWidth = Width;
	HelpHeight = Height;
}

// 0x4a2a70
void patella::THelp::SetHelpHandle(int Handle)
{
	const bool Changed = HelpHandle != Handle;
	HelpHandle = Handle;
	if (Changed)
		HelpHandleChanged = true;
}

// 0x4a2a90
void patella::THelp::GetHelpArea(int *x, int *y, int *Width, int *Height) const
{
	*x = HelpX;
	*y = HelpY;
	*Width = HelpWidth;
	*Height = HelpHeight;
}

// 0x4a2ac0
int patella::THelp::GetHelpHandle() const
{
	return HelpHandle;
}

// 0x4a2ad0. Reports a change of the handle once.
bool patella::THelp::GetHelpHandleChanged()
{
	const bool Changed = HelpHandleChanged;
	HelpHandleChanged = false;
	return Changed;
}

// 0x4a2ae0
void patella::THelp::InitHelp()
{
	HelpHandle = 1;
	HelpHandleChanged = false;
	EventHelps.clear();
}

// 0x4a2b00. The next event help to show, 0 if none.
int patella::THelp::GetEventHelp()
{
	if (EventHelps.empty())
		return 0;
	return EventHelps.front();
}

// 0x4a2b20. Takes the next event help from the queue.
int patella::THelp::PopEventHelp()
{
	if (EventHelps.empty())
		return 0;
	const int Number = EventHelps.front();
	EventHelps.erase(EventHelps.begin());
	return Number;
}

// 0x4a2e70, line 354. The help bubble text of a handle: 10-23 parts of the
// screen, 1000+ things in the game world. SKIN numbers are skins of the
// buttons drawn inside the text. Texts without arguments are also passed as
// the format. _snprintf until issue #64, now std::format's language ("%d" is
// "{}"): no text has a brace or another conversion, and the longest (1297
// characters) fits HelpText, so the bytes are the same.
const char *patella::THelp::GetHelpText(int Handle)
{
	switch (Handle) {
	case 10:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Automap<N>\n<br=5>The automap shows a bird\'s-eye view of your surroundings. Only those areas you have already explored are shown, while areas you have not been to yet are covered by dark fog. Your current position is marked by a white cross. To move your character simply click on any spot on the automap using the left mouse button. Your character will try to walk there automatically.\nIt is possible to mark individual spots on the automap by right-clicking onto them. You can choose from a number of different symbols and add comments.\nBy using the buttons next to the automap window you can scroll the automap in any direction:\n<SKIN={}><INDENT=46>Click on the compass rose to scroll the map into the desired direction.<INDENT=0>\n\n<SKIN={}><hspace=3><SKIN={}><INDENT=46>Switch between floors, i.e. levels of the map.<INDENT=0>\n\n<SKIN={}><hspace=3><SKIN={}><INDENT=46>Zoom in or out. By using these buttons you can set the automap to the zoom level of your choice.<INDENT=0>\n<FONT=3>Centre<FONT=2><INDENT=46>A click on this button will centre the automap around your character.<INDENT=0>\nYour automap is saved locally in the files labelled as *.map on your computer. Your automap will not be available if you play on a different computer or if you re-install Tibia into a different directory.", 65, 70, 72, 66, 68);
		break;
	case 11:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Status Bars<N>\n<br=5>These two bars show the condition of your characters\' most important attributes: Their health (red) and their mana (blue), i.e. their magical energy. The bars show the current state of these attributes in relation to their respective maximums, while the numbers behind the bars show the actual amount of hit points and mana your character has.\n<br=5>There are different ways to regain health and mana: you can either eat something, drink special potions such as life or mana fluids, or use magic.");
		break;
	case 12:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Inventory<N>\n<br=5>The inventory shows your equipment. You can see a portrait of your character\'s body split up into a number of individual slots.\nThere are three slots in the left column. From top to bottom, these slots represent:\n<indent=10>Neck, right hand and fingers.<indent=0>\nThe central column contains four slots:\n<indent=10>Head, body, legs and feet.<indent=0>\nFinally, the column to the right comprises:\n<indent=10>Container, left hand, and belt.<indent=0>\n<br=5>Note that items will only be useful if placed in the slots they are designed for. E.g. weapons and shields must be placed in hand slots, while rings must be equipped in the finger slot.\n<br=5>In the bottom left corner you see the amount of soul points (Soul). These are needed to cast rune spells.\n<br=5>In the bottom right corner you see your capacity (Cap). Your character cannot pick up items whose weight exceeds his capacity.\n<br=5>The little field on the bottom shows special character conditions. Their nature is indicated by a corresponding icon. To get further information on these conditions, move the mouse over it and wait. A tooltip will pop up and explain the special condition.\n<br=5>You can minimise your inventory by clicking on the\n<SKIN={}> button.", 138);
		break;
	case 13:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Minimised Inventory<N>\n<br=5>The little window to the left shows your character\'s remaining capacity (Cap), i.e., the amount of additional weight he can carry. You also see your remaining soul points (Soul). They are needed to cast rune spells. They regenerate for a certain time when you kill a monster that yields XP equal or above your XP level. Also, if your character is affected by a special condition a corresponding icon will be displayed here. Place the mouse-pointer on the icon and wait a second to get a tooltip that will explain the special condition\'s meaning.\n<br=5>Even though you cannot see your character\'s equipment as long as the inventory is minimised, you can still pick up items by dragging them onto this window. They will be placed in a suitable slot automatically.\n<br=5>To see your inventory, click on the <SKIN={}> button.", 140);
		break;
	case 14:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Combat Controls and Other Buttons<N>\n<br=5>This area contains some buttons to control your combat tactics as well as two buttons with other functions.\n<br=5><SKIN={}><hspace=3><SKIN={}><hspace=3><SKIN={}><INDENT=73>Offensive, balanced and defensive fighting. These buttons determine how aggressively a character engages in combat, which will affect their general performance on the battlefield. E.g. characters who take an offensive stance in combat will hit harder, but they will also suffer more damage.<INDENT=0>\n<SKIN={}><hspace=3><SKIN={}><INDENT=73>Stand still or chase your opponent. If you use a distance fighting weapon, you should probably select \'stand\'. If you prefer melee, \'chase\' may be the right choice.<INDENT=0>\n<SKIN={}><hspace=3><SKIN={}><INDENT=73>Secure/Normal mode. In secure mode you cannot attack unmarked characters. The flat hand indicates secure mode, while the fist indicates normal mode.<INDENT=0>\n<FONT=3>Stop<FONT=2><INDENT=73>Stop fighting, walking, or any object handling at all immediately.<INDENT=0>\n<FONT=3>Options<FONT=2><INDENT=73>This button opens a menu where you can set several options for the program\'s behaviour.<INDENT=0>\n<FONT=3>Help<FONT=2><INDENT=73>This button opens a menu where you can get help in several different ways.<INDENT=0>", 103, 105, 107, 109, 111, 113, 114);
		break;
	case 15:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Combat Controls and Other Buttons<N>\n<br=5>This area contains some buttons to control your combat tactics as well as two buttons with other functions.\n<br=5><SKIN={}><hspace=3><SKIN={}><hspace=3><SKIN={}><INDENT=73>Offensive, balanced and defensive fighting. These buttons determine how aggressively a character engages in combat, which will affect their general performance on the battlefield. E.g. characters who take an offensive stance in combat will hit harder, but they will also suffer more damage.<INDENT=0>\n<SKIN={}><hspace=3><SKIN={}><INDENT=73>Stand still or chase your opponent. If you use a distance fighting weapon, you should probably select \'stand\'. If you prefer melee, \'chase\' may be the right choice.<INDENT=0>\n<SKIN={}><hspace=3><SKIN={}><INDENT=73>Secure/Normal mode. In secure mode you cannot attack unmarked characters. The flat hand indicates secure mode, while the fist indicates normal mode.<INDENT=0>\n<FONT=3>Stop<FONT=2><INDENT=73>Stop fighting, walking, or any object handling at all immediately.<INDENT=0>\n<SKIN={}><INDENT=73>This button opens a menu where you can set several options for the program\'s behaviour.<INDENT=0>\n<SKIN={}><INDENT=73>This button opens a menu where you can get help in several different ways.<INDENT=0>", 103, 105, 107, 109, 111, 113, 114, 115, 117);
		break;
	case 16:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Control Buttons<N>\n<br=5>This section of the screen contains three buttons that serve to open further subwindows as well as the \'Logout\' button. To get further information on any of these subwindows open it and move the magnifying glass onto it.\n<br=5><FONT=3>Skills<FONT=2><INDENT=50>In this subwindow you will find important character values such as experience and skill levels.<INDENT=0>\n<FONT=3>Battle<FONT=2><INDENT=50>The battle window contains a list of all characters and monsters that are currently within battle range. You can use this window to single out enemies for attack.<INDENT=0>\n<FONT=3>VIP<FONT=2><INDENT=50>The VIP list is an editable list of characters that are of special interest to you. Characters that are currently online are highlighted by green writing.<INDENT=0>\n<FONT=3>Logout<FONT=2><INDENT=50>Click on this button to logout. Note that it is not possible to logout in certain places or whenever the character is affected by a logout block.<INDENT=0>");
		break;
	case 17:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Skills<N>\n<br=5>This window shows important information about your character, such as your experience, level, hit points, mana, capacity and magic level. Also you can check the levels of your character\'s skills, like fishing, shielding and handling of various weapons. The skillbars show the percentages that are needed to the next advancement of each skill or to the next level. By right-clicking on this window you can turn on and off individual or all skill bars.");
		break;
	case 18:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Battle<N>\n<br=5>This window contains a list of all creatures that are currently within battle range. You can single out any creature on this list for attack by clicking on its name using the left mouse button. You can also choose to follow creatures or characters that are displayed on this list. To do so open a context menu by right-clicking on the respective name and select \'follow\'.\n<br=5>Creatures or characters that are attacked are marked by a red frame, while those who are followed are marked by a green frame. The creature you are aiming at using the mouse-pointer is marked by a white frame.");
		break;
	case 19:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>VIP<N>\n<br=5>This window contains an editable list of players that are of special interest to you. Character names listed here are highlighted by green writing whenever the respective player is online.\n<br=5>You can add players to your list by using the context menu on the map, in the battle window, or in the VIP window.\n<br=5>By selecting \'Edit ...\' you can add further information on characters. For example, you can sort names by various factors, add notes and icons, or arrange to be notified whenever the marked character logs in.\n<br=5>Context menus can also be used to select additonal actions such as sending private messages to players, to  remove players from the list or to sort the list by various criteria.");
		break;
	case 20:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Open Containers<N>\n<br=5>Whenever you open a container, its contents are displayed in this window. You can put items into containers or remove them by using drag & drop, i.e. you left-click at the item you want to move and drag it onto the container, keeping the mouse-button pressed. If the item is placed on a container that is stored within the open container, it will be placed in there; otherwise it will simply be moved to the first free slot of the current container.\n<br=5>If a container is stored within another container a <SKIN={}>\nbutton will be displayed. Use this button to switch between containers. To move items from a subordinate to the main container open the context menu and select \'Move up\'.", 126);
		break;
	case 21:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Trade<N>\n<br=5>If you start a trade with another player, you will see both your own as well as your trading partner\'s offer displayed in this window, provided they have already been submitted.\n<br=5>You can now inspect the items on offer by clicking onto them with either mouse button. Any contents of containers will be extracted to make sure both trading partners can see exactly what they will get if they accept the trade.\n<br=5>If you agree with your partner\'s offer, click on <FONT=3> Accept <FONT=2>. <FONT=3> Reject <FONT=2> will abort the trade.\n<br=5>The trade will be completed if both partners accept, and if both of them possess enough capacity to carry the new items they are about to buy.");
		break;
	case 22:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Console<N>\n<br=5>This area is reserved for communication with other players. Communication is organised into several different channels. The default channel contains all public communication that is taking place in the immediate vicinity.\n<br=5>Any recent messages on the active channel are displayed in the large box in the centre. You can enter your own messages in the entry field at the bottom of the box. Simply type whatever you wish to say and confirm by pressing Return.\n<br=5>On the default channel, you can modify your character\'s speech volume by using the button in the bottom left corner:\n<SKIN={}> say, <SKIN={}> whisper, <SKIN={}> yell.\n<br=7>Alternatively, you can modify the speech volume by placing #S, #W, or #Y to \'say\', \'whisper\', or \'yell\' in front of your message.\n<br=5>Placing *Name* in front of a message will cause it to be sent directly to the designated player regardless of which channel is active. It is possible to attach tildes (~) to incomplete names in order to have them completed automatically.\n<br=5>For a detailed description of communication in the game please take a look at the manual.", 197, 199, 201);
		break;
	case 23:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Console Channels<N>\n<br=5>This section of the screen is reserved for communication with other players. Communication is organised into several different channels. The default channel contains all public communication that is taking place in the immediate vicinity.\n<br=5>In addition to this channel you can open private channels to message other players or join public channels. To switch between channels click on the tab at the top of the respective channel or use the TAB key.\n<br=5>If you wish to open a new channel, click on the\n<SKIN={}> button, then choose an existing channel in the following dialog or open a private channel by entering a character\'s name. \n<br=5>Clicking on the <SKIN={}> button will open a dialog in which you can either ignore characters or allow them to message you while you are ignoring all other private messages.\n<br=5>To close channels click on the <SKIN={}> button. You can also select whether server messages  should be displayed in each channel by clicking on the\n<SKIN={}> button.\n<br=5>For a detailed description of communication please take a look at the manual.", 195, 192, 188, 190);
		break;
	case 1000:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Creatures<N>\n<br=5>There are three types of creatures: Player characters, NPCs, and monsters.\n<br=5>NPCs can help you by selling useful items to you or by sending you on exciting quests. To start a conversation with NPCs move close to them and say \'Hello\'.\n<br=5>Monsters are your enemies! Fight them! Whenever you defeat one you will get experience and possibly some loot. But watch out! Initially, many monsters will be much stronger than you!");
		break;
	case 1001:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Your Character<N>\n<br=5>This is your character. You can move around by using the cursor keys or by left-clicking on a spot in the game window or in the automap.\n<br=5>To change your outfit, right-click on your character and select \'Set outfit\' from the context menu.");
		break;
	case 1002:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Containers<N>\n<br=5>A container can hold other items. To check its contents right-click on a container and select \'Open\' from the context menu. A window will pop up in which the container\'s contents will be shown. You can move items into or out of the container by using drag & drop.");
		break;
	case 1100:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Ladders<N>\n<br=5>By using ladders you can climb up to the next floor. To climb a ladder right-click on the field that is directly in front of it and select \'Use\' from the context menu.");
		break;
	case 1101:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Sewer Grates<N>\n<br=5>It is possible to enter the sewers through sewer grates. To enter the sewers right-click on a sewer grate and select \'Use\' from the context menu. Watch out, though, for the sewers are teeming with rats! Also, make sure you have a light source such as a torch with you.");
		break;
	case 1102:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Dungeon Floor<N>\n<br=5>Spots like this one indicate that there is a hole in the ceiling. You can climb up through holes using a rope. To do so right-click on the rope, then move the cursor on the spot and click on the left mouse button.");
		break;
	case 1103:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Levers<N>\n<br=5>Levers typically trigger some event close to your character. To pull a lever right-click on it and select \'Use\' from the context menu.");
		break;
	case 1104:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Doors<N>\n<br=5>To open or close a door right-click on it and select \'Use\' from the context menu. Keep in mind that you need a key to open locked doors.");
		break;
	case 1105:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Special Doors<N>\n<br=5>This door belongs to a private house. Only house owners and characters who have their explicit permission are allowed to open private houses. Please see the manual on our website for further information on houses.");
		break;
	case 1106:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Stairs<N>\n<br=5>To climb stairs just move your character onto them.");
		break;
	case 1107:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Mailboxes<N>\n<br=5>You can use mailboxes to send letters or parcels to yourself or to any other player. To send a letter or parcel simply move it onto the mailbox. It will be sent to the addressee\'s depot immediately. You can buy letters and parcels in every city that has a depot.");
		break;
	case 1108:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Depot boxes<N>\n<br=5>Whenever you enter the field in front of a depot box, the contents of your personal depot are loaded into it. As a free player, you can store up to 1,000 objects in your box, regardless of their size and weight. Note that if you have a Premium Account, the amount of items you may store in depot boxes is 2000. Once you move away from the box, all contents are saved automatically, and no other player can get access to your depot.");
		break;
	case 1109:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Dustbins<N>\n<br=5>Dustbins can absorb unlimited amounts of waste. Be careful, though, because any item you move into a dustbin is lost forever.");
		break;
	case 1110:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Stone Piles<N>\n<br=5>It appears that there is some sort of opening underneath this pile of stones. In order to find out what is hidden underneath you need a shovel to remove the debris. Right-click on the shovel, then select \'use with...\' and click on the pile of stones with the left mouse button.");
		break;
	case 1111:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Signs<N>\n<br=5>To read a sign right-click on it and select \'Look\' from the context menu.");
		break;
	case 1112:
		TStringBuffer(HelpText, sizeof(HelpText)).Format("<B>Books and Scrolls<N>\n<br=5>To read or edit a book right-click on it and select \'Use\' from the context menu.");
		break;
	default:
		TERROR1("unknown handle", Handle);
	}
	return HelpText;
}

// 0x4a32e0, line 369. Queues an event help that was not shown before.
void patella::THelp::AddEventHelp(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,1,EVENTHELPS), Number);
	if (!IsKnownHint(Number)) {
		AddKnownHint(Number);
		EventHelps.push_back(Number);
	}
}

// 0x4a3460, line 381. Event helps for talking to the NPCs of Rookgaard.
// Every name is tested on its own.
void patella::THelp::CheckEventHelpName(const char *Name)
{
	PATELLA_ASSERT(Name!=nullptr);
	if (strcmp(Name, "Cipfried") == 0)
		AddEventHelp(2);
	if (strcmp(Name, "Seymour") == 0)
		AddEventHelp(4);
	if (strcmp(Name, "Obi") == 0)
		AddEventHelp(5);
	if (strcmp(Name, "Norma") == 0)
		AddEventHelp(5);
	if (strcmp(Name, "Al Dee") == 0)
		AddEventHelp(5);
	if (strcmp(Name, "Tom") == 0)
		AddEventHelp(5);
	if (strcmp(Name, "Dixi") == 0)
		AddEventHelp(5);
	if (strcmp(Name, "Lily") == 0)
		AddEventHelp(5);
}

// 0x4a3690. Event helps for places: the Rookgaard crossroad (3), the
// Rookgaard underground (6), the arrival on the mainland (11) and the
// depots (12). Written as independent tests in an order that gives the
// binary's results (the compiler merged the tests that share an x and
// returned early where no later test can match); the bounds are the
// binary's, exclusive where written with < and >.
void patella::THelp::CheckEventHelpPosition(int x, int y, int z)
{
	if (x > 0x7d5d && x < 0x7d65 && y == 0x7dd5 && z == 7)
		AddEventHelp(3);
	if (x > 0x7cbf && x < 0x7dc0 && y > 0x7d3f && y < 0x7e00 && z == 8)
		AddEventHelp(6);
	if (x > 0x7e6d && x < 0x7e75 && y > 0x7ded && y < 0x7df5 && z == 7)
		AddEventHelp(11);
	if (x > 0x7e64 && x < 0x7e6c && y > 0x7c22 && y < 0x7c2a && z == 7)
		AddEventHelp(11);
	if (x > 0x81bd && x < 0x81c5 && y > 0x7c42 && y < 0x7c4a && z == 8)
		AddEventHelp(11);
	if (x > 0x80b9 && x < 0x80c1 && y > 0x7d48 && y < 0x7d50 && z == 7)
		AddEventHelp(11);
	if (x > 0x7e57 && x < 0x7e5b && y == 0x7dda && z == 7)
		AddEventHelp(12);
	if (x == 0x7e55 && y > 0x7ddc && y < 0x7de0 && z == 7)
		AddEventHelp(12);
	if (x == 0x7e55 && y > 0x8044 && y < 0x8048 && z == 7)
		AddEventHelp(12);
	if (x == 0x7e55 && y >= 0x8044 && y <= 0x8046 && z == 6)
		AddEventHelp(12);
	if (x > 0x7e4e && x < 0x7e53 && y == 0x7c2b && z == 7)
		AddEventHelp(12);
	if (x == 0x7e53 && y >= 0x7c27 && y <= 0x7c29 && z == 7)
		AddEventHelp(12);
	if (x > 0x818e && x < 0x8192 && y == 0x7c45 && z == 8)
		AddEventHelp(12);
	if (x == 0x808e && y == 0x7d4b && z == 7)
		AddEventHelp(12);
	if (x == 0x808e && y == 0x7d4e && z == 7)
		AddEventHelp(12);
	if (x == 0x8097 && y == 0x7d48 && z == 7)
		AddEventHelp(12);
	if (x == 0x80a3 && y >= 0x7d4a && y <= 0x7d4e && z == 7)
		AddEventHelp(12);
	if (x == 0x80fc && y == 0x7d3c && z == 7)
		AddEventHelp(12);
	if (x == 0x7f85 && y == 0x7ca3 && z == 8)
		AddEventHelp(12);
	if (x == 0x7f84 && y >= 0x7cac && y <= 0x7cad && z == 8)
		AddEventHelp(12);
	if (x > 0x7fa7 && x < 0x7fac && y == 0x7bc3 && z == 7)
		AddEventHelp(12);
	if (x > 0x7fa7 && x < 0x7fab && y == 0x7bc9 && z == 7)
		AddEventHelp(12);
	if (x > 0x81bc && x < 0x81c0 && y == 0x7ec8 && z == 8)
		AddEventHelp(12);
	if (x > 0x8164 && x < 0x8168 && y == 0x8053 && z == 7)
		AddEventHelp(12);
	if (x > 0x8164 && x < 0x8168 && y == 0x8044 && z == 7)
		AddEventHelp(12);
	if (x == 0x8166 && y == 0x8051 && z == 6)
		AddEventHelp(12);
	if (x > 0x8164 && x < 0x8168 && y == 0x8046 && z == 6)
		AddEventHelp(12);
	if (x == 0x8161 && y >= 0x804b && y <= 0x804c && z == 6)
		AddEventHelp(12);
	if (x == 0x816c && y >= 0x804b && y <= 0x804c && z == 6)
		AddEventHelp(12);
	if (x > 0x8165 && x < 0x8168 && y == 0x804f && z == 5)
		AddEventHelp(12);
	if (x > 0x7f6d && x < 0x7f79 && y == 0x7fec && z == 7)
		AddEventHelp(12);
	if (x == 0x7e46 && y >= 0x8042 && y <= 0x8044 && z == 7)
		AddEventHelp(12);
	if (x > 0x7e4e && x < 0x7e52 && y == 0x8050 && z == 7)
		AddEventHelp(12);
	if (x > 0x7e4e && x < 0x7e52 && y == 0x8050 && z == 6)
		AddEventHelp(12);
}

// 0x4a3b60, lines 571-602
const char *patella::THelp::GetEventHelpTitle(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,1,EVENTHELPS), Number);
	switch (Number) {
	case 1:
		return "First Login";
	case 2:
		return "NPC Cipfried";
	case 3:
		return "Rookgaard Crossroad";
	case 4:
		return "NPC Seymour";
	case 5:
		return "Buying & Selling";
	case 6:
		return "Underground";
	case 7:
		return "First Experience Points";
	case 8:
		return "Advancing to Level 2";
	case 9:
		return "First Skill Advance";
	case 10:
		return "Reaching Level 8";
	case 11:
		return "Continent";
	case 12:
		return "Depot";
	case 13:
		return "Player Killing";
	case 14:
		return "Level 20 and Security Information";
	default:
		TERROR1("unknown hint", Number);
	}
}

namespace {

// The first hint names the site (BuildConfig.h, issue #169): the original's text with SiteName in place of its site.
constexpr const char *WelcomeBeforeSite = "<b>Welcome to Tibia!\n<br=7><n>Greetings wanderer! You are about to enter the mysterious world of Tibia. Fight fearsome monsters, find untold treasures and make your character a true hero of the land! There are many different ways to play Tibia, but keep in mind that there are many others players out there who want to enjoy the game just like you, so please play fair and follow the Tibia Rules at all times.\n<br=7>To move your character use the cursor keys or click on a spot on the game window or in the automap. Right-clicking on items, monsters, characters or simply on the map will open context menus in which you can select appropriate actions such as attacking or using things. Items can be moved around by using drag&drop.\n<br=7>If you need assistance, click on the \'Help\' button to the right. There are several help options to choose from. For instance, the \'Client Help\' will provide you with information on important objects on the map or parts of the client program. Also, a flashing exclamation mark will appear in the top right corner from time to time to inform you that further hints like this one you are reading right now are available. To review previous hints click on \'Help\', then \'Hints\'. For detailed information please read the manual and the \'FAQ\' on our offical website ";
constexpr const char *WelcomeAfterSite = ".";
constexpr auto WelcomeHelpText = patella::buildconfig::ComposeText<patella::buildconfig::ComposedSize(WelcomeBeforeSite,
	patella::buildconfig::SiteName, WelcomeAfterSite)>(WelcomeBeforeSite, patella::buildconfig::SiteName, WelcomeAfterSite);

} // namespace

// 0x4a3fd0, lines 610-691
const char *patella::THelp::GetEventHelpText(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,1,EVENTHELPS), Number);
	switch (Number) {
	case 1:
		return WelcomeHelpText.data();
	case 2:
		return "You have met an NPC (Non-Player-Character) called Cipfried. To talk to NPCs or other players that are close to you type any text you wish to say on the keyboard and press Enter. You will find your conversations are logged in the lower section of the adventure screen. This section of the screen is called the \'Default Console\'.\n<br=5>To start a conversation with Cipfried say \'hello\'. NPCs react on certain keywords. For example, if you would like to find out more an NPCs you should ask him about his \'job\' or \'name\'.\n<br=5>Note that Cipfried can give you certain \'hints\', and he can \'heal\' you if your are heavily wounded or poisoned. To find out more you should try out further keywords. Conversations are generally ended by saying \'bye\'.";
	case 3:
		return "You have taken your first steps into the village of Rookgaard. To the south you can see the temple where you have started out. Cipfried, the kind priest living there, will gladly heal some of your wounds whenever you are seriously injured. It is a good idea to visit the academy now. Go north until you reach a large building. The Academy\'s famous library contains many important books on Tibia\'s history and on its inhabitants. Also, Seymour, the Academy\'s master, is happy to chat with all new citizens.\n<br=7>To the east you will find Obi\'s shop. The old smith sells and buys weapons, armors, and equipment. Moreover, there are further shops in the village you should take a look at.\n<br=7>To get an idea of what combat is like it is a good idea to look for some animals to hunt. Follow the path to the west or the east in order to enter the woods close to the village. Don\'t worry - you won\'t run into any dangerous enemies there. Once you feel prepared for your first battle you should start exploring the village sewers. You can enter them through the sewer grates.";
	case 4:
		return "The academy is the village\'s main building. It is run by Seymour. Seymour always has some \'mission\' for new Tibians. Please read the hints on the boards.\n<br=7>The library contains important books concerning various aspects of Tibia, including its history and its fauna. A small training centre has been set up in the cellar where new players can try out some basic elements of the game.";
	case 5:
		return "In this shop you can trade with an NPC. Just greet him or her with a hearty \'hello\' to start a conversation. You will receive a list of the items that are in stock if you ask shop owners about their \'offers\'. If you are interested in buying an object type \'buy <name of object>\'. The shop owner will tell you the price and ask you to confirm the deal. Type \'yes\' and hit Enter if you agree to the trade.\n<br=7>NPC shop owners typically buy objects, too. If you would like to earn an extra gold coin by selling some spare object type \'sell <name of object>\'. Again you will be asked to confirm your offer by typing \'yes\' in order for the trade to take place.";
	case 6:
		return "You have now entered a dungeon. These areas are often dangerous because they are popular hideouts of all kinds of nasty creatures. If you should bump into a monster you try fighting it. Right-click onto the creature and select \'Attack\'.\n<br=7>It is a good idea to carefully watch your hit points during battle. If you lose too many hit points in too short a time you should run for it. Remember that even though death in Tibia is not final, it is a costly matter because you may loose hard-earned abilities or objects.\n<br=7>To get a detailed list of all creatures close to you press the \'Battle\' button to the right to open the battle window. You can use the battle window to attack monsters. Simply right-click on the creature\'s name on the list and select \'Attack\'.\n<br=7>For details on combat modes use the \'Client Help\' and place the magnifying glass on the combat control buttons to the right hand side of the screen.Note that dungeons are dark and dreary places, so you should always make sure to have a light source such as a torch with you. Also, keep in mind that many dungeons should only be entered if you have a rope with you.";
	case 7:
		return "<b>Congratulations!\n<br=7><n>You have gained your first experience points (XP)! You need to collect a certain amount of XP to reach a new \'level\' and to improve your character\'s abilities. Also, keep in mind that dead monsters often contain valuable items such as money or food. If you collect enough hard cash you can buy equipment such as better weapons and armour at a shop or from a player.\n<br=7>You should use the best weapons, armour and shields money can buy in order to improve your character\'s performance in combat. To equip items place them in the corresponding inventory slot. Remember that wounds can be healed by eating food, although this takes a while.";
	case 8:
		return "You advanced to Level 2! Your character\'s abilites have improved:\n<br=7>-<indent=15>You will be more difficult to kill because your number of hit points has increased.<indent=0>\n-<indent=15>You are walking faster.<indent=0>\n-<indent=15>You can carry additional weight.<indent=0>\n-<indent=15>You have gained mana which you will need later on to cast spells.<indent=0>\n-<indent=15>You can now yell messages. Also, from now on you are allowed to chat with other players on public channels.<indent=0>\n-<indent=15>Finally, you may now leave the safety of the village via the bridge to the north. Watch out! There are dangerous creatures lurking in the wilderness!";
	case 9:
		return "You can check your character\'s current abilities by pressing the button labelled \'Skills\' at the right hand side of the screen. This will open a window containing important information such as your character\'s experience, hit points, and skills.\n<br=5>Skill levels advance if the respective skill is used as often as possible. For example, if the sword is your weapon of choice and you have a sword equipped whenever you storm into combat, your sword skill will eventually rise. Needless to say, the higher the skill the better your character.";
	case 10:
		return "Characters of Level 8 or higher may leave Rookgaard and choose a vocation. There are fours paths to choose from: Be a valorous knight, a nimble paladin, a mysterious druid or a mighty sorcerer! To choose your destiny visit the oracle in the academy\'s tower or the gatekeeper in Rookgaard\'s premium area.\n<br=5><b>Warning:<n> Characters who do not leave Rookgaard while they are still level 8 will have reduced stats and skills on the main continent! You should make sure to leave Rookgaard as soon as possible.\n<br=5><h>Knights<n> are powerful melee fighters. They have the largest amount of hit points of all vocations and their combat skills are unrivalled. However, knights can cast very few spells. <h>Paladins<n> are the best distance fighters, and they perform reasonably well in melee. Since the Paladin also is a respectable magic user who knows how to cast many important spells he is the best all-around vocation. <h>Druids<n> are rather bad melee fighters, but their large number of mana and an impressive arsenal of spells make up for this drawback. The druids\' focus is on healing spells and on the benevolent aspects of magic, although they know how to cast many offensive spells as well. Like druids, <h>sorcerers<n> focus on magic. Being worse melee fighters than even their spell-casting cousins, sorcerers rely completely on their devastating spells to survive. High-level sorcerers are feared far and wide because of their deadly magic.";
	case 11:
		return "<b>Congratulations!\n<br=7><n>You have finally mastered Rookgaard and found your way to the mainland. There, you will get more of everything - more areas, more creatures and more items! Also, many additional features will now be available to you: You can store items in depots, you can send parcels and letters to other players ... and of course, you can use magic!\n<br=7>Travel to various cities to find NPCs who serve as masters of your vocation. These masters will sell spells to you. Once you have learnt a spell you can cast it by uttering the magic words. Mana is used whenever a spell is successfully cast, but thankfully characters continually regenerate mana as long as they have enough to eat.\n<br=7>If you are playing on a PvP (Player versus Player) or on a PvP-enforced world you will notice another important difference to Rookgaard: Players can now actively attack and even kill each other! However, on PvP worlds there are limits on how many player characters may be killed within certain periods of time. Unlimited playerkilling is only allowed on PvP-enforced worlds, while characters playing on Non-PvP worlds cannot attack each other just like on Rookgaard.";
	case 12:
		return "You have entered a depot - a warehouse where players can safely store items. Each major town has one, and players can store a certain amount of items in each of them, the limit being 1000 items per depot for free players and a staggering 2000 items per depot for premium players. To access your personal depot step on the field in front of any locker and open it like a normal container. Don\'t worry - your items will be perfectly safe in depots!\n<br=7>Depots typically contain mailboxes which you can use to send letters or parcels to other characters. To buy a parcel or a letter talk to one of the numerous post officials.\n<br=7>To send a parcel or a letter write the name of the addressee in the first line and his home town in the second line. Note that this information must be entered on a label if you are planning to send a parcel. Do not forget to place the label in the parcel before you post it!\n<br=7>To send off your letter or parcel simply move it onto the mailbox. If everything is fine the letter or parcel will be delivered right away.";
	case 13:
		return "<b>Watch out!\n<n><br=5>You have just encountered a marked player. The little skull mark underneath the name indicates that he has been involved in violence. All characters are free to attack marked players. If a marked player is killed the character who has killed him will not receive a mark himself.\n<br=7>On the other hand, if you attack unmarked players you will be marked yourself, and all other players are free to attack you!\n<br=7>Please note that the skulls come in various colours. For further details please see the manual on the website.";
	case 14:
		return "<b>Congratulations!\n<br=7><n>You have reached level 20!\n<br=5>If you are a premium player and if your character has the necessary funds, you can now promote your character to the elite rank of his vocation!\n<br=7>We would like to take the opportunity to remind you that you should keep your account secure. Remember there might be people who try to steal it!\n<br=7>Please take a minute and read the following security hints carefully. For further information please see our website. \n<br=10>1.<INDENT=15>Never give your password and/or account number away! <INDENT=0>\n<br=2>2.<INDENT=15>Use a secure password.<INDENT=0>\n<br=2>3.<INDENT=15>Do not store account data on your computer.<INDENT=0>\n<br=2>3.<INDENT=15>Do not download modified clients or tools for Tibia!<INDENT=0>\n<br=2>4.<INDENT=15>Do not share or trade accounts!<INDENT=0>\n<br=2>5.<INDENT=15>Protect your computer.<INDENT=0>\n<br=2>6.<INDENT=15>Keep an eye on the registered email address.<INDENT=0>\n<br=2>7.<INDENT=15>Personalise your account!<INDENT=0>\n<br=2>";
	default:
		TERROR1("unknown hint", Number);
	}
}

// 0x4a4440. The text for the current help handle.
const char *patella::THelp::GetCurrentHelpText()
{
	return GetHelpText(HelpHandle);
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Help.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void patella::THelp::ExitHelp()
{
}

// The functions of the original (issue #107): each calls its member on the one TClient's THelp.

void SetHelpPosition(int x, int y)
{
	patella::bridge::Help().SetHelpPosition(x, y);
}

void MoveHelpPosition(int dx, int dy)
{
	patella::bridge::Help().MoveHelpPosition(dx, dy);
}

void SetHelpSize(int Width, int Height)
{
	patella::bridge::Help().SetHelpSize(Width, Height);
}

void SetHelpHandle(int Handle)
{
	patella::bridge::Help().SetHelpHandle(Handle);
}

void GetHelpArea(int *x, int *y, int *Width, int *Height)
{
	patella::bridge::Help().GetHelpArea(x, y, Width, Height);
}

int GetHelpHandle()
{
	return patella::bridge::Help().GetHelpHandle();
}

bool GetHelpHandleChanged()
{
	return patella::bridge::Help().GetHelpHandleChanged();
}

void InitHelp()
{
	patella::bridge::Help().InitHelp();
}

void ExitHelp()
{
	patella::THelp::ExitHelp();
}

int GetEventHelp()
{
	return patella::bridge::Help().GetEventHelp();
}

int PopEventHelp()
{
	return patella::bridge::Help().PopEventHelp();
}

const char *GetHelpText(int Handle)
{
	return patella::bridge::Help().GetHelpText(Handle);
}

void AddEventHelp(int Number)
{
	patella::bridge::Help().AddEventHelp(Number);
}

void CheckEventHelpName(const char *Name)
{
	patella::bridge::Help().CheckEventHelpName(Name);
}

void CheckEventHelpPosition(int x, int y, int z)
{
	patella::bridge::Help().CheckEventHelpPosition(x, y, z);
}

const char *GetEventHelpTitle(int Number)
{
	return patella::bridge::Help().GetEventHelpTitle(Number);
}

const char *GetEventHelpText(int Number)
{
	return patella::bridge::Help().GetEventHelpText(Number);
}

const char *GetCurrentHelpText()
{
	return patella::bridge::Help().GetCurrentHelpText();
}
