#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

class RecruitAFriendAnnounce : public PlayerScript
{
    public:
        RecruitAFriendAnnounce() : PlayerScript("RecruitAFriendAnnounce") {}

        void OnLogin(Player* player) override
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server allows the use of the refer a friend feature.");
            ChatHandler(player->GetSession()).PSendSysMessage("Use the command |cff4CFF00.recruit help |rto get started.");
        }
};

class RecruitAFriendCommand : public CommandScript
{
    public:
        RecruitAFriendCommand() : CommandScript("RecruitAFriendCommand") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable recruitCommandTable =
            {
                { "friend", HandleRecruitFriendCommand, SEC_PLAYER, Console::No },
                { "help", HandleRecruitHelpCommand, SEC_PLAYER, Console::No },
            };

            static ChatCommandTable commandTable =
            {
                { "recruit", recruitCommandTable }
            };

            return commandTable;
        }

        static bool HandleRecruitHelpCommand(ChatHandler* handler)
        {
            uint32 duration = sConfigMgr->GetIntDefault("RecruitAFriend.Duration", 90);

            ChatHandler(handler->GetSession()).SendSysMessage("You can recruit a friend using |cff4CFF00.recruit friend <name> |r.");
            ChatHandler(handler->GetSession()).PSendSysMessage("You will both receive a bonus to experience and reputation up to level %i.", sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL));

            if (duration > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("The recruit a friend benefits will expire after %i days.", duration);

                if (sConfigMgr->GetBoolDefault("RecruitAFriend.Reusable", 0))
                    ChatHandler(handler->GetSession()).SendSysMessage("You can recruit the account again after it has expired.");
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("The recruit a friend benefits will never expire.");
            }

            return true;
        }

        static bool HandleRecruitFriendCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            if (!target || !target->IsConnected())
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 referrerAccountId = handler->GetSession()->GetAccountId();
            uint32 referralAccountId = target->GetConnectedPlayer()->GetSession()->GetAccountId();

            if (referrerAccountId == referralAccountId)
            {
                ChatHandler(handler->GetSession()).SendSysMessage("|cffFF0000You can't recruit yourself! |r");
                return true;
            }

            QueryResult result = LoginDatabase.PQuery("SELECT `active` FROM `mod_recruitafriend` WHERE `account_id` = %i AND `recruiter` = %i", referralAccountId, referrerAccountId);

            if (result)
            {
                if ((*result)[0].GetInt32() == 1)
                {
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cff4CFF00active |r.");
                    return true;
                }
                else
                {
                    if (!sConfigMgr->GetBoolDefault("RecruitAFriend.Reusable", 0))
                    {
                        ChatHandler(handler->GetSession()).SendSysMessage("|cffFF0000A referral of that account has |cffFF0000expired |r.");
                        return true;
                    }
                }
            }

            return true;
        }
};

void AddRecruitAFriendScripts()
{
    new RecruitAFriendAnnounce();
    new RecruitAFriendCommand();
}
