#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

uint32 rafDuration;
uint32 rafAge;
uint32 rafRewardDays;
bool rafRewardSwiftZhevra;
bool rafRewardTouringRocket;
bool rafRewardCelestialSteed;
uint32 rafRealmId;

enum ReferralStatus
{
    STATUS_REFERRAL_PENDING = 1,
    STATUS_REFERRAL_ACTIVE  = 2,
    STATUS_REFERRAL_EXPIRED = 3
};

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

    static bool HandleRecruitAcceptCommand(ChatHandler* handler)
    {
        uint32 recruitedAccountId = handler->GetSession()->GetAccountId();

        QueryResult result = LoginDatabase.Query("SELECT `account_id`, `recruiter_id` FROM `recruit_a_friend_accounts` WHERE `account_id` = {} AND `status` = {}", recruitedAccountId, STATUS_REFERRAL_PENDING);
        if (result)
        {
            Field* fields = result->Fetch();
            std::string referralDate = fields[0].Get<std::string>();
            uint32 accountId = fields[0].Get<uint32>();
            uint32 recruiterId = fields[1].Get<uint32>();

            result = LoginDatabase.Query("DELETE FROM `recruit_a_friend_accounts` WHERE `account_id` = {} AND `status` = {}", accountId, STATUS_REFERRAL_PENDING);
            result = LoginDatabase.Query("UPDATE `account` SET `recruiter` = {} WHERE `id` = {}", recruiterId, accountId);
            result = LoginDatabase.Query("INSERT INTO `recruit_a_friend_accounts` (`account_id`, `recruiter_id`, `status`) VALUES ({}, {}, {})", accountId, recruiterId, STATUS_REFERRAL_ACTIVE);
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

        QueryResult result = LoginDatabase.Query("SELECT `account_id` FROM `recruit_a_friend_accounts` WHERE `account_id` = {} AND `status` = {}", recruitedAccountId, STATUS_REFERRAL_PENDING);
        if (result)
        {
            result = LoginDatabase.Query("DELETE FROM `recruit_a_friend_accounts` WHERE `account_id` = {} AND `status` = {}", recruitedAccountId, STATUS_REFERRAL_PENDING);
            ChatHandler(handler->GetSession()).SendSysMessage("You have |cffFF0000declined|r the referral request.");
            return true;
        }

        ChatHandler(handler->GetSession()).SendSysMessage("You don't have a pending referral request.");
        return true;
    }

    static bool HandleRecruitFriendCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target || !target->IsConnected() || target->GetConnectedPlayer()->GetSession()->GetSecurity() != SEC_PLAYER)
        {
            handler->SendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (handler->GetSession()->GetSecurity() != SEC_PLAYER)
        {
            ChatHandler(handler->GetSession()).SendSysMessage("You can't recruit a player because you're a |cffFF0000gamemaster|r!");
            return true;
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
            if (referralStatus == STATUS_REFERRAL_PENDING)
                ChatHandler(handler->GetSession()).SendSysMessage("A referral of that account is currently |cffFF0000pending|r.");
            else if (referralStatus == STATUS_REFERRAL_ACTIVE)
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

        if (!IsReferralValid(recruitedAccountId) && rafAge > 0)
        {
            ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cffFF0000%s|r because their account was created more than %i days ago.", target->GetConnectedPlayer()->GetName(), rafAge);
            return true;
        }

        if (IsReferralPending(recruitedAccountId))
        {
            ChatHandler(handler->GetSession()).PSendSysMessage("You can't recruit |cffFF0000%s|r because they already have a pending request.", target->GetConnectedPlayer()->GetName());
            return true;
        }

        QueryResult result = LoginDatabase.Query("INSERT INTO `recruit_a_friend_accounts` (`account_id`, `recruiter_id`, `status`) VALUES ({}, {}, {})", recruitedAccountId, recruiterAccountId, STATUS_REFERRAL_PENDING);
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

        if (rafDuration > 0)
        {
            ChatHandler(handler->GetSession()).PSendSysMessage("The recruit a friend benefits will expire after %i days.", rafDuration);
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

        QueryResult result = LoginDatabase.Query("SELECT `referral_date`, `referral_date` + INTERVAL {} DAY, `status` FROM `recruit_a_friend_accounts` WHERE `account_id` = {}", rafDuration, accountId);
        if (result)
        {
            Field* fields = result->Fetch();
            std::string referralDate = fields[0].Get<std::string>();
            std::string expirationDate = fields[1].Get<std::string>();
            uint8 status = fields[2].Get<int8>();

            if (status == STATUS_REFERRAL_EXPIRED)
            {
                ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it expired at |cffFF0000%s|r.", referralDate, expirationDate);
            }
            else if (status == STATUS_REFERRAL_ACTIVE)
            {
                if (rafDuration > 0)
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it will expire at |cffFF0000%s|r.", referralDate, expirationDate);
                }
                else
                {
                    ChatHandler(handler->GetSession()).PSendSysMessage("You were recruited at |cff4CFF00%s|r and it will |cffFF0000never|r expire.", referralDate, expirationDate);
                }
            }
            else
            {
                ChatHandler(handler->GetSession()).SendSysMessage("You have not been recruited but have a |cff4CFF00pending|r request.");
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
        QueryResult result = LoginDatabase.Query("SELECT * FROM `account` WHERE `id` = {} AND `joindate` > NOW() - INTERVAL {} DAY", accountId, rafAge);

        if (!result)
            return false;

        return true;
    }

    static int ReferralStatus(uint32 accountId)
    {
        QueryResult result = LoginDatabase.Query("SELECT `status` FROM `recruit_a_friend_accounts` WHERE `account_id` = {}", accountId);

        if (!result)
            return 0;

        Field* fields = result->Fetch();
        uint32 status = fields[0].Get<uint32>();

        return status;
    }

    static uint32 WhoRecruited(uint32 accountId)
    {
        QueryResult result = LoginDatabase.Query("SELECT `recruiter_id` FROM `recruit_a_friend_accounts` WHERE `account_id` = {}", accountId);

        if (!result)
            return 0;

        Field* fields = result->Fetch();
        uint32 referrerAccountId = fields[0].Get<uint32>();

        return referrerAccountId;
    }

    static bool IsReferralPending(uint32 accountId)
    {
        QueryResult result = LoginDatabase.Query("SELECT `account_id` FROM `recruit_a_friend_accounts` WHERE `account_id` = {} AND `status` = {}", accountId, STATUS_REFERRAL_PENDING);

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

        if (rafRewardDays > 0)
        {
            if (!IsEligible(player->GetSession()->GetAccountId()) && rafDuration > 0)
                return;

            if (IsRewarded(player))
                return;

            if (rafRewardSwiftZhevra)
                SendMailTo(player, "Swift Zhevra", "I found this stray Zhevra walking around The Barrens, aimlessly. I figured you, if anyone, could give it a good home!", 37719, 1);

            if (rafRewardTouringRocket)
                SendMailTo(player, "X-53 Touring Rocket", "This rocket was found flying around Northrend, with what seemed like no purpose. Perhaps you could put it to good use?", 54860, 1);

            if (rafRewardCelestialSteed)
                SendMailTo(player, "Celestial Steed", "A strange steed was found roaming Northrend, phasing in and out of existence. I figured you would be interested in such a companion.", 54811, 1);


            QueryResult result = LoginDatabase.Query("INSERT INTO `recruit_a_friend_rewarded` (`account_id`, `realm_id`, `character_guid`) VALUES ({}, {}, {})", player->GetSession()->GetAccountId(), rafRealmId, player->GetGUID().GetCounter());
        }
    }

private:
    bool IsEligible(uint32 accountId)
    {
        QueryResult result = LoginDatabase.Query("SELECT * FROM `recruit_a_friend_accounts` WHERE `referral_date` < NOW() - INTERVAL {} DAY AND (`account_id` = {} OR `recruiter_id` = {}) AND `status` NOT LIKE {}", rafRewardDays, accountId, accountId, STATUS_REFERRAL_PENDING);

        if (!result)
            return false;

        return true;
    }

    bool IsRewarded(Player* player)
    {
        QueryResult result = LoginDatabase.Query("SELECT * FROM `recruit_a_friend_rewarded` WHERE `account_id` = {} AND `realm_id` = {} AND `character_guid` = {}", player->GetSession()->GetAccountId(), rafRealmId, player->GetGUID().GetCounter());

        if (result)
            return true;

        return false;
    }

    void SendMailTo(Player* receiver, std::string subject, std::string body, uint32 itemId, uint32 itemCount)
    {
        uint32 guid = receiver->GetGUID().GetCounter();

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft* mail = new MailDraft(subject, body);
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
        timeDelay = 1h;
        currentTime = timeDelay;
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        rafDuration = sConfigMgr->GetOption<int32>("RecruitAFriend.Duration", 90);
        rafAge = sConfigMgr->GetOption<int32>("RecruitAFriend.MaxAccountAge", 7);
        rafRewardDays = sConfigMgr->GetOption<int32>("RecruitAFriend.Rewards.Days", 30);
        rafRewardSwiftZhevra = sConfigMgr->GetOption<bool>("RecruitAFriend.Rewards.SwiftZhevra", 1);
        rafRewardTouringRocket = sConfigMgr->GetOption<bool>("RecruitAFriend.Rewards.TouringRocket", 1);
        rafRewardCelestialSteed = sConfigMgr->GetOption<bool>("RecruitAFriend.Rewards.CelestialSteed", 1);
        rafRealmId = sConfigMgr->GetOption<uint32>("RealmID", 0);
    }

    void OnStartup() override
    {
        QueryResult result = LoginDatabase.Query("DELETE FROM `recruit_a_friend_accounts` WHERE `status` = {}", STATUS_REFERRAL_PENDING);
    }

    void OnUpdate(uint32 diff) override
    {
        if (rafDuration > 0)
        {
            currentTime += Milliseconds(diff);

            if (currentTime > timeDelay)
            {
                QueryResult result = LoginDatabase.Query("UPDATE `account` SET `recruiter` = 0 WHERE `id` IN (SELECT `account_id` FROM `recruit_a_friend_accounts` WHERE `referral_date` < NOW() - INTERVAL {} DAY AND status = {})", rafDuration, STATUS_REFERRAL_ACTIVE);
                result = LoginDatabase.Query("UPDATE `recruit_a_friend_accounts` SET `status` = {} WHERE `referral_date` < NOW() - INTERVAL {} DAY AND `status` = {}", STATUS_REFERRAL_EXPIRED, rafDuration, STATUS_REFERRAL_ACTIVE);

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
