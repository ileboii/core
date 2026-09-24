#pragma once

#include "playerbot/strategy/Action.h"
#include "playerbot/strategy/actions/MovementActions.h"
#include "playerbot/strategy/actions/AttackAction.h"

namespace ai
{
    class AIPlayAttackAction : public AttackAction
    {
    public:
        AIPlayAttackAction(PlayerbotAI* ai) : AttackAction(ai, "ai play attack") {}
        bool Execute(Event& event) override;
        bool isUseful() override { return true; }
    };

    class AIPlayMoveRandomAction : public MovementAction
    {
    public:
        AIPlayMoveRandomAction(PlayerbotAI* ai) : MovementAction(ai, "ai play move random") {}
        bool Execute(Event& event) override;
        bool isUseful() override { return true; }
    };

    class AIPlayStopAttackAction : public Action
    {
    public:
        AIPlayStopAttackAction(PlayerbotAI* ai) : Action(ai, "ai play stop attack") {}
        bool Execute(Event& event) override;
        bool isUseful() override { return true; }
    };

    class AIPlayMoveToRequesterAction : public MovementAction
    {
    public:
        AIPlayMoveToRequesterAction(PlayerbotAI* ai) : MovementAction(ai, "ai play move to requester") {}
        bool Execute(Event& event) override;
        // Explicit AI-play movement commands are allowed even while "stay" is active.
        bool isUseful() override { return true; }
    };

    class AIPlayAction : public Action
    {
    public:
        AIPlayAction(PlayerbotAI* ai) : Action(ai, "ai play") {}
        bool Execute(Event& event) override;
        bool isUseful() override;
        bool isUsefulWhenStunned() override { return true; }

        static bool ProcessPlayerMessage(PlayerbotAI* ai, uint32 type, ObjectGuid sender,
            ObjectGuid receiver, const std::string& text);
        static bool ProcessGeneratedText(PlayerbotAI* ai, const std::string& text, bool appendContext = true, Player* owner = nullptr);
        static void QueueGeneratedResponse(ObjectGuid botGuid, ObjectGuid ownerGuid, const std::string& text);
        static std::string GetCompactActionMenu();
        static void Observe(PlayerbotAI* ai);
        static void RememberEvent(PlayerbotAI* ai, std::string message);
        static void ObserveFact(PlayerbotAI* ai, const std::string& key, std::string value, bool reportChange = false);
        static std::string DescribeWorld(PlayerbotAI* ai, const std::string& topic = "", size_t maxLength = 450);
        static std::string ExtractActionIntent(std::string& text);
        static void TryStartAutonomous(PlayerbotAI* ai);

    private:
        static void StartActionSelection(PlayerbotAI* ai, const std::string& latestText, ObjectGuid ownerGuid);
    };
}
