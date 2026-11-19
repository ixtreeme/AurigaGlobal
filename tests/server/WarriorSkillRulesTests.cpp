#include "../../SRC/Server/GameServer/warrior/WarriorSkillRules.hpp"
#include "../../SRC/Server/GameServer/warrior/WarriorProjectileRules.hpp"
#include <iostream>
#include <stdexcept>
using namespace WarriorExpansion;
void Check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
int main()
{
    try
    {
        Check(WithinHop(2400,0,4400,0,2000),"hop is relative to previous target beyond caster range");
        Check(!WithinHop(2400,0,4401,0,2000),"hop cannot exceed 20 metres");
        Check(!WithinHop(0,0,2001,0,999999),"configured hop range is hard capped");
        Check(ProjectileTravel(0,1000)==400&&ProjectileTravel(2000,1000)==2000,"visible projectile travel duration");
        Check(BeamRange(1)==1200 && BeamRange(40)==4000,"beam range bounds");
        Check(!IsBuffAffect(200)&&!IsBuffAffect(201)&&!IsBuffAffect(202)&&IsBuffAffect(BuffAffect(200)),"native potion affect namespace");
        Check(EffectDuration(193,40)==10 && EffectDuration(198,40)==20 && EffectDuration(199,40)==10,"field duration P");
        Check(TargetCount(195,40)==10 && TargetCount(196,40)==12,"multi-target P");
        Check(InsideBeam(3999,149,1,0,4000,150) && !InsideBeam(4001,0,1,0,4000,150) &&
              !InsideBeam(200,151,1,0,4000,150) && !InsideBeam(-1,0,1,0,4000,150),"beam footprint boundary");
        for(int level=1;level<=40;++level) {
            Check(BeamRange(level)>=1200 && BeamRange(level)<=4000,"beam progression");
            for(uint32_t id : {193u,198u,199u}) {
                const int pulses=EffectDuration(id,level)*(id==198?2:1);
                double budget=0;for(int tick=0;tick<pulses;++tick)budget+=1.5/pulses;
                Check(std::abs(budget-1.5)<1e-9,"periodic damage budget");
            }
        }
        for(uint32_t id : {194u,200u,201u,202u,203u,204u})
            for(unsigned group : {1u,2u})
                Check(ValidateChoice(0,group,10,Choices{},0,id)==ChoiceError::None,"shared buff choice");
        for(uint32_t id : {193u,197u,198u,199u})
            Check(ValidateChoice(0,1,90,Choices{},4,id)==ChoiceError::None,"new area choice");
        for(uint32_t id : {195u,196u})
            Check(ValidateChoice(0,1,30,Choices{},1,id)==ChoiceError::None,"new ranged choice");
        Choices choices{};
        int combinations = 0;
        for (unsigned group : {1u, 2u})
            for (int buff = 0; buff < 2; ++buff)
            {
                auto id = 180 + (group - 1) * 2 + buff;
                Check(ValidateChoice(0, group, 9, choices, 0, id) == ChoiceError::Level, "early buff");
                Check(ValidateChoice(0, group, 10, choices, 0, id) == ChoiceError::None, "buff unlock");
                Check(ValidateChoice(0, group, 90, choices, 0, 180 + (2 - group) * 2) == ChoiceError::Category,
                      "opposite specialization buff");
                for (unsigned job : {1u, 2u, 3u}) {
                    Check(ValidateChoice(job, group, 150, choices, 0, 182) == ChoiceError::None, "all classes battle power");
                    Check(ValidateChoice(job, group, 150, choices, 1, 195) == ChoiceError::None, "all classes ranged");
                    Check(ValidateChoice(job, group, 150, choices, 4, 199) == ChoiceError::None, "all classes area");
                }
                Check(ValidateChoice(4, group, 150, choices, 0, id) == ChoiceError::Class, "unsupported class");
                for (uint16_t a = 184; a <= 189; ++a)
                    for (uint16_t b = 184; b <= 189; ++b)
                        for (uint16_t c = 184; c <= 189; ++c)
                        {
                            if (a == b || a == c || b == c)
                                continue;
                            Choices picked{static_cast<uint16_t>(id), a, b, c, 0};
                            for (uint16_t area = 190; area <= 192; ++area)
                            {
                                Check(ValidateChoice(0, group, 90, picked, 4, area) == ChoiceError::None,
                                      "legal five-skill build");
                                ++combinations;
                            }
                            Check(ValidateChoice(0, group, 89, picked, 4, 190) == ChoiceError::Level, "early area");
                            picked[3] = 0;
                            Check(ValidateChoice(0, group, 70, picked, 3, a) == ChoiceError::Duplicate,
                                  "duplicate across tiers");
                            Check(ValidateChoice(0, group, 70, picked, 1, c) == ChoiceError::Spent, "spent tier");
                        }
            }
        for (unsigned tier = 0; tier < 5; ++tier)
        {
            int level = UnlockLevels[tier];
            uint32_t id = tier == 0 ? 180 : tier == 4 ? 190 : 184;
            Check(ValidateChoice(0, 1, level - 1, choices, tier, id) == ChoiceError::Level, "unlock boundary");
            Check(ValidateChoice(0, 1, level, choices, tier, id) == ChoiceError::None, "unlock exact");
        }
        for (uint32_t forged : {0u, 1u, 4u, 19u, 31u, 61u, 91u, 179u, 193u, 255u, 0xffffffffu})
            Check(ValidateChoice(0, 1, 150, choices, 1, forged) != ChoiceError::None, "forged skill");
        Check(ValidateChoice(0, 1, 150, choices, 0xffffffffu, 184) == ChoiceError::Tier, "forged tier bounds");
        Check(ValidateChoice(0, 0, 150, choices, 0, 180) == ChoiceError::Group, "missing doctrine");
        Check(AvailablePoints(9, 0) == 0 && AvailablePoints(10, 0) == 1 && AvailablePoints(90, 80) == 1 &&
                  AvailablePoints(50, 80) == 0,
              "isolated expansion point budget");
        int successes = 0;
        for (int master = 20; master <= 29; ++master)
        {
            ReadProgress p{static_cast<uint8_t>(master), 0};
            for (int read = 1; read <= BooksRequired(master); ++read)
            {
                auto failure = AdvanceReading(p, false);
                Check(failure.level == p.level && failure.successes == p.successes, "failure advances progress");
                p = AdvanceReading(p, true);
                ++successes;
                Check(p.level == (read < BooksRequired(master) ? master : master + 1), "master threshold");
            }
            Check(p.successes == 0, "stage counter reset");
        }
        Check(successes == 65, "M1 to G1 requires exactly 65 successful reads");
        for (uint8_t level : {uint8_t(1), uint8_t(19), uint8_t(30), uint8_t(39), uint8_t(40)})
            Check(AdvanceReading({level, 0}, true).level == level, "book bypasses mastery");
        ImpactGate gate;
        gate.range = 1800;
        gate.distance = 1800;
        Check(CanImpact(gate), "range inclusive");
        gate.distance = 1801;
        Check(!CanImpact(gate), "range cap");
        gate.distance = 1;
        for (int flag = 0; flag < 6; ++flag)
        {
            auto invalid = gate;
            switch (flag)
            {
            case 0:
                invalid.alive = false;
                break;
            case 1:
                invalid.connected = false;
                break;
            case 2:
                invalid.sameMap = false;
                break;
            case 3:
                invalid.weaponUnchanged = false;
                break;
            case 4:
                invalid.learned = false;
                break;
            case 5:
                invalid.targetValid = false;
                break;
            }
            Check(!CanImpact(invalid), "invalid impact state");
        }
        std::cout << "PASS warrior expansion legal_builds=" << combinations << " successful_reads=" << successes
                  << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
