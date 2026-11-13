#pragma once

#include "TestComponent.h"

namespace ai
{
    // =====================================================
    // AV test commands - Alterac Valley objective state
    // =====================================================
    class CommandAvStatus : public TestCommand
    {
    public:
        TestResult Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message) override;
    protected:
        std::string GetName() const override { return "av status"; }
    };

    class RequireAvNode : public TestCommand
    {
    public:
        TestResult Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message) override;
    protected:
        std::string GetName() const override { return "require av node"; }
    };
}
