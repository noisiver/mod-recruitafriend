#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

uint32 duration;
uint32 rewardDays;
bool rewardSwiftZhevra;
bool rewardTouringRocket;

class ReferAFriendCommands : public CommandScript
{
    public:
        ReferAFriendCommands() : CommandScript("ReferAFriendCommands") {}

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

            uint32 referralStatus = ReferralStatus(referralAccountId);

            if (referralStatus > 0)
            {
                if (referralStatus == 1)
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cff4CFF00active|r.");
                else if (referralStatus == 2)
                    ChatHandler(handler->GetSession()).SendSysMessage("|cffFF0000A referral of that account has |cffFF0000expired|r.");

                return true;
            }

            if (WhoReferred(referrerAccountId) == referralAccountId)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't refer |cff4CFF00%s|r because they referred you.", target->GetConnectedPlayer()->GetName());
                return true;
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
            static int ReferralStatus(uint32 accountId)
            {
                QueryResult result = LoginDatabase.PQuery("SELECT `active` FROM `mod_referafriend` WHERE `id` = %i", accountId);

                if (!result)
                    return 0;

                Field* fields = result->Fetch();
                uint32 active = fields[0].GetUInt32();

                if (active == 1)
                    return 1;
                else
                    return 2;
            }

            static int WhoReferred(uint32 accountId)
            {
                QueryResult result = LoginDatabase.PQuery("SELECT `referrer` FROM `mod_referafriend` WHERE `id` = %i", accountId);

                if (!result)
                    return 0;

                Field* fields = result->Fetch();
                uint32 referrerAccountId = fields[0].GetUInt32();

                return referrerAccountId;
            }
};

class ReferAFriendPlayer : public PlayerScript
{
    public:
        ReferAFriendPlayer() : PlayerScript("ReferAFriendPlayer") {}

        void OnLogin(Player* player) override
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server allows the use of the refer a friend feature.");
            ChatHandler(player->GetSession()).PSendSysMessage("Use the command |cff4CFF00.refer help|r to get started.");

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

                CharacterDatabase.DirectPExecute("UPDATE `characters` SET `referRewarded` = 1 WHERE `guid` = %i", player->GetGUID().GetCounter());
            }
        }

        private:
            bool IsEligible(uint32 accountId)
            {
                QueryResult result = LoginDatabase.PQuery("SELECT * FROM `mod_referafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND `id` = %i OR `referrer` = %i", rewardDays, accountId, accountId);

                if (!result)
                    return false;

                return true;
            }

            bool IsRewarded(uint32 characterGuid)
            {
                QueryResult result = CharacterDatabase.PQuery("SELECT `referRewarded` FROM `characters` WHERE `guid` = %i", characterGuid);

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

class ReferAFriendWorld : public WorldScript
{
    public:
        ReferAFriendWorld() : WorldScript("ReferAFriendWorld")
        {
            // 60 minutes
            timeDelay = 60 * (60 * 1000);
            currentTime = 0;
        }

        void OnStartup() override
        {
            duration = sConfigMgr->GetOption<int32>("ReferAFriend.Duration", 90);
            rewardDays = sConfigMgr->GetOption<int32>("ReferAFriend.Rewards.Days", 30);
            rewardSwiftZhevra = sConfigMgr->GetOption<bool>("ReferAFriend.Rewards.SwiftZhevra", 1);
            rewardTouringRocket = sConfigMgr->GetOption<bool>("ReferAFriend.Rewards.TouringRocket", 1);
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
    new ReferAFriendCommands();
    new ReferAFriendPlayer();
    new ReferAFriendWorld();
}
