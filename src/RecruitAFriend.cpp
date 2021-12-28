#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

uint32 duration;
uint32 age;
uint32 rewardDays;
bool rewardSwiftZhevra;
bool rewardTouringRocket;

class RecruitAFriendCommand : public CommandScript
{
    public:
        RecruitAFriendCommand() : CommandScript("RecruitAFriendCommand") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable recruitCommandTable =
            {
                { "accept", HandleRecruitAcceptCommand, SEC_PLAYER, Console::No },
                { "decline", HandleRecruitDeclineCommand, SEC_PLAYER, Console::No },
                { "friend", HandleRecruitFriendCommand, SEC_PLAYER, Console::No },
                { "help", HandleRecruitHelpCommand, SEC_PLAYER, Console::No },
                { "status", HandleRecruitStatusCommand, SEC_PLAYER, Console::No },
            };

            static ChatCommandTable commandTable =
            {
                { "recruit", recruitCommandTable }
            };

            return commandTable;
        }

        static bool HandleRecruitAcceptCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            uint32 recruitedAccountId = handler->GetSession()->GetAccountId();

            QueryResult result = LoginDatabase.PQuery("SELECT `id`, `recruiter_id` FROM `mod_recruitafriend` WHERE `id` = %i AND `status` = 1", recruitedAccountId);
            if (result)
            {
                Field* fields = result->Fetch();
                std::string referralDate = fields[0].GetString();
                uint32 accountId = fields[0].GetUInt32();
                uint32 recruiterId = fields[1].GetUInt32();

                result = LoginDatabase.PQuery("DELETE FROM `mod_recruitafriend` WHERE `id` = %i AND `status` = 1", accountId);
                result = LoginDatabase.PQuery("UPDATE `account` SET `recruiter` = %i WHERE `id` = %i", recruiterId, accountId);
                result = LoginDatabase.PQuery("INSERT INTO `mod_recruitafriend` (`id`, `recruiter_id`, `status`) VALUES (%i, %i, 2)", accountId, recruiterId);
                ChatHandler(handler->GetSession()).SendSysMessage("You have |cff4CFF00accepted|r the referral request.");
                ChatHandler(handler->GetSession()).SendSysMessage("You have to log out and back in for the changes to take effect.");
                return true;
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You don't have a pending referral request.");
            return true;
        }

        static bool HandleRecruitDeclineCommand(ChatHandler* handler)
        {
            uint32 recruitedAccountId = handler->GetSession()->GetAccountId();

            QueryResult result = LoginDatabase.PQuery("SELECT `id` FROM `mod_recruitafriend` WHERE `id` = %i AND `status` = 1", recruitedAccountId);
            if (result)
            {
                result = LoginDatabase.PQuery("DELETE FROM `mod_recruitafriend` WHERE `id` = %i AND `status` = 1", recruitedAccountId);
                ChatHandler(handler->GetSession()).SendSysMessage("You have |cffFF0000declined|r the referral request.");
                return true;
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You don't have a pending referral request.");
            return true;
        }

        static bool HandleRecruitFriendCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            if (!target || !target->IsConnected() || target->GetConnectedPlayer()->IsGameMaster())
            {
                handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 recruiterAccountId = handler->GetSession()->GetAccountId();
            uint32 recruitedAccountId = target->GetConnectedPlayer()->GetSession()->GetAccountId();

            if (recruiterAccountId == recruitedAccountId)
            {
                ChatHandler(handler->GetSession()).SendSysMessage("You can't recruit |cffFF0000yourself|r!");
                return true;
            }

            uint32 referralStatus = ReferralStatus(recruitedAccountId);
            if (referralStatus > 0)
            {
                if (referralStatus == 1)
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cffFF0000pending|r.");
                else if (referralStatus == 2)
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cff4CFF00active|r.");
                else
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account has |cffFF0000expired|r.");

                return true;
            }

            if (WhoRecruited(recruiterAccountId) == recruitedAccountId)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cff4CFF00%s|r because they referred you.", target->GetConnectedPlayer()->GetName());
                return true;
            }

            if (!IsReferralValid(recruitedAccountId) && age > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cffFF0000%s|r because their account was created more than %i days ago.", target->GetConnectedPlayer()->GetName(), age);
                return true;
            }

            if (IsReferralPending(recruitedAccountId))
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cffFF0000%s|r because they already have a pending request.", target->GetConnectedPlayer()->GetName());
                return true;
            }

            QueryResult result = LoginDatabase.PQuery("INSERT INTO `mod_recruitafriend` (`id`, `recruiter_id`, `status`) VALUES (%i, %i, 1)", recruitedAccountId, recruiterAccountId);
            ChatHandler(handler->GetSession()).PSendSysMessage("You have sent a referral request to |cff4CFF00%s|r.", target->GetConnectedPlayer()->GetName());
            ChatHandler(handler->GetSession()).SendSysMessage("The player has to |cff4CFF00accept|r, or |cff4CFF00decline|r, the pending request.");
            ChatHandler(handler->GetSession()).SendSysMessage("If they accept the request, you have to log out and back in for the changes to take effect.");

            ChatHandler(target->GetConnectedPlayer()->GetSession()).PSendSysMessage("|cff4CFF00%s|r has sent you a referral request.", handler->GetPlayer()->GetName());
            ChatHandler(target->GetConnectedPlayer()->GetSession()).SendSysMessage("Use |cff4CFF00.recruit accept|r to accept or |cff4CFF00.recruit decline|r to decline the request.");
            return true;
        }

        static bool HandleRecruitHelpCommand(ChatHandler* handler)
        {
            ChatHandler(handler->GetSession()).SendSysMessage("You can recruit a friend using |cff4CFF00.recruit friend <name>|r.");
            ChatHandler(handler->GetSession()).SendSysMessage("You can accept a pending request using |cff4CFF00.recruit accept|r.");
            ChatHandler(handler->GetSession()).SendSysMessage("You can decline a pending request using |cff4CFF00.recruit decline|r.");
            ChatHandler(handler->GetSession()).PSendSysMessage("You will both receive a bonus to experience and reputation up to level %i.", sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL));

            if (duration > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("The recruit a friend benefits will expire after %i days.", duration);
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("The recruit a friend benefits will never expire.");
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You can see the status of your referral using |cff4CFF00.recruit status|r.");
            return true;
        }

        static bool HandleRecruitStatusCommand(ChatHandler* handler)
        {
            uint32 accountId = handler->GetSession()->GetAccountId();

            QueryResult result = LoginDatabase.PQuery("SELECT `referral_date`, `referral_date` + INTERVAL %i DAY, `status` FROM `mod_recruitafriend` WHERE `id` = %i", duration, accountId);
            if (result)
            {
                Field* fields = result->Fetch();
                std::string referralDate = fields[0].GetString();
                std::string expirationDate = fields[1].GetString();
                uint8 status = fields[2].GetUInt8();

                if (status == 3)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it expired at |cffFF0000%s|r.", referralDate, expirationDate);
                }
                else if (status == 2)
                {
                    ChatHandler(handler->GetSession()).SendSysMessage("You have not been recruited but have a |cff4CFF00pending|r request.");
                }
                else
                {
                    if (duration > 0)
                    {
                        ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it will expire at |cffFF0000%s|r.", referralDate, expirationDate);
                    }
                    else
                    {
                        ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it will |cffFF0000never|r expire.", referralDate, expirationDate);
                    }
                }
            }
            else
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You have |cffFF0000not|r been referred.");
            }

            return true;
        }

    private:
        static bool IsReferralValid(uint32 accountId)
        {
            QueryResult result = LoginDatabase.PQuery("SELECT * FROM `account` WHERE `id` = %i AND `joindate` > NOW() - INTERVAL %i DAY", accountId, age);

            if (!result)
                return false;

            return true;
        }

        static int ReferralStatus(uint32 accountId)
        {
            QueryResult result = LoginDatabase.PQuery("SELECT `status` FROM `mod_recruitafriend` WHERE `id` = %i", accountId);

            if (!result)
                return 0;

            Field* fields = result->Fetch();
            uint32 status = fields[0].GetUInt32();

            return status;
        }

        static int WhoRecruited(uint32 accountId)
        {
            QueryResult result = LoginDatabase.PQuery("SELECT `recruiter_id` FROM `mod_recruitafriend` WHERE `id` = %i", accountId);

            if (!result)
                return 0;

            Field* fields = result->Fetch();
            uint32 referrerAccountId = fields[0].GetUInt32();

            return referrerAccountId;
        }

        static bool IsReferralPending(uint32 accountId)
        {
            QueryResult result = LoginDatabase.PQuery("SELECT `id` FROM `mod_recruitafriend` WHERE `id` = %i AND `status` = 1", accountId);

            if (!result)
                return false;

            return true;
        }
};

