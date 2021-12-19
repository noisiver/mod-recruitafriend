#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

struct Referrals
{
    uint32 referralId;
    uint32 referrerId;
};

std::vector<Referrals> pendingReferrals;

uint32 duration;
uint32 age;
uint32 rewardDays;
bool rewardSwiftZhevra;
bool rewardTouringRocket;

class ReferAFriendCommand : public CommandScript
{
    public:
        ReferAFriendCommand() : CommandScript("ReferAFriendCommand") {}

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable referCommandTable =
            {
                { "accept", HandleReferAcceptCommand, SEC_PLAYER, Console::No },
                { "decline", HandleReferDeclineCommand, SEC_PLAYER, Console::No },
                { "friend", HandleReferFriendCommand, SEC_PLAYER, Console::No },
                { "help", HandleReferHelpCommand, SEC_PLAYER, Console::No },
                { "info", HandleReferInfoCommand, SEC_PLAYER, Console::No },
            };

            static ChatCommandTable commandTable =
            {
                { "refer", referCommandTable }
            };

            return commandTable;
        }

        static bool HandleReferAcceptCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
        {
            uint32 referralAccountId = handler->GetSession()->GetAccountId();

            for (int i = 0; i < pendingReferrals.size(); i++)
            {
                if (pendingReferrals[i].referralId == referralAccountId)
                {
                    pendingReferrals.erase(pendingReferrals.begin() + i);

                    LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = %i WHERE `id` = %i", pendingReferrals[i].referrerId, pendingReferrals[i].referralId);
                    LoginDatabase.DirectPExecute("INSERT INTO `mod_referafriend` (`id`, `referrer`) VALUES (%i, %i)", pendingReferrals[i].referralId, pendingReferrals[i].referrerId);
                    ChatHandler(handler->GetSession()).SendSysMessage("You have |cff4CFF00accepted|r the referral request.");
                    ChatHandler(handler->GetSession()).SendSysMessage("Remember that you have to log out and back in for the changes to take effect.");

                    return true;
                }
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You don't have a pending referral request.");

            return true;
        }

        static bool HandleReferDeclineCommand(ChatHandler* handler)
        {
            uint32 referralAccountId = handler->GetSession()->GetAccountId();

            for (int i = 0; i < pendingReferrals.size(); i++)
            {
                if (pendingReferrals[i].referralId == referralAccountId)
                {
                    pendingReferrals.erase(pendingReferrals.begin() + i);
                    ChatHandler(handler->GetSession()).SendSysMessage("You have |cffFF0000declined|r the referral request.");
                    return true;
                }
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You don't have a pending referral request.");

            return true;
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
            uint32 referralAccountId = target->GetConnectedPlayer()->GetSession()->GetAccountId();

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
                else
                    ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account has |cffFF0000expired|r.");

                return true;
            }

            if (WhoReferred(referrerAccountId) == referralAccountId)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't refer |cff4CFF00%s|r because they referred you.", target->GetConnectedPlayer()->GetName());
                return true;
            }

            if (!IsReferralValid(referralAccountId) && age > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You can't refer |cffFF0000%s|r because their account was created more than %i days ago.", target->GetConnectedPlayer()->GetName(), age);
                return true;
            }

            for (int i = 0; i < pendingReferrals.size(); i++)
            {
                if (pendingReferrals[i].referralId == referralAccountId)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You can't refer |cffFF0000%s|r because they already have a pending referral request.", target->GetConnectedPlayer()->GetName());
                    return true;
                }
            }

            uint32 index = pendingReferrals.size();
            pendingReferrals.push_back(Referrals());
            pendingReferrals[index].referralId = referralAccountId;
            pendingReferrals[index].referrerId = referrerAccountId;

            ChatHandler(handler->GetSession()).PSendSysMessage("You have sent a referral request to |cff4CFF00%s|r.", target->GetConnectedPlayer()->GetName());
            ChatHandler(handler->GetSession()).SendSysMessage("The player has to |cff4CFF00accept|r, or |cff4CFF00decline|r, the pending request.");

            ChatHandler(target->GetConnectedPlayer()->GetSession()).PSendSysMessage("|cff4CFF00%s|r has sent you a referral request.", handler->GetPlayer()->GetName());
            ChatHandler(target->GetConnectedPlayer()->GetSession()).SendSysMessage("Use |cff4CFF00.refer accept|r to accept or |cff4CFF00.refer decline|r to decline the request.");

            return true;
        }

        static bool HandleReferHelpCommand(ChatHandler* handler)
        {
            ChatHandler(handler->GetSession()).SendSysMessage("You can refer a friend using |cff4CFF00.refer friend <name>|r.");
            ChatHandler(handler->GetSession()).SendSysMessage("You can accept a pending referral using |cff4CFF00.refer accept|r.");
            ChatHandler(handler->GetSession()).SendSysMessage("You can decline a pending referral using |cff4CFF00.refer decline|r.");
            ChatHandler(handler->GetSession()).PSendSysMessage("You will both receive a bonus to experience and reputation up to level %i.", sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL));

            if (duration > 0)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("The refer a friend benefits will expire after %i days.", duration);
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("The refer a friend benefits will never expire.");
            }

            ChatHandler(handler->GetSession()).SendSysMessage("You can see the status of your referral using |cff4CFF00.refer info|r.");

            return true;
        }

        static bool HandleReferInfoCommand(ChatHandler* handler)
        {
            uint32 accountId = handler->GetSession()->GetAccountId();

            QueryResult result = LoginDatabase.PQuery("SELECT `referral_date`, `referral_date` + INTERVAL %i DAY, `active` FROM `mod_referafriend` WHERE `id` = %i", duration, accountId);
            if (result)
            {
                Field* fields = result->Fetch();
                std::string referralDate = fields[0].GetString();
                std::string expirationDate = fields[1].GetString();
                uint8 active = fields[2].GetUInt8();

                if (!active)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You were referred at |cff4CFF00%s|r and the referral expired at |cffFF0000%s|r.", referralDate, expirationDate);
                }
                else
                {
                    if (duration > 0)
                    {
                        ChatHandler(handler->GetSession()).PSendSysMessage("You were referred at |cff4CFF00%s|r and the referral will expire at |cffFF0000%s|r.", referralDate, expirationDate);
                    }
                    else
                    {
                        ChatHandler(handler->GetSession()).PSendSysMessage("You were referred at |cff4CFF00%s|r and the referral will |cffFF0000never|r expire.", referralDate, expirationDate);
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

                CharacterDatabase.DirectPExecute("UPDATE `characters` SET `rafRewarded` = 1 WHERE `guid` = %i", player->GetGUID().GetCounter());
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

class ReferAFriendWorld : public WorldScript
{
    public:
        ReferAFriendWorld() : WorldScript("ReferAFriendWorld")
        {
            // 60 minutes
            timeDelay = 60 * (60 * 1000);
            currentTime = timeDelay;
        }

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            duration = sConfigMgr->GetOption<int32>("ReferAFriend.Duration", 90);
            age = sConfigMgr->GetOption<int32>("ReferAFriend.AccountAge", 7);
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
                    LoginDatabase.DirectPExecute("UPDATE `account` SET `recruiter` = 0 WHERE `id` IN (SELECT `id` FROM `mod_referafriend` WHERE `referral_date` < NOW() - INTERVAL %i DAY AND active = 1)", duration);
                    LoginDatabase.DirectPExecute("UPDATE `mod_referafriend` SET `active` = 0 WHERE `referral_date` < NOW() - INTERVAL %i DAY AND `active` = 1", duration);

                    currentTime = 0;
                }
            }
        }

        private:
            uint32 currentTime;
            uint32 timeDelay;
};

void AddReferAFriendScripts()
{
    new ReferAFriendCommand();
    new ReferAFriendPlayer();
    new ReferAFriendWorld();
}
