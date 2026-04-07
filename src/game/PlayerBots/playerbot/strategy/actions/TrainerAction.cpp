
#include "playerbot/playerbot.h"
#include "TrainerAction.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/BudgetValues.h"

using namespace ai;

void TrainerAction::Learn(uint32 cost, ObjectGuid trainerGuid, uint32 spellId, TrainerSpell const* tSpell, std::ostringstream& msg)
{
    if (sPlayerbotAIConfig.autoTrainSpells != "free" &&  !ai->HasCheat(BotCheatMask::gold))
    {
        if (AI_VALUE2(uint32, "free money for", (uint32)NeedMoneyFor::spells) < cost)
        {
            msg << " - too expensive";
            return;
        }

        bot->ModifyMoney(-int32(cost));
    }

    SpellEntry const* proto = sServerFacade.LookupSpellInfo(tSpell->spell);
    if (!proto)
        return;

#ifdef MANGOSBOT_ZERO
    if (tSpell->spell)
    {
        // old code
        // bot->LearnSpell(tSpell->spell, false);
        bool learned = false;
        for (int j = 0; j < 3; ++j)
        {
            if (proto->Effect[j] == SPELL_EFFECT_LEARN_SPELL)
            {
                uint32 learnedSpell = proto->EffectTriggerSpell[j];
                bot->LearnSpell(learnedSpell, false);
                learned = true;
            }
        }
        if (!learned) bot->LearnSpell(tSpell->spell, false);
    }
    else
        ai->CastSpell(tSpell->spell, bot);
#else
    // From NPCHandler
    bot->GetSession()/* SendPlaySpellVisual not in vmangos */;   // visual effect on trainer

    WorldPacket data(SMSG_PLAY_SPELL_IMPACT, 8 + 4);             // visual effect on player
    data << bot->GetObjectGuid();
    data << uint32(0x016A);                                      // index from SpellVisualKit.dbc
    bot->GetSession()->SendPacket(&data);

    if (tSpell->IsCastable())
        bot->CastSpell(bot, tSpell->spell, true);
    else
        bot->LearnSpell(spellId, false);
#endif

    sPlayerbotAIConfig.logEvent(ai, "TrainerAction", proto->SpellName[0], std::to_string(proto->Id));

    msg << " - learned";
}