class RecruitAFriendPlayer : public PlayerScript
{
    public:
        RecruitAFriendPlayer() : PlayerScript("RecruitAFriendPlayer") {}

        void OnLogin(Player* player) override
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server allows the use of the recruit a friend feature.");
            ChatHandler(player->GetSession()).PSendSysMessage("Use the command |cff4CFF00.recruit help|r to get started.");

            if (rewardDays > 0)
            {
                if (!IsEligible(player->GetSession()->GetAccountId()) && duration > 0)
                    return;

                if (IsRewarded(player->GetGUID().GetCounter()))
                    return;

                if (rewardSwiftZhevra)
                    SendMailTo(player, "Swift Zhevra", "I found this stray Zhevra walking around The Barrens, aimlessly. I figured you, if anyone, could give it a good home!", 37719, 1);

                if (rewardTouringRocket)
                    SendMailTo(player, "X-53 Touring Rocket", "This rocket was found flying around Northrend, with what seemed like no purpose. Perhaps you could put it to good use?", 54860, 1);

                QueryResult result = LoginDatabase.PQuery("UPDATE `characters` SET `rafRewarded` = 1 WHERE `guid` = %i", player->GetGUID().GetCounter());
            }
        }

    private:
        bool IsEligible(uint32 accountId)
        {
            QueryResult result = LoginDatabase.PQuery("SELECT * FROM `mod_recruitafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND (`id` = %i OR `recruiter_id` = %i) AND `status` NOT LIKE 1", rewardDays, accountId, accountId);

            if (!result)
                return false;

            return true;
        }

        bool IsRewarded(uint32 characterGuid)
        {
            QueryResult result = CharacterDatabase.PQuery("SELECT `rafRewarded` FROM `characters` WHERE `guid` = %i", characterGuid);

            if (!result)
                return true;

            Field* fields = result->Fetch();
            uint8 rewarded = fields[0].GetInt8();

            if (rewarded != 0)
                return true;

            return false;
        }

        void SendMailTo(Player* receiver, std::string subject, std::string body, uint32 itemId, uint32 itemCount)
        {
            uint32 guid = receiver->GetGUID().GetCounter();

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            MailDraft* mail  = new MailDraft(subject, body);
            ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(itemId);
            if (pProto)
            {
                Item* mailItem = Item::CreateItem(itemId, itemCount);
                if (mailItem)
                {
                    mailItem->SaveToDB(trans);
                    mail->AddItem(mailItem);
                }
            }

            mail->SendMailTo(trans, receiver ? receiver : MailReceiver(guid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_RETURNED);
            delete mail;
            CharacterDatabase.CommitTransaction(trans);
        }
};

