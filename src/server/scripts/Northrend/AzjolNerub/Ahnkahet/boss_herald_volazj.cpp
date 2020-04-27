/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Comment: Missing AI for Twisted Visages
 */

#include "Log.h"
#include "ScriptMgr.h"
#include "ahnkahet.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"

enum Spells
{
    SPELL_INSANITY                                = 57496, // Dummy cast, but it hits every player
    INSANITY_VISUAL                               = 57561,
    SPELL_INSANITY_TARGET                         = 57508,
    SPELL_MIND_FLAY                               = 57941,
    SPELL_SHADOW_BOLT_VOLLEY                      = 57942,
    SPELL_SHIVER                                  = 57949,
    SPELL_CLONE_PLAYER                            = 57507, // Player should cast on a Twisted Visage for it to clone the player's appearance
    SPELL_INSANITY_PHASING_1                      = 57508,
    SPELL_INSANITY_PHASING_2                      = 57509,
    SPELL_INSANITY_PHASING_3                      = 57510,
    SPELL_INSANITY_PHASING_4                      = 57511,
    SPELL_INSANITY_PHASING_5                      = 57512

};

uint32 const InsanityPhasingSpells [5] = // These are the spells that the player casts in order to phase them. Also produces the debuff "Insanity"
{
    57508, // Phase 16
    57509, // Phase 32
    57510, // Phase 64
    57511, // Phase 128
    57512  // Phase 256
};

uint32 const InsanitySummonTwistedVisageSpells [5] = // These are the spells that the player should cast to make a Twisted Visage of themselves for other players to attack
{
    57500, // Summons 30621
    57501, // Summons 30622
    57502, // Summons 30623
    57503, // Summons 30624
    57504  // Summons 30625
};

uint32 const TwistedVisageNPCs [5] = // These are the NPC IDs that are spawned by the SummonTwistedVisageSpells above
{
    30621, // Summoned by 57500
    30622, // Summoned by 57501
    30623, // Summoned by 57502
    30624, // Summoned by 57503
    30625  // Summoned by 57504
};

enum TwistedVisageSpells // These are the possible spells used by the twisted visage, based on their player target's skills
{
    TV_SPELL_AVENGERS_SHIELD  = 57799, // Protection Paladin, original 31935
    TV_SPELL_BLOOD_PLAGUE     = 57601, // Probably the result of a death knight's Plague Strike
    TV_SPELL_BLOOD_THIRST     = 57790, // Fury Warrior, original 23881
    TV_SPELL_CONSECRATION     = 57798, // All Paladin
    TV_SPELL_CORRUPTION       = 57645, // Warlock
    TV_SPELL_DEVASTATE        = 57795, // Protection Warrior, original 20243
    TV_SPELL_DISENGAGE        = 57635, // Hunter
    TV_SPELL_EARTH_SHIELD     = 57802, // Resto Shaman, Original 974
    TV_SPELL_EARTH_SHOCK      = 57783, // Elemental Shaman
    TV_SPELL_FIREBALL         = 57628, // All Mage
    TV_SPELL_HEALING_WAVE     = 57785, // Resto Shaman
    TV_SPELL_INTERCEPT_DNU    = 61490, // This one stuns until cancelled - Do not use?
    TV_SPELL_INTERCEPT        = 61491, // All Warrior
    TV_SPELL_LIFEBLOOM        = 57762, // Resto Druid
    TV_SPELL_LIGHTNING_BOLT   = 57781, // Elemental Shaman
    TV_SPELL_MANGLE           = 57657, // Feral Druid, Original 33917
    TV_SPELL_MOONFIRE         = 57647, // Balance Druid
    TV_SPELL_MORTAL_STRIKE    = 57789, // Arms Warrior, Original 12294
    TV_SPELL_NOURISH          = 57765, // Resto Druid
    TV_SPELL_PLAGUE_STRIKE    = 57599, // Death Knight
    TV_SPELL_RENEW            = 57777, // Priest
    TV_SPELL_SEAL_OF_COMMAND  = 57769, // Retribution Paladin
    TV_SPELL_SHADOW_BOLT      = 57644, // Warlock
    TV_SPELL_SINISTER_STRIKE  = 57640, // Rogue
    TV_SPELL_SUNDER_ARMOR     = 57807, // All Warrior except Protection
    TV_SPELL_THUNDER_CLAP     = 57832, // All Warrior
    TV_SPELL_THUNDERSTORM     = 57784, // Elemental Shaman, 51490
    TV_SPELL_WRATH            = 57648  // Balance Druid
};

enum Yells
{
    SAY_AGGRO   = 0,
    SAY_SLAY    = 1,
    SAY_DEATH   = 2,
    SAY_PHASE   = 3
};

