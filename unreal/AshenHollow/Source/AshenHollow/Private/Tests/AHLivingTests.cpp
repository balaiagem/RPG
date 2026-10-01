#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHPlayerController.h"
#include "AHVillager.h"
#include "AHWildEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLivingRulesTest,"AshenHollow.Living.AwarenessLootQuestsAndAdvancement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHLivingRulesTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Spawn=[&](FVector At){return W->SpawnActor<AAHCharacter>(At,FRotator::ZeroRotator,P);};
    auto* Hero=Spawn(FVector(0,0,100)); Hero->ChooseClass(EAHHeroClass::Fighter);
    auto* PC=W->SpawnActor<AAHPlayerController>(); PC->Possess(Hero);
    auto* Mode=W->SpawnActor<AAHGameMode>();
    auto* Foe=Spawn(FVector(100,0,100)); Foe->bEnemy=true;
    auto* Near=Spawn(FVector(300,0,100)); Near->bEnemy=true;
    auto* Far=Spawn(FVector(601,0,100)); Far->bEnemy=true;
    TestTrue(TEXT("Witness at two metres"),Mode->CanWitness(Near,Foe));
    TestFalse(TEXT("Three metres is a hard limit"),Mode->CanWitness(Far,Near));
    auto* Wall=W->SpawnActor<AActor>();
    auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(15,100,180)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Box->SetWorldLocation(FVector(200,0,100));
    TestFalse(TEXT("Wall blocks nearby perception"),Mode->CanWitness(Near,Foe));
    Wall->Destroy();
    FAHCamp Camp; Camp.bAwake=true; Camp.Foes={Foe,Near,Far}; Mode->Camps.Add(Camp);
    Mode->BeginCombat(0,false,Foe);
    TestTrue(TEXT("Trigger participates"),Mode->Order.Contains(Foe));
    TestTrue(TEXT("Nearby witness participates"),Mode->Order.Contains(Near));
    TestFalse(TEXT("Same camp cannot bypass distance"),Mode->Order.Contains(Far));
    Mode->DrawInBystanders();
    TestFalse(TEXT("Nearby recruit cannot spread an alarm chain"),Mode->Order.Contains(Far));
    Foe->Health=Near->Health=0; Mode->EndCombat();
    TestFalse(TEXT("Camp stays uncleared while an unalerted member lives"),Mode->Camps[0].bCleared);
    Mode->bExploring=true;

    Hero->Carry(TEXT("couro_batido"));
    int32 Index=Hero->Backpack.IndexOfByPredicate([](const auto& X){return X.Id==TEXT("couro_batido");});
    const int32 Before=Hero->Backpack[Index].Many;
    TestFalse(TEXT("Wrong drop target does not consume item"),Hero->DropBackpackOnSlot(Index,EAHSlot::Elmo));
    TestEqual(TEXT("Rejected drop preserves stack"),Hero->Backpack[Index].Many,Before);
    TestTrue(TEXT("Armor drops into body slot"),Hero->DropBackpackOnSlot(Index,EAHSlot::Armadura));
    Hero->GainExperience(2700); PC->bSheetOpen=true;
    PC->CombatCommand(TEXT("Feat2"));
    TestEqual(TEXT("Level four choice works inside sheet before victory"),Hero->Feat,2);
    const int32 Init=Hero->InitiativeBonus;
    PC->CombatCommand(TEXT("Feat2"));
    TestEqual(TEXT("Feat cannot stack"),Hero->InitiativeBonus,Init);
    auto* AbilityHero=Spawn(FVector(1000,0,100)); AbilityHero->ChooseClass(EAHHeroClass::Wizard); AbilityHero->GainExperience(2700);
    const int32 IntBefore=AHSheet::Total(AbilityHero->Abilities,AbilityHero->Ancestry).Score[3];
    const int32 DCBefore=AbilityHero->SpellDC;
    TestTrue(TEXT("Can choose Intelligence improvement"),AbilityHero->ChooseFeat(7));
    TestEqual(TEXT("Real Intelligence increases by two"),AHSheet::Total(AbilityHero->Abilities,AbilityHero->Ancestry).Score[3],IntBefore+2);
    TestEqual(TEXT("Intelligence improves spell DC"),AbilityHero->SpellDC,DCBefore+1);

    Mode->DropLoot(Foe); TestEqual(TEXT("Defeated bandit yields a loot container"),Mode->Loot.Num(),1);
    TestTrue(TEXT("Loot can be picked up nearby"),Mode->TakeNearbyLoot(Hero));
    const int32 Gold=Hero->Gold;
    TestTrue(TEXT("Bandit drops gold"),Gold>0);
    TestFalse(TEXT("Taken loot cannot be taken twice"),Mode->TakeNearbyLoot(Hero));
    TestEqual(TEXT("Gold cannot duplicate"),Hero->Gold,Gold);

    auto* Npc=W->SpawnActor<AAHVillager>(); Npc->Casa=FVector(10,20,0);
    FAHSideQuest Quest; Quest.Giver=Npc->Casa; Quest.CampIndex=0; Quest.RewardItem=TEXT("pocao_cura"); Mode->SideQuests.Add(Quest);
    Mode->TalkSideQuest(Npc,Hero); TestEqual(TEXT("Conversation accepts mission"),Mode->SideQuests[0].Stage,1);
    Mode->Camps[0].bCleared=true; Mode->UpdateSideQuests();
    TestEqual(TEXT("Clearing the target completes objective"),Mode->SideQuests[0].Stage,2);
    Mode->TalkSideQuest(Npc,Hero); TestEqual(TEXT("Return grants mission reward"),Mode->SideQuests[0].Stage,3);
    auto PotionCount=[&](){int32 N=0; for(const auto& X:Hero->Backpack) if(X.Id==TEXT("pocao_cura")) N+=X.Many; return N;};
    const int32 Potions=PotionCount(); Mode->TalkSideQuest(Npc,Hero);
    TestEqual(TEXT("Mission rewards cannot duplicate"),PotionCount(),Potions);
    for(int32 Kind=1;Kind<=2;++Kind)
    {
        auto* Animal=W->SpawnActor<AAHWildEnemy>(FVector(2000+Kind*500,0,100),FRotator::ZeroRotator,P);
        Animal->ConfigureWildlife(Kind,42);
        TestFalse(TEXT("Animals do not carry ranged weapons"),Animal->HasRangedAttack());
        TArray<UStaticMeshComponent*> Parts; Animal->GetComponents(Parts);
        int32 WildlifeMeshes=0;
        for(auto* Part:Parts) if(Part->GetStaticMesh() && Part->GetStaticMesh()->GetPathName().Contains(TEXT("/Wildlife/"))) ++WildlifeMeshes;
        TestEqual(TEXT("Animal has body and four legs"),WildlifeMeshes,5);
    }
    GEngine->DestroyWorldContext(W); W->DestroyWorld(false); return true;
}
#endif