class RecruitAFriendWorld : public WorldScript
{
    public:
        RecruitAFriendWorld() : WorldScript("RecruitAFriendWorld")
        {
            // 60 minutes
            timeDelay = 1h;
            currentTime = timeDelay;
        }

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            duration = sConfigMgr->GetOption<int32>("RecruitAFriend.Duration", 90);
            age = sConfigMgr->GetOption<int32>("RecruitAFriend.AccountAge", 7);
            rewardDays = sConfigMgr->GetOption<int32>("RecruitAFriend.Rewards.Days", 30);
            rewardSwiftZhevra = sConfigMgr->GetOption<bool>("RecruitAFriend.Rewards.SwiftZhevra", 1);
            rewardTouringRocket = sConfigMgr->GetOption<bool>("RecruitAFriend.Rewards.TouringRocket", 1);
        }

        void OnStartup() override
        {
            QueryResult result = LoginDatabase.Query("DELETE FROM `mod_recruitafriend` WHERE `status` = 1");
        }

        void OnUpdate(uint32 diff) override
        {
            if (duration > 0)
            {
                currentTime += Milliseconds(diff);

                if (currentTime > timeDelay)
                {
                    QueryResult result = LoginDatabase.PQuery("UPDATE `account` SET `recruiter` = 0 WHERE `id` IN (SELECT `id` FROM `mod_recruitafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND status = 2)", duration);
                    result = LoginDatabase.PQuery("UPDATE `mod_recruitafriend` SET `status` = 3 WHERE `referral_date` < NOW() - INTERVAL %i DAY AND `status` = 2", duration);

                    currentTime = 0s;
                }
            }
        }

    private:
        Milliseconds currentTime;
        Milliseconds timeDelay;
};

void AddRecruitAFriendScripts()
{
    new RecruitAFriendCommand();
    new RecruitAFriendPlayer();
    new RecruitAFriendWorld();
}