enum Achievements
{
    ACHIEV_QUICK_DEMISE_START_EVENT               = 20382,
};

class boss_volazj : public CreatureScript
{
public:
    boss_volazj() : CreatureScript("boss_volazj") { }

    struct boss_volazjAI : public ScriptedAI
    {
        boss_volazjAI(Creature* creature) : ScriptedAI(creature), Summons(me)
        {
            Initialize();
            instance = creature->GetInstanceScript();
        }

        void Initialize()
        {
            uiMindFlayTimer = 8 * IN_MILLISECONDS;
            uiShadowBoltVolleyTimer = 5 * IN_MILLISECONDS;
            uiShiverTimer = 15 * IN_MILLISECONDS;
            // Used for Insanity handling
            insanityHandled = 0;
        }

        InstanceScript* instance;

        uint32 uiMindFlayTimer;
        uint32 uiShadowBoltVolleyTimer;
        uint32 uiShiverTimer;
        uint32 insanityHandled;
        SummonList Summons;

        // returns the percentage of health after taking the given damage.
        uint32 GetHealthPct(uint32 damage)
        {
            if (damage > me->GetHealth())
                return 0;
            return 100*(me->GetHealth()-damage)/me->GetMaxHealth();
        }

        void DamageTaken(Unit* /*pAttacker*/, uint32 &damage) override
        {
            if (me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
                damage = 0;

            if ((GetHealthPct(0) >= 66 && GetHealthPct(damage) < 66)||
                (GetHealthPct(0) >= 33 && GetHealthPct(damage) < 33))
            {
                me->InterruptNonMeleeSpells(false);
                DoCast(me, SPELL_INSANITY, false);
            }
        }

        void SpellHitTarget(Unit* target, SpellInfo const* spell) override
        {
            if (spell->Id == SPELL_INSANITY)
            {
                // Check Target
                if (target->GetTypeId() != TYPEID_PLAYER || insanityHandled > 4)
                    // Target either isn't player, or there are more than 5 players in the instance
                    return;

                // Check if this is the first Insanity to hit a player
                if (!insanityHandled)
                {
                    // First target to be hit with insanity
                    // Channel the visual effect
                    DoCast(me, INSANITY_VISUAL, true);
                    // Make myself unattackable
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    me->SetControlled(true, UNIT_STATE_STUNNED);
                    TC_LOG_INFO("scripts","First Insanity Handled");
                }

                // Apply phase mask to the player
                target->CastSpell(target, InsanityPhasingSpells[insanityHandled], true);
                TC_LOG_INFO("scripts","Applying phase mask %d to %s",insanityHandled,target->GetName());

                // Summon Twisted Visages for the current player
                // Get players currently in the instance
                Map::PlayerList const& players = me->GetMap()->GetPlayers();
                TC_LOG_INFO("scripts","Getting Player List (%d players)",players.getSize());
                for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
                {
                    // For each player in the instance
                    Player* player = i->GetSource();
                    // Check for valid player
                    if (!player || !player->IsAlive() || player == target)
                        // Player either was null, is not alive, or is the current player affected. Skip
                        continue;
                    TC_LOG_INFO("scripts","Found valid other player %s",player->GetName());
                    // Summon a twisted visage
                    player->CastSpell(player,InsanitySummonTwistedVisageSpells[insanityHandled],true);

                    // Next, get the creature we just summoned
                    // FIXME: This is not the way to do it.  When we come through again for another player, then we will always get the same creature, causing phantom visages
                    Creature* summoned = GetClosestCreatureWithEntry(player,TwistedVisageNPCs[insanityHandled],1.0f,true);

                    // Add to the summons list
                    Summons.Summon(summoned);

                    // Next, clone their appearance
                    player->CastSpell(summoned,SPELL_CLONE_PLAYER,true);

                    // Set the summoned creature's phase mask
                    summoned->SetPhaseMask((1<<(4+insanityHandled)), true);

                    // Start their attack
                    summoned->SetReactState(REACT_AGGRESSIVE);
                    DoZoneInCombat(summoned);

                    // Decide what kind of spells they will use
                    // Check class
                    // player->GetClass();
                    // Check for Specialization in class
                    // player->HasTalent(spell_id,spec);
                    // Start the AI based on the player's spec
                }
                // Put the zone in combat to start all the attackers

                // All done, so increment the insanity handled
                TC_LOG_INFO("scripts","All done with this insanity.");
                ++insanityHandled;
            }
        }

        void ResetPlayersPhaseMask()
        {
            Map::PlayerList const& players = me->GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
            {
                Player* player = i->GetSource();
                player->RemoveAurasDueToSpell(GetSpellForPhaseMask(player->GetPhaseMask()));
            }
        }

        void Reset() override
        {
            Initialize();

            instance->SetBossState(DATA_HERALD_VOLAZJ, NOT_STARTED);
            instance->DoStopTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEV_QUICK_DEMISE_START_EVENT);

            // Visible for all players in insanity
            me->SetPhaseMask((1|16|32|64|128|256), true);

            ResetPlayersPhaseMask();

            // Cleanup
            Summons.DespawnAll();
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetControlled(false, UNIT_STATE_STUNNED);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            Talk(SAY_AGGRO);

            instance->SetBossState(DATA_HERALD_VOLAZJ, IN_PROGRESS);
            instance->DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEV_QUICK_DEMISE_START_EVENT);
        }

