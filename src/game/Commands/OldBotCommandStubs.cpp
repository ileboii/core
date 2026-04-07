// Bot command implementations.
#include "Chat.h"
#include "playerbot/PlayerbotMgr.h"
#include "playerbot/RandomPlayerbotMgr.h"

static bool RouteToPlayerbotMgr(ChatHandler* handler, const char* subcmd, char* args) {
    std::string fullArgs = subcmd;
    if (args) { if (args[0]) { fullArgs += " "; fullArgs += args; } }
    return PlayerbotMgr::HandlePlayerbotMgrCommand(handler, fullArgs.c_str());
}
bool ChatHandler::HandlePlayerbotMgrChatCommand(char* args) {
    return PlayerbotMgr::HandlePlayerbotMgrCommand(this, args);
}
bool ChatHandler::HandleRandomPlayerbotCommand(char* args) {
    return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(this, args);
}
bool ChatHandler::HandleBotAddCommand(char* a) { return RouteToPlayerbotMgr(this, "add", a); }
bool ChatHandler::HandleBotAddAllCommand(char* a) { return RouteToPlayerbotMgr(this, "add", a); }
bool ChatHandler::HandleBotAddRandomCommand(char* a) { return RouteToPlayerbotMgr(this, "add", a); }
bool ChatHandler::HandleBotDeleteCommand(char* a) { return RouteToPlayerbotMgr(this, "remove", a); }
bool ChatHandler::HandleBotInfoCommand(char* a) { return RouteToPlayerbotMgr(this, "info", a); }
bool ChatHandler::HandleBotStartCommand(char* a) { return RouteToPlayerbotMgr(this, "add", a); }
bool ChatHandler::HandleBotStopCommand(char* a) { return RouteToPlayerbotMgr(this, "remove", a); }
bool ChatHandler::HandleBotReloadCommand(char* a) { return RouteToPlayerbotMgr(this, "reload", a); }
bool ChatHandler::HandlePartyBotAddCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotRemoveCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotCloneCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotSetRoleCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotAttackStartCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotAttackStopCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotAoECommand(char*) { return false; }
bool ChatHandler::HandlePartyBotPauseCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotUnpauseCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotFocusMarkCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotControlMarkCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotClearMarksCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotComeToMeCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotUseGObjectCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotPullCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotStartCastingCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotStopCastingCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotLoadCommand(char*) { return false; }
bool ChatHandler::HandlePartyBotUnequipCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotAddWarsongCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotAddArathiCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotAddAlteracCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotRemoveCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotRemoveAllCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotShowPathCommand(char*) { return false; }
bool ChatHandler::HandleBattleBotShowAllPathsCommand(char*) { return false; }
