#include "AHCharacter.h"
#include "AHAnimInstance.h"
#include "AHMagicVisual.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHClassesTest,"AshenHollow.Rules.ClassesAndLocomotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHClassesTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Spawn=[&](float X){ return World->SpawnActor<AAHCharacter>(FVector(X,0,100),FRotator::ZeroRotator,Params); };
    auto* Fighter=Spawn(0); auto* Barb=Spawn(3000); auto* Cleric=Spawn(6000); auto* Wizard=Spawn(9000); auto* Enemy=Spawn(9600);
    if(!Fighter||!Barb||!Cleric||!Wizard||!Enemy) { AddError(TEXT("Class fixture failed")); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
    Fighter->ChooseClass(EAHHeroClass::Fighter);
    for(int32 Race=0;Race<4;++Race) for(int32 Class=0;Class<4;++Class)
    {
        auto* Created=Spawn(12000+Race*4000+Class*800);
        if(!TestNotNull(TEXT("Ancestry/class fixture"),Created)) continue;
        Created->ChooseAncestry(static_cast<EAHAncestry>(Race));
        Created->ChooseClass(static_cast<EAHHeroClass>(Class));
        const int32 HP[]={12,14,10,8},AC[]={16,14,18,12},Initiative[]={1,2,0,2};
        TestEqual(TEXT("Ancestry HP applied once"),Created->MaxHealth,HP[Class]+(Race==2?1:0));
        TestEqual(TEXT("Elf armor trait"),Created->ArmorClass,AC[Class]+(Race==1?1:0));
        TestEqual(TEXT("Human initiative trait"),Created->InitiativeBonus,Initiative[Class]+(Race==0?1:0));
        Created->StartTurn();
        TestEqual(TEXT("Ancestry turn speed"),Created->Turn.Movement,Race>=2?750.f:900.f);
        Created->Dash(); TestEqual(TEXT("Dash uses ancestry speed"),Created->Turn.Movement,Race>=2?1500.f:1800.f);
        Created->ChooseAncestry(static_cast<EAHAncestry>((Race+1)%4));
        TestEqual(TEXT("Ancestry locks after character confirmation"),static_cast<int32>(Created->Ancestry),Race);
        Created->Destroy();
    }
    Fighter->ChooseClass(EAHHeroClass::Wizard);
    TestEqual(TEXT("Class cannot change after confirmation"),Fighter->MaxHealth,12);
    Barb->ChooseClass(EAHHeroClass::Barbarian); Barb->StartTurn(); Barb->UseClassAbility();
    TestTrue(TEXT("Rage activated"),Barb->bRaging);
    TestFalse(TEXT("Rage consumes bonus"),Barb->Turn.bBonus);
    TestTrue(TEXT("Rage preserves action"),Barb->Turn.bAction);
    Barb->ReceiveHit(5);
    TestEqual(TEXT("Temporary HP absorbs the resisted physical hit"),Barb->Health,14);
    TestEqual(TEXT("Rage halves physical damage before temporary HP"),Barb->TempHP,3);
    Barb->ReceiveHit(5,EAHDamageType::Force);
    TestEqual(TEXT("Force bypasses resistance and consumes remaining temporary HP"),Barb->Health,12);
    TestEqual(TEXT("Temporary HP is exhausted"),Barb->TempHP,0);
    Barb->FinishTurn(); Barb->StartTurn(); Barb->FinishTurn();
    TestFalse(TEXT("Inactive rage ends"),Barb->bRaging);
    Cleric->ChooseClass(EAHHeroClass::Cleric); Cleric->StartTurn(); Cleric->Health=5; Cleric->UseClassAbility();
    TestTrue(TEXT("Cleric heals within maximum"),Cleric->Health>5 && Cleric->Health<=10);
    TestEqual(TEXT("Cleric spends one slot"),Cleric->ClassCharges,1);
    TestFalse(TEXT("Cleric spell consumes action"),Cleric->Turn.bAction);
    TestTrue(TEXT("Cleric spell preserves bonus"),Cleric->Turn.bBonus);
    Wizard->ChooseClass(EAHHeroClass::Wizard); Wizard->StartTurn();
    Enemy->bEnemy=true; Enemy->Health=Enemy->MaxHealth=40;
    Wizard->UseClassAbility();
    TWeakObjectPtr<AAHMagicVisual> SpellVisual;
    for(TActorIterator<AAHMagicVisual> It(World);It;++It) SpellVisual=*It;
    TestTrue(TEXT("Missiles create a cosmetic flight"),SpellVisual.IsValid());
    TestEqual(TEXT("Missile damage waits for contact"),Enemy->Health,40);
    TestEqual(TEXT("Wizard spends a spell slot"),Wizard->ClassCharges,1);
    Wizard->OnMeleeImpactNotify();
    TestFalse(TEXT("Contact removes the cosmetic flight"),SpellVisual.IsValid());
    TestTrue(TEXT("Missiles resolve beyond melee reach"),Enemy->Health>=25 && Enemy->Health<=34);
    const int32 Health=Enemy->Health; Wizard->ResolveImpact();
    TestEqual(TEXT("Missiles cannot resolve twice"),Enemy->Health,Health);
    TestTrue(TEXT("Automatic spell does not fabricate d20"),Wizard->LastRollTime<0.f);

    auto* Mesh=Fighter->GetMesh(); Mesh->InitAnim(true);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    TestNotNull(TEXT("Native locomotion graph is bound"),Cast<UAHAnimInstance>(Mesh->GetAnimInstance()));
    Fighter->GetCharacterMovement()->Velocity=FVector(300,0,0);
    for(int32 I=0;I<5;++I) { ++GFrameCounter; Mesh->TickAnimation(.1f,false); Mesh->RefreshBoneTransforms(); }
    const FTransform Before=Mesh->GetSocketTransform(TEXT("foot_l"),RTS_Component);
    for(int32 I=0;I<3;++I) { ++GFrameCounter; Mesh->TickAnimation(.1f,false); Mesh->RefreshBoneTransforms(); }
    const FTransform After=Mesh->GetSocketTransform(TEXT("foot_l"),RTS_Component);
    AddInfo(FString::Printf(TEXT("Locomotion speed %.1f; foot before %s; after %s"),Fighter->GetVelocity().Size2D(),*Before.ToString(),*After.ToString()));
    TestTrue(TEXT("Walking moves the foot by more than idle sway"),FVector::Dist(Before.GetLocation(),After.GetLocation())>5.f);
    // ── Data tables ──────────────────────────────────────────────────────────
    // These numbers were hand-written switch cases before. If a table row drifts,
    // an archetype silently changes without anyone touching gameplay code.
    TestEqual(TEXT("Every archetype has a row"),AHRules::ClassCount(),static_cast<int32>(EAHHeroClass::Count));
    TestEqual(TEXT("Every ancestry has a row"),AHRules::AncestryCount(),static_cast<int32>(EAHAncestry::Count));
    {
        const int32 HP[]={12,14,10,8},AC[]={16,14,18,12},Atk[]={5,5,4,2};
        const int32 Sides[]={8,12,6,6},Mod[]={3,3,2,0},Init[]={1,2,0,2};
        const int32 Uses[]={1,2,2,2},Growth[]={8,9,7,6};
        for(int32 I=0;I<AHRules::ClassCount();++I)
        {
            const FAHClassSheet& Sheet=AHRules::Class(static_cast<EAHHeroClass>(I));
            TestEqual(TEXT("Archetype hit points"),Sheet.MaxHealth,HP[I]);
            TestEqual(TEXT("Archetype armour class"),Sheet.ArmorClass,AC[I]);
            TestEqual(TEXT("Archetype attack bonus"),Sheet.AttackBonus,Atk[I]);
            TestEqual(TEXT("Archetype damage die"),Sheet.DamageSides,Sides[I]);
            TestEqual(TEXT("Archetype damage bonus"),Sheet.DamageModifier,Mod[I]);
            TestEqual(TEXT("Archetype initiative"),Sheet.InitiativeBonus,Init[I]);
            TestEqual(TEXT("Archetype ability uses"),Sheet.ClassCharges,Uses[I]);
            TestEqual(TEXT("Archetype hit points per level"),Sheet.HitPointGrowth,Growth[I]);
            TestTrue(TEXT("Archetype names are filled in"),
                Sheet.Name && Sheet.FoeName && Sheet.AbilityName && Sheet.Icon
                && FCString::Strlen(Sheet.Name)>0 && FCString::Strlen(Sheet.FoeName)>0);
            TestTrue(TEXT("Archetype weapon has an authored clip"),
                static_cast<int32>(Sheet.Weapon)<static_cast<int32>(EAHWeaponKind::Count));
        }
        const float Speed[]={900.f,900.f,750.f,750.f,900.f,900.f,900.f};
        const int32 RInit[]={1,0,0,0,0,0,0},RArmour[]={0,1,0,0,0,1,0},RHealth[]={0,0,1,0,0,0,0};
        for(int32 I=0;I<AHRules::AncestryCount();++I)
        {
            const FAHAncestrySheet& Blood=AHRules::Ancestry(static_cast<EAHAncestry>(I));
            TestEqual(TEXT("Ancestry movement"),Blood.Movement,Speed[I]);
            TestEqual(TEXT("Ancestry initiative trait"),Blood.InitiativeBonus,RInit[I]);
            TestEqual(TEXT("Ancestry armour trait"),Blood.ArmorBonus,RArmour[I]);
            TestEqual(TEXT("Ancestry toughness"),Blood.HealthPerLevel,RHealth[I]);
        }
        TestTrue(TEXT("Only the halfling is lucky"),
            AHRules::Ancestry(EAHAncestry::Halfling).bLucky
            && !AHRules::Ancestry(EAHAncestry::Human).bLucky
            && !AHRules::Ancestry(EAHAncestry::Elf).bLucky
            && !AHRules::Ancestry(EAHAncestry::Dwarf).bLucky);
        TestTrue(TEXT("Only full casters carry spell slots"),
            AHRules::Class(EAHHeroClass::Cleric).bCaster
            && AHRules::Class(EAHHeroClass::Wizard).bCaster
            && !AHRules::Class(EAHHeroClass::Fighter).bCaster
            && !AHRules::Class(EAHHeroClass::Barbarian).bCaster);
    }
    // ── Ancestry traits ──────────────────────────────────────────────────────
    TestTrue(TEXT("Only the tiefling and dragonborn resist fire"),
        AHRules::Ancestry(EAHAncestry::Tiefling).bFireResistant
        && AHRules::Ancestry(EAHAncestry::Dragonborn).bFireResistant
        && !AHRules::Ancestry(EAHAncestry::Human).bFireResistant);
    TestTrue(TEXT("Only the half-orc refuses to fall"),
        AHRules::Ancestry(EAHAncestry::HalfOrc).bRelentless
        && !AHRules::Ancestry(EAHAncestry::Dwarf).bRelentless);
    TestTrue(TEXT("Only the dragonborn breathes"),
        AHRules::Ancestry(EAHAncestry::Dragonborn).bBreathWeapon
        && !AHRules::Ancestry(EAHAncestry::Tiefling).bBreathWeapon);

    {
        auto* Infernal=Spawn(40000);
        if(TestNotNull(TEXT("Tiefling fixture"),Infernal))
        {
            Infernal->ChooseAncestry(EAHAncestry::Tiefling);
            Infernal->ChooseClass(EAHHeroClass::Fighter);
            const int32 Full=Infernal->Health;
            Infernal->ReceiveHit(8,EAHDamageType::Fire);
            TestEqual(TEXT("Fire is halved for a tiefling"),Infernal->Health,Full-4);
            Infernal->Health=Full;
            Infernal->ReceiveHit(8,EAHDamageType::Physical);
            TestEqual(TEXT("Steel is not halved for a tiefling"),Infernal->Health,Full-8);
        }

        auto* Orc=Spawn(44000);
        if(TestNotNull(TEXT("Half-orc fixture"),Orc))
        {
            Orc->ChooseAncestry(EAHAncestry::HalfOrc);
            Orc->ChooseClass(EAHHeroClass::Fighter);
            TestEqual(TEXT("A heavier build hits harder"),Orc->DamageModifier,
                AHRules::Class(EAHHeroClass::Fighter).DamageModifier+1);
            Orc->ReceiveHit(500,EAHDamageType::Physical);
            TestTrue(TEXT("Relentless endurance refuses the first drop"),Orc->IsAlive() && Orc->Health==1);
            TestFalse(TEXT("Refusing to fall does not mark the character downed"),Orc->bDowned);
            Orc->ReceiveHit(500,EAHDamageType::Physical);
            TestTrue(TEXT("The second drop goes through"),Orc->bDowned);
            Orc->Rest();
            TestFalse(TEXT("A rest restores the refusal"),Orc->bRelentlessUsed);
        }

        auto* Drake=Spawn(48000);
        auto* Ahead=Spawn(48300);   // 300 cm in front, inside the 450 cm cone
        auto* Behind=Spawn(47550);  // 450 cm behind, so the one in front is strictly nearest
        if(TestNotNull(TEXT("Dragonborn fixture"),Drake)
           && TestNotNull(TEXT("Cone target"),Ahead) && TestNotNull(TEXT("Rear target"),Behind))
        {
            Drake->ChooseAncestry(EAHAncestry::Dragonborn);
            Drake->ChooseClass(EAHHeroClass::Fighter);
            Ahead->bEnemy=true;  Ahead->Health=Ahead->MaxHealth=60;
            Behind->bEnemy=true; Behind->Health=Behind->MaxHealth=60;
            TestFalse(TEXT("The breath needs an active turn"),Drake->CanUseRacialAbility());
            Drake->StartTurn();
            TestTrue(TEXT("The breath is ready on a fresh turn"),Drake->CanUseRacialAbility());
            Drake->UseRacialAbility();
            TestTrue(TEXT("The breath burns a target in the cone"),Ahead->Health<60);
            TestEqual(TEXT("The breath spares a target behind the dragonborn"),Behind->Health,60);
            TestFalse(TEXT("The breath spends the action"),Drake->Turn.bAction);
            Drake->StartTurn();
            TestFalse(TEXT("The breath is once per rest, not per turn"),Drake->CanUseRacialAbility());
            Drake->Rest();
            Drake->StartTurn();
            TestTrue(TEXT("A rest restores the breath"),Drake->CanUseRacialAbility());
        }
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
