
#include "playerbot/playerbot.h"
#include "SpellCastUsefulValue.h"
#include "LastSpellCastValue.h"
#include "playerbot/ServerFacade.h"
#ifdef MANGOS
#include "Spell.h"
#endif

using namespace ai;

bool SpellCastUsefulValue::Calculate()
{
    uint32 spellid = AI_VALUE2(uint32, "spell id", qualifier);
	if (!spellid)
		return true; // there can be known alternatives

	SpellEntry const *spellInfo = sServerFacade.LookupSpellInfo(spellid);
	if (!spellInfo)
		return true; // there can be known alternatives

	if (spellInfo->Attributes & SPELL_ATTR_ON_NEXT_SWING ||
		spellInfo->Attributes & SPELL_ATTR_ON_NEXT_SWING_NO_DAMAGE)
	{
		Spell* spell = bot->GetCurrentSpell(CURRENT_MELEE_SPELL);
if (spell && spell->m_spellInfo->Id == spellid && spell->m_spellInfo->IsNextMeleeSwingSpell() && bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
	return false;
	}
	else
	{
        uint32 lastSpellId = AI_VALUE(LastSpellCast&, "last spell cast").id;
        if (spellid == lastSpellId)
        {
            Spell* const pSpell = bot->FindCurrentSpellBySpellId(lastSpellId);
            if (pSpell && (pSpell->getState() == SPELL_STATE_CASTING || pSpell->getState() == SPELL_STATE_CASTING /* channeling mapped to casting */))
                return false;
        }
	}

    if ((spellInfo->HasAttribute(SPELL_ATTR_USES_RANGED_SLOT)) && bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) &&
            bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)->m_spellInfo->Id == spellid)
    {
        return false;
    }

    // TODO: workaround
    if (qualifier == "windfury weapon" || qualifier == "flametongue weapon" || qualifier == "frostbrand weapon" ||
            qualifier == "rockbiter weapon" || qualifier == "earthliving weapon" || qualifier == "spellstone")
    {
        Item *item = AI_VALUE2(Item*, "item for spell", spellid);
        if (item && item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
            return false;
    }

    std::set<uint32>& skipSpells = AI_VALUE(std::set<uint32>&, "skip spells list");
    if (skipSpells.find(spellid) != skipSpells.end())
        return false;

    const std::string spellName = spellInfo->SpellName[0];
    for (std::set<uint32>::iterator i = skipSpells.begin(); i != skipSpells.end(); ++i)
    {
        SpellEntry const *spell = sServerFacade.LookupSpellInfo(*i);
        if (!spell)
            continue;

        std::wstring wnamepart;
        if (!Utf8toWStr(spell->SpellName[0], wnamepart))
            continue;

        wstrToLower(wnamepart);
        if (!spellName.empty() && spellName.length() == wnamepart.length() && Utf8FitTo(spellName, wnamepart))
            return false;
    }

	return true;
}

bool SpellReadyValue::Calculate()
{
    uint32 spellid = stoi(qualifier);
    if (!spellid)
        return false;

    return bot->IsSpellReady(spellid);
}
