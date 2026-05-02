#include "StdAfx.h"
#include "PdaConstants.h"

namespace
{
const char* ResolveAlias(const char* defaultId)
{
    CInifile* aliasFile = nullptr;
    if (pSettings && pSettings->section_exist(PdaConfig::TabAliasesSection))
    {
        aliasFile = pSettings;
    }
    else if (pGameGlobals && pGameGlobals->section_exist(PdaConfig::TabAliasesSection))
    {
        aliasFile = pGameGlobals;
    }

    if (!aliasFile)
    {
        return defaultId;
    }

    if (!aliasFile->line_exist(PdaConfig::TabAliasesSection, defaultId))
    {
        return defaultId;
    }

    const char* alias = aliasFile->r_string(PdaConfig::TabAliasesSection, defaultId);
    if (!alias || !alias[0])
    {
        return defaultId;
    }

    return alias;
}
}

namespace PdaSectionId
{
const char* Resolve(const char* defaultId)
{
    return ResolveAlias(defaultId);
}

bool Equals(const shared_str& sectionId, const char* defaultId)
{
    const char* resolvedId = Resolve(defaultId);
    return sectionId == defaultId || sectionId == resolvedId;
}
}
