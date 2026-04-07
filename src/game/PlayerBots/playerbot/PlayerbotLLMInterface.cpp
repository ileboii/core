// LLM Interface disabled - requires OpenSSL SSL library (ssleay32)
#include "playerbot/playerbot.h"
#include "PlayerbotLLMInterface.h"

INSTANTIATE_SINGLETON_1(PlayerbotLLMInterface);

std::string PlayerbotLLMInterface::Generate(const std::string&, int, int, std::vector<std::string>&)
{
    return "";
}

std::string PlayerbotLLMInterface::SanitizeForJson(const std::string& input)
{
    return input;
}

std::vector<std::string> PlayerbotLLMInterface::ParseResponse(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, std::vector<std::string>&)
{
    return {};
}

void PlayerbotLLMInterface::LimitContext(std::string&, int)
{
}
