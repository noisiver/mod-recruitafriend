#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

struct Recruited
{
    uint32 accountId;
    uint32 recruiterId;
    time_t timestamp;
    uint8 active;
};

std::vector<Recruited> recruited;

uint32 duration;

class RecruitAFriendAnnounce : public PlayerScript
{
    public:
        RecruitAFriendAnnounce() : PlayerScript("RecruitAFriendAnnounce") {}

        void OnLogin(Player* player) override
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server allows the use of the refer a friend feature.");
            ChatHandler(player->GetSession()).PSendSysMessage("Use the command |cff4CFF00.recruit help|r to get started.");
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

        static bool HandleRecruitFriendCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            if (!target || !target->IsConnected())
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 recruiterAccountId = handler->GetSession()->GetAccountId();
            uint32 referralAccountId = target->GetConnectedPlayer()->GetSession()->GetAccountId();

            if (recruiterAccountId == referralAccountId)
            {
                ChatHandler(handler->GetSession()).SendSysMessage("You can't recruit |cffFF0000yourself|r!");
                return true;
            }

            LoadRecruitedAccounts();

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

            if (!IsReferralValid(recruiterAccountId))
            {
                if (WhoRecruited(recruiterAccountId) == referralAccountId)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cff4CFF00%s|r because they recruited you.", target->GetConnectedPlayer()->GetName());
                    return true;
                }
            }

            LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = %i WHERE `id` = %i", recruiterAccountId, referralAccountId);
            LoginDatabase.DirectPExecute("INSERT INTO `mod_recruitafriend` (`id`, `recruiter`) VALUES (%i, %i)", referralAccountId, recruiterAccountId);
            ChatHandler(handler->GetSession()).PSendSysMessage("You have successfully recruited |cff4CFF00%s|r.", target->GetConnectedPlayer()->GetName());
            ChatHandler(handler->GetSession()).SendSysMessage("You both need to log out and back in for the changes to take effect.");

            return true;
        }

        static bool HandleRecruitHelpCommand(ChatHandler* handler)
        {
            ChatHandler(handler->GetSession()).SendSysMessage("You can recruit a friend using |cff4CFF00.recruit friend <name>|r.");
            ChatHandler(handler->GetSession()).PSendSysMessage("You will both receive a bonus to experience and reputation up to level %i.", sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL));

            if (duration > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("The recruit a friend benefits will expire after %i days.", duration);
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("The recruit a friend benefits will never expire.");
            }

            return true;
        }

        private:
            static void LoadRecruitedAccounts()
            {
                recruited.clear();

                QueryResult result = LoginDatabase.Query("SELECT `id`, `recruiter`, `referral_date`, `active` FROM `mod_recruitafriend` ORDER BY `id` ASC");

                if (!result)
                    return;

                int i = 0;

                do
                {
                    Field* fields = result->Fetch();

                    recruited.push_back(Recruited());
                    recruited[i].accountId = fields[0].GetUInt32();
                    recruited[i].recruiterId = fields[1].GetUInt32();
                    recruited[i].timestamp = fields[2].GetUInt32();
                    recruited[i].active = fields[3].GetUInt8();

                    i++;
                } while (result->NextRow());
            }

            static bool IsReferralValid(uint32 accountId)
            {
                for (int i = 0; i < recruited.size(); i++)
                {
                    if (recruited[i].accountId == accountId)
                        return false;
                }

                return true;
            }

            static bool IsReferralActive(uint32 accountId)
            {
                for (int i = 0; i < recruited.size(); i++)
                {
                    if (recruited[i].accountId == accountId)
                        if (recruited[i].active == 1)
                            return true;
                }

                return false;
            }

            static int WhoRecruited(uint32 accountId)
            {
                for (int i = 0; i < recruited.size(); i++)
                {
                    if (recruited[i].accountId == accountId)
                        return recruited[i].recruiterId;
                }

                return 0;
            }
};

class RecruitAFriendExpire : public WorldScript
{
    public:
        RecruitAFriendExpire() : WorldScript("RecruitAFriendExpire")
        {
            // 3600 seconds is 60 minutes
            timeDelay = (3600 * 1000);
        }

        void OnStartup() override
        {
            duration = sConfigMgr->GetIntDefault("RecruitAFriend.Duration", 90);
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
                LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = 0 WHERE `id` IN (SELECT `id` FROM `mod_recruitafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND active = 1)", duration);
                LoginDatabase.DirectPExecute("UPDATE `mod_recruitafriend` SET `active` = 0 WHERE `referral_date` < NOW() - INTERVAL %i DAY AND `active` = 1", duration);
            }
};

void AddRecruitAFriendScripts()
{
    new RecruitAFriendAnnounce();
    new RecruitAFriendCommand();
    new RecruitAFriendExpire();
}
