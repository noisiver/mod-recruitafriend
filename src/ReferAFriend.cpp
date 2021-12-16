#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

struct Referred
{
    uint32 accountId;
    uint32 referrerId;
    time_t timestamp;
    uint8 active;
};

std::vector<Referred> referred;

uint32 duration;

class ReferAFriendAnnounce : public PlayerScript
{
    public:
        ReferAFriendAnnounce() : PlayerScript("ReferAFriendAnnounce") {}

        void OnLogin(Player* player) override
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server allows the use of the refer a friend feature.");
            ChatHandler(player->GetSession()).PSendSysMessage("Use the command |cff4CFF00.refer help|r to get started.");
        }
};

class ReferAFriendCommand : public CommandScript
{
    public:
        ReferAFriendCommand() : CommandScript("ReferAFriendCommand") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable referCommandTable =
            {
                { "friend", HandleReferFriendCommand, SEC_PLAYER, Console::No },
                { "help", HandleReferHelpCommand, SEC_PLAYER, Console::No },
            };

            static ChatCommandTable commandTable =
            {
                { "refer", referCommandTable }
            };

            return commandTable;
        }

        static bool HandleReferFriendCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            if (!target || !target->IsConnected() || target->GetConnectedPlayer()->IsGameMaster())
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 referrerAccountId = handler->GetSession()->GetAccountId();
            uint32 referralAccountId  = target->GetConnectedPlayer()->GetSession()->GetAccountId();

            if (referrerAccountId == referralAccountId)
            {
                ChatHandler(handler->GetSession()).SendSysMessage("You can't refer |cffFF0000yourself|r!");
                return true;
            }

            LoadReferredAccounts();

            if (IsReferralActive(referralAccountId))
            {
                ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cff4CFF00active|r.");
                return true;
            }

            if (!IsReferralValid(referralAccountId))
            {
                ChatHandler(handler->GetSession()).SendSysMessage("|cffFF0000A referral of that account has |cffFF0000expired|r.");
                return true;
            }

            if (!IsReferralValid(referrerAccountId))
            {
                if (WhoReferred(referrerAccountId) == referralAccountId)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You can't refer |cff4CFF00%s|r because they referred you.", target->GetConnectedPlayer()->GetName());
                    return true;
                }
            }

            LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = %i WHERE `id` = %i", referrerAccountId, referralAccountId);
            LoginDatabase.DirectPExecute("INSERT INTO `mod_referafriend` (`id`, `referrer`) VALUES (%i, %i)", referralAccountId, referrerAccountId);
            ChatHandler(handler->GetSession()).PSendSysMessage("You have successfully referred |cff4CFF00%s|r.", target->GetConnectedPlayer()->GetName());
            ChatHandler(handler->GetSession()).SendSysMessage("You both need to log out and back in for the changes to take effect.");

            return true;
        }

        static bool HandleReferHelpCommand(ChatHandler* handler)
        {
            ChatHandler(handler->GetSession()).SendSysMessage("You can refer a friend using |cff4CFF00.refer friend <name>|r.");
            ChatHandler(handler->GetSession()).PSendSysMessage("You will both receive a bonus to experience and reputation up to level %i.", sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL));

            if (duration > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("The refer a friend benefits will expire after %i days.", duration);
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("The refer a friend benefits will never expire.");
            }

            return true;
        }

        private:
            static void LoadReferredAccounts()
            {
                referrer.clear();

                QueryResult result = LoginDatabase.Query("SELECT `id`, `referrer`, `referral_date`, `active` FROM `mod_referafriend` ORDER BY `id` ASC");

                if (!result)
                    return;

                int i = 0;

                do
                {
                    Field* fields = result->Fetch();

                    referred.push_back(Referred());
                    referred[i].accountId  = fields[0].GetUInt32();
                    referred[i].referrerId = fields[1].GetUInt32();
                    referred[i].timestamp  = fields[2].GetUInt32();
                    referred[i].active     = fields[3].GetUInt8();

                    i++;
                } while (result->NextRow());
            }

            static bool IsReferralValid(uint32 accountId)
            {
                for (int i = 0; i < referred.size(); i++)
                {
                    if (referred[i].accountId == accountId)
                        return false;
                }

                return true;
            }

            static bool IsReferralActive(uint32 accountId)
            {
                for (int i = 0; i < referred.size(); i++)
                {
                    if (referred[i].accountId == accountId)
                        if (referred[i].active == 1)
                            return true;
                }

                return false;
            }

            static int WhoReferred(uint32 accountId)
            {
                for (int i = 0; i < referred.size(); i++)
                {
                    if (referred[i].accountId == accountId)
                        return referred[i].referrerId;
                }

                return 0;
            }
};

class ReferAFriendExpire : public WorldScript
{
    public:
        ReferAFriendExpire() : WorldScript("ReferAFriendExpire")
        {
            // 3600 seconds is 60 minutes
            timeDelay = (3600 * 1000);
        }

        void OnStartup() override
        {
            duration = sConfigMgr->GetOption<int32>("ReferAFriend.Duration", 90);
        }

        void OnUpdate(uint32 diff) override
        {
            if (duration > 0)
            {
                currentTime += diff;

                if (currentTime > timeDelay)
                {
                    CheckExpiredReferrals();
                    currentTime = 0;
                }
            }
        }

        private:
            uint32 currentTime;
            uint32 timeDelay;

            void CheckExpiredReferrals()
            {
                LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = 0 WHERE `id` IN (SELECT `id` FROM `mod_referafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND active = 1)", duration);
                LoginDatabase.DirectPExecute("UPDATE `mod_referafriend` SET `active` = 0 WHERE `referral_date` < NOW() - INTERVAL %i DAY AND `active` = 1", duration);
            }
};

void AddReferAFriendScripts()
{
    new ReferAFriendAnnounce();
    new ReferAFriendCommand();
    new ReferAFriendExpire();
}
