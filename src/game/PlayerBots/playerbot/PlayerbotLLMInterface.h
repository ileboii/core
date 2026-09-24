#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include "Policies/Singleton.h"

class PlayerbotLLMInterface
{
public:
    PlayerbotLLMInterface() {}
    static std::string SanitizeForJson(const std::string& input);

    static std::string Generate(const std::string& prompt, int timeOutSeconds, int maxGenerations, std::vector<std::string>& debugLines);

    // AI play has no queue: admit one request only while no chat request is outstanding.
    static bool TryReserveAIPlayGeneration();
    static void FinishAIPlayGeneration();
    static void ReserveChatGeneration();
    static void FinishChatGeneration();

    static std::vector<std::string> ParseResponse(const std::string& response, const std::string& startPattern, const std::string& endPattern, const std::string& deletePattern, const std::string& splitPattern, std::vector<std::string>& debugLines);

    static void LimitContext(std::string& context, int currentLength);
private:
    std::atomic<int> generationCount = 0;
    std::mutex generationAdmissionMutex;
    unsigned int outstandingChatGenerations = 0;
    bool aiPlayGenerationReserved = false;
};

#define sPlayerbotLLMInterface MaNGOS::Singleton<PlayerbotLLMInterface>::Instance()