bool TrainerAction::Iterate(Player* requester, Creature* creature, TrainerSpellAction action, SpellIds& spells)
{
    bool hasHeader = false;    
    bool hasTrainable = false;

    TrainerSpellData const* cSpells = creature->GetTrainerSpells();
    TrainerSpellData const* tSpells = creature->GetTrainerTemplateSpells();
    float fDiscountMod =  bot->GetReputationPriceDiscount(creature);
    uint32 totalCost = 0;

    TrainerSpellMap trainer_spells;
    if (cSpells)
        trainer_spells.insert(cSpells->spellList.begin(), cSpells->spellList.end());
    if (tSpells)
        trainer_spells.insert(tSpells->spellList.begin(), tSpells->spellList.end());

    for (TrainerSpellMap::const_iterator itr =  trainer_spells.begin(); itr !=  trainer_spells.end(); ++itr)
    {
        TrainerSpell const* tSpell = &itr->second;

        if (!tSpell)
            continue;

        uint32 reqLevel = 0;

        reqLevel = tSpell->reqLevel;
        TrainerSpellState state = bot->GetTrainerSpellState(tSpell);
        if (state != TRAINER_SPELL_GREEN)
            continue;

        uint32 spellId = tSpell->spell;
        const SpellEntry *const pSpellInfo =  sServerFacade.LookupSpellInfo(spellId);
        if (!pSpellInfo)
            continue;

#ifdef MANGOSBOT_ZERO
        if (tSpell->spell)
        {
            bool learned = true;
            if (bot->HasSpell(tSpell->spell))
            {
                learned = false;
            }
            else
            {
                for (int j = 0; j < 3; ++j)
                {
                    if (pSpellInfo->Effect[j] == SPELL_EFFECT_LEARN_SPELL)
                    {
                        learned = false;
                        uint32 learnedSpell = pSpellInfo->EffectTriggerSpell[j];

                        if (!bot->HasSpell(learnedSpell))
                        {
                            learned = true;
                            hasTrainable = true;
                            break;
                        }
                    }
                }
            }

            if (!learned)
                continue;
        }
#else
        if (!tSpell->spell.empty())
        {
            bool anySpellLearned = false;
            for (auto& learnedSpell : tSpell->spell)
            {
                bool learned = true;
                if (bot->HasSpell(learnedSpell))
                {
                    learned = false;
                }
                else
                {
                    for (int j = 0; j < 3; ++j)
                    {
                        if (pSpellInfo->Effect[j] == SPELL_EFFECT_LEARN_SPELL)
                        {
                            learned = false;
                            uint32 learnedSpell = pSpellInfo->EffectTriggerSpell[j];

                            if (!bot->HasSpell(learnedSpell))
                            {
                                learned = true;
                                hasTrainable = true;
                                break;
                            }
                        }
                    }
                }
                if (learned)
                    anySpellLearned = true;
            }
            if (!anySpellLearned)
                continue;
        }
#endif

        if (!spells.empty() && spells.find(tSpell->spell) == spells.end())
            continue;

        uint32 cost = uint32(floor(tSpell->spellCost *  fDiscountMod));
        totalCost += cost;

        std::ostringstream out;
        out << chat->formatSpell(pSpellInfo) << chat->formatMoney(cost);

        if (action)
            (this->*action)(cost, creature->GetObjectGuid(), itr->first, tSpell, out);

        if (!hasHeader)
        {
            TellHeader(requester, creature);
            hasHeader = true;
        }
        ai->TellPlayer(requester, out, PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
    }

    if(hasHeader)
        TellFooter(requester, totalCost);
    else if (!ai->GetMaster() || sServerFacade.GetDistance2d(bot, ai->GetMaster()) < sPlayerbotAIConfig.reactDistance || ai->HasStrategy("debug", BotState::BOT_STATE_NON_COMBAT))
        ai->TellPlayerNoFacing(requester, "No spells can be learned from this trainer");

    return hasTrainable;
}

bool TrainerAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    std::string text = event.getParam();
    Creature* creature = nullptr;

    if (event.getSource() == "rpg action")
    {
        ObjectGuid guid = event.getObject();
        creature = ai->GetCreature(guid);
    }
    else
    {
        if (requester)
            creature = ai->GetCreature(requester->GetSelectionGuid());
        else
            return false;
    }

#ifdef MANGOS
    if (!creature || !creature->IsTrainer())
#endif
#ifdef CMANGOS
    if (!creature || !creature->isTrainer())
#endif
        return false;       
            
    if (!creature->IsTrainerOf(bot, false))
    {
        if (!ai->GetMaster() || sServerFacade.GetDistance2d(bot, ai->GetMaster()) < sPlayerbotAIConfig.reactDistance || ai->HasStrategy("debug", BotState::BOT_STATE_NON_COMBAT))
            ai->TellPlayerNoFacing(requester, "This trainer cannot teach me");
        return false;
    }

    // check present spell in trainer spell list
    TrainerSpellData const* cSpells = creature->GetTrainerSpells();
    TrainerSpellData const* tSpells = creature->GetTrainerTemplateSpells();
    if (!cSpells && !tSpells)
    {
        if (!ai->GetMaster() || sServerFacade.GetDistance2d(bot, ai->GetMaster()) < sPlayerbotAIConfig.reactDistance || ai->HasStrategy("debug", BotState::BOT_STATE_NON_COMBAT))
            ai->TellPlayerNoFacing(requester, "No spells can be learned from this trainer");
        return false;
    }

    uint32 spell = chat->parseSpell(text);
    SpellIds spells;
    if (spell)
        spells.insert(spell);

    if (text.find("learn") != std::string::npos || sRandomPlayerbotMgr.IsFreeBot(bot) || (sPlayerbotAIConfig.autoTrainSpells != "no" && (creature->GetCreatureInfo()->trainer_type != TRAINER_TYPE_TRADESKILLS || !ai->HasActivePlayerMaster()))) //Todo rewrite to only exclude start primary profession skills and make config dependent.
    {
        if(Iterate(requester, creature, &TrainerAction::Learn, spells))
            context->ClearValues("item usage"); //Bot might be able to use new items.
    }
    else
        Iterate(requester, creature, NULL, spells);

    return true;
}

void TrainerAction::TellHeader(Player* requester, Creature* creature)
{
    std::ostringstream out; out << "--- Can learn from " << creature->GetName() << " ---";
    ai->TellPlayer(requester, out, PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
}

void TrainerAction::TellFooter(Player* requester, uint32 totalCost)
{
    if (totalCost)
    {
        std::ostringstream out; out << "Total cost: " << chat->formatMoney(totalCost);
        ai->TellPlayer(requester, out, PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
    }
}