        void JustSummoned(Creature* summon) override
        {
            // This is not called now that we aren't doing manual summons.
            // Summons.Summon(summon);
        }

        uint32 GetSpellForPhaseMask(uint32 phase)
        {
            uint32 spell = 0;
            switch (phase)
            {
                case 16:
                    spell = SPELL_INSANITY_PHASING_1;
                    break;
                case 32:
                    spell = SPELL_INSANITY_PHASING_2;
                    break;
                case 64:
                    spell = SPELL_INSANITY_PHASING_3;
                    break;
                case 128:
                    spell = SPELL_INSANITY_PHASING_4;
                    break;
                case 256:
                    spell = SPELL_INSANITY_PHASING_5;
                    break;
            }
            return spell;
        }

        virtual void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
        {
            // This is not called since we aren't doing manual summons
            TC_LOG_INFO("scripts","Summoned Creature %s has died",summon->GetName());
        }


        void SummonedCreatureDespawn(Creature* summon) override
        {
            // This is also not called since we aren't doing manual summons
            TC_LOG_INFO("scripts","Summoned Creature %s has despawned",summon->GetName());
            uint32 phase = summon->GetPhaseMask();
            uint32 nextPhase = 0;
            Summons.Despawn(summon);

            // Check if all summons in this phase killed
            for (SummonList::const_iterator iter = Summons.begin(); iter != Summons.end(); ++iter)
            {
                if (Creature* visage = ObjectAccessor::GetCreature(*me, *iter))
                {
                    // Not all are dead
                    if (phase == visage->GetPhaseMask())
                        return;
                    else
                        nextPhase = visage->GetPhaseMask();
                }
            }

            // Roll Insanity
            uint32 spell = GetSpellForPhaseMask(phase);
            uint32 spell2 = GetSpellForPhaseMask(nextPhase);
            Map::PlayerList const& PlayerList = me->GetMap()->GetPlayers();
            if (!PlayerList.isEmpty())
            {
                for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                {
                    if (Player* player = i->GetSource())
                    {
                        if (player->HasAura(spell))
                        {
                            player->RemoveAurasDueToSpell(spell);
                            if (spell2) // if there is still some different mask cast spell for it
                                player->CastSpell(player, spell2, true);
                        }
                    }
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            //Return since we have no target
            if (!UpdateVictim())
                return;

            if (insanityHandled)
            {
                if (!Summons.empty())
                    return;

                insanityHandled = 0;
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetControlled(false, UNIT_STATE_STUNNED);
                me->RemoveAurasDueToSpell(INSANITY_VISUAL);
            }

            if (uiMindFlayTimer <= diff)
            {
                DoCastVictim(SPELL_MIND_FLAY);
                uiMindFlayTimer = 20*IN_MILLISECONDS;
            } else uiMindFlayTimer -= diff;

            if (uiShadowBoltVolleyTimer <= diff)
            {
                DoCastVictim(SPELL_SHADOW_BOLT_VOLLEY);
                uiShadowBoltVolleyTimer = 5*IN_MILLISECONDS;
            } else uiShadowBoltVolleyTimer -= diff;

            if (uiShiverTimer <= diff)
            {
                if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                    DoCast(target, SPELL_SHIVER);
                uiShiverTimer = 15*IN_MILLISECONDS;
            } else uiShiverTimer -= diff;

            DoMeleeAttackIfReady();
        }

        void JustDied(Unit* /*killer*/) override
        {
            Talk(SAY_DEATH);

            instance->SetBossState(DATA_HERALD_VOLAZJ, DONE);

            Summons.DespawnAll();
            ResetPlayersPhaseMask();
        }

        void KilledUnit(Unit* who) override
        {
            if (who->GetTypeId() == TYPEID_PLAYER)
                Talk(SAY_SLAY);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetAhnKahetAI<boss_volazjAI>(creature);
    }
};

void AddSC_boss_volazj()
{
    new boss_volazj();
}
