#include "RuleParser.h"
#include <sstream>
#include <algorithm>

namespace LiteBrowser {

namespace {

void Trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

} // namespace

bool RuleParser::ParseLine(const std::string& lineRaw, FilterRule& outRule) {
    std::string line = lineRaw;
    Trim(line);

    if (line.empty()) return false;
    // Comments
    if (line[0] == '!' || line[0] == '[') return false;
    // Cosmetic / element hiding rules (contains ## or #@#)
    if (line.find("##") != std::string::npos || line.find("#@#") != std::string::npos) return false;

    outRule = FilterRule();

    // Check if exception rule
    if (line.rfind("@@", 0) == 0) {
        outRule.type = FilterRuleType::Exception;
        line = line.substr(2);
    } else {
        outRule.type = FilterRuleType::Block;
    }

    // Check options separated by '$'
    size_t optPos = line.find('$');
    if (optPos != std::string::npos) {
        std::string options = line.substr(optPos + 1);
        line = line.substr(0, optPos);

        std::stringstream ss(options);
        std::string opt;
        while (std::getline(ss, opt, ',')) {
            Trim(opt);
            if (opt == "third-party") {
                outRule.isThirdPartyOnly = true;
            } else if (opt == "~third-party") {
                outRule.isFirstPartyOnly = true;
            } else if (opt == "script") {
                outRule.resourceTypes |= RESOURCE_SCRIPT;
            } else if (opt == "image") {
                outRule.resourceTypes |= RESOURCE_IMAGE;
            } else if (opt == "stylesheet") {
                outRule.resourceTypes |= RESOURCE_STYLESHEET;
            } else if (opt == "subdocument") {
                outRule.resourceTypes |= RESOURCE_SUBDOCUMENT;
            } else if (opt == "xmlhttprequest") {
                outRule.resourceTypes |= RESOURCE_XMLHTTP;
            }
        }
    }

    // Check domain anchor ||
    if (line.rfind("||", 0) == 0) {
        outRule.isDomainAnchor = true;
        std::string rest = line.substr(2);
        // Find end of domain (^ or / or $)
        size_t endDomain = rest.find_first_of("^/$");
        if (endDomain != std::string::npos) {
            outRule.domainAnchor = rest.substr(0, endDomain);
            outRule.pattern = rest.substr(endDomain);
            if (!outRule.pattern.empty() && outRule.pattern[0] == '^') {
                outRule.pattern = outRule.pattern.substr(1);
            }
        } else {
            outRule.domainAnchor = rest;
            outRule.pattern = "";
        }
        // Lowercase domain
        std::transform(outRule.domainAnchor.begin(), outRule.domainAnchor.end(),
                       outRule.domainAnchor.begin(), ::tolower);
    } else {
        outRule.isDomainAnchor = false;
        outRule.pattern = line;
    }

    return true;
}

std::vector<FilterRule> RuleParser::ParseBuffer(const std::string& buffer) {
    std::vector<FilterRule> rules;
    std::stringstream ss(buffer);
    std::string line;
    while (std::getline(ss, line)) {
        FilterRule r;
        if (ParseLine(line, r)) {
            rules.push_back(std::move(r));
        }
    }
    return rules;
}

} // namespace LiteBrowser
