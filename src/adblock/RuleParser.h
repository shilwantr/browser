#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace LiteBrowser {

enum class FilterRuleType {
    Block,
    Exception // Whitelist / Allow
};

enum ResourceTypeFilter : uint32_t {
    RESOURCE_ANY         = 0x0000,
    RESOURCE_SCRIPT      = 0x0001,
    RESOURCE_IMAGE       = 0x0002,
    RESOURCE_STYLESHEET  = 0x0004,
    RESOURCE_SUBDOCUMENT = 0x0008, // iframe
    RESOURCE_XMLHTTP     = 0x0010, // fetch / xhr
    RESOURCE_OTHER       = 0x0020
};

struct FilterRule {
    FilterRuleType type = FilterRuleType::Block;
    std::string pattern;
    std::string domainAnchor; // e.g. "doubleclick.net" if rule starts with ||
    bool isDomainAnchor = false;
    bool isThirdPartyOnly = false;
    bool isFirstPartyOnly = false;
    uint32_t resourceTypes = RESOURCE_ANY;
    bool isTracker = false;
};

class RuleParser {
public:
    static bool ParseLine(const std::string& line, FilterRule& outRule);
    static std::vector<FilterRule> ParseBuffer(const std::string& buffer);
};

} // namespace LiteBrowser
