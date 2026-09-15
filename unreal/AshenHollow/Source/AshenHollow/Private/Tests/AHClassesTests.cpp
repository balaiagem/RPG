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
    Fighter->ChooseClass(EAHHeroClass::Wizard);
    TestEqual(TEXT("Class cannot change after confirmation"),Fighter->MaxHealth,12);
    Barb->ChooseClass(EAHHeroClass::Barbarian); Barb->StartTurn(); Barb->UseClassAbility();
    TestTrue(TEXT("Rage activated"),Barb->bRaging);
    TestFalse(TEXT("Rage consumes bonus"),Barb->Turn.bBonus);
    TestTrue(TEXT("Rage preserves action"),Barb->Turn.bAction);
    Barb->ReceiveHit(5);
    TestEqual(TEXT("Temporary HP absorbs the resisted physical hit"),Barb->Health,14);
    TestEqual(TEXT("Rage halves physical damage before temporary HP"),Barb->TempHP,3);
    Barb->ReceiveHit(5,false);
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
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
