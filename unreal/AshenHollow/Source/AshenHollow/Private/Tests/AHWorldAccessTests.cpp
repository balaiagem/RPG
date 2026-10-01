#include "Misc/AutomationTest.h"
#include "AHArena.h"
#include "AHGameMode.h"
#include "AHCharacter.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHKitAssetsTest, "AshenHollow.Assets.KitBindingsAndCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAHKitAssetsTest::RunTest(const FString&)
{
    const TCHAR* Names[] = {TEXT("arvore_a"),TEXT("arvore_b"),TEXT("arvore_c"),TEXT("portao"),TEXT("arco")};
    for (const TCHAR* Name : Names)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/AshenHollow/Kit/Meshes/SM_Kit_%s"),Name));
        if (!TestNotNull(Name,Mesh)) continue;
        for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
        {
            UMaterialInterface* Skin=Slot.MaterialInterface;
            TestTrue(FString::Printf(TEXT("%s %s uses saved kit material"),Name,*Slot.MaterialSlotName.ToString()),
                Skin && Skin->GetPathName().StartsWith(TEXT("/Game/AshenHollow/Kit/Materials/")));
            if (Skin) TestTrue(TEXT("Material supports instancing"),Skin->GetMaterial()->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
        }
        UBodySetup* Body=Mesh->GetBodySetup();
        if (!TestNotNull(TEXT("Collision body"),Body)) continue;
        TestTrue(TEXT("Simple collision exists"),Body->AggGeom.GetElementCount()>0);
        TestEqual(TEXT("No auto-generated convex hull remains"),Body->AggGeom.ConvexElems.Num(),0);
        const bool bTree=FString(Name).StartsWith(TEXT("arvore"));
        TestEqual(TEXT("Authored collision box count"),Body->AggGeom.BoxElems.Num(),bTree?1:2);
        for(const FKBoxElem& Box:Body->AggGeom.BoxElems)
        {
            const FBox Bounds=Box.CalcAABB(FTransform::Identity,1.f);
            if(bTree) TestTrue(TEXT("Tree box only surrounds trunk"),Box.X<100 && Box.Y<100);
            else TestFalse(TEXT("Gateway leaves capsule width open"),Bounds.ExpandBy(FVector(42,42,0)).IsInside(FVector(0,0,100)));
        }
        for(const FKConvexElem& Hull:Body->AggGeom.ConvexElems)
        {
            const FBox Bounds=Hull.ElemBox;
            AddInfo(FString::Printf(TEXT("%s collision %s"),Name,*Bounds.ToString()));
            if(FString(Name).StartsWith(TEXT("arvore")))
                TestTrue(TEXT("Tree hull only surrounds trunk"),Bounds.GetSize().X<100 && Bounds.GetSize().Y<100);
            else
                TestFalse(TEXT("Gateway collision leaves centre open"),Bounds.ExpandBy(FVector(42,42,0)).IsInside(FVector(0,0,100)));
        }
    }
    return true;
}

class FAHCheckDungeonPaths final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Began=FPlatformTime::Seconds(), MovedAt=0;
    int32 Index=0;
    FVector Outside;
    FVector FirstOutside, FirstRoom;
    TWeakObjectPtr<AAHCharacter> TestFoe;
    double ChaseAt=0;
    int32 ChasePhase=0;
    TArray<FAHLandmark> Dungeons;
public:
    explicit FAHCheckDungeonPaths(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.WorldType==EWorldType::Game) { World=Context.World(); break; }
        auto* Mode=World?World->GetAuthGameMode<AAHGameMode>():nullptr;
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Hero=PC?Cast<AAHCharacter>(PC->GetPawn()):nullptr;
        if(!Mode || !Hero || Mode->Plan.Pieces.IsEmpty())
        {
            if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("World unavailable")); return true; }
            return false;
        }
        Mode->SetActorTickEnabled(false);
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if(Dungeons.IsEmpty())
        {
            for(const auto& Mark:Mode->Plan.Landmarks)
                if(Mark.Kind==EAHSite::Masmorra) Dungeons.Add(Mark);
            if(Dungeons.IsEmpty()) { Test->AddError(TEXT("No dungeon generated")); return true; }
        }
        if(Index>=Dungeons.Num())
        {
            if(ChasePhase==0)
            {
                Hero->SetActorLocation(FirstOutside+FVector(0,0,110),false);
                Hero->Turn.bReaction=false;
                Mode->bExploring=false;
                ChaseAt=FPlatformTime::Seconds(); ChasePhase=1;
                return false;
            }
            if(ChasePhase==1)
            {
                if(FPlatformTime::Seconds()-ChaseAt<5) return false;
                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
                TestFoe=World->SpawnActor<AAHCharacter>(FirstRoom+FVector(0,0,110),FRotator::ZeroRotator,Params);
                if(!Test->TestNotNull(TEXT("Indoor AI fixture spawns"),TestFoe.Get())) return true;
                TestFoe->BecomeEnemy(42);
                TestFoe->HeroClass=EAHHeroClass::Fighter;
                TestFoe->RangedRange=0;
                TestFoe->ClassCharges=0;
                TestFoe->StartTurn(); TestFoe->Turn.Movement=6000;
                Mode->TurnStarted=World->GetTimeSeconds()-1;
                ChaseAt=FPlatformTime::Seconds(); ChasePhase=2;
                return false;
            }
            if(FVector::Dist2D(TestFoe->GetActorLocation(),Hero->GetActorLocation())<=190)
            {
                Test->AddInfo(TEXT("Indoor enemy physically walked through corridors and gateway to hero"));
                // Exercise the real damage entry point: a nearby camp joins once,
                // and joining cannot steal the active player's turn.
                FNavLocation Nearby;
                if(Nav && Nav->GetRandomReachablePointInRadius(TestFoe->GetNavAgentLocation(),180,Nearby))
                {
                    FActorSpawnParameters Params;
                    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
                    auto* Ally=World->SpawnActor<AAHCharacter>(Nearby.Location+FVector(0,0,100),FRotator::ZeroRotator,Params);
                    auto* Far=World->SpawnActor<AAHCharacter>(FirstRoom+FVector(5000,0,100),FRotator::ZeroRotator,Params);
                    if(Test->TestNotNull(TEXT("Alert ally spawns"),Ally) && Test->TestNotNull(TEXT("Distant fixture spawns"),Far))
                    {
                        Ally->BecomeEnemy(43); Far->BecomeEnemy(44);
                        FAHCamp NearCamp; NearCamp.bAwake=true; NearCamp.Centre=Ally->GetActorLocation(); NearCamp.Foes.Add(Ally);
                        FAHCamp FarCamp; FarCamp.bAwake=true; FarCamp.Centre=Far->GetActorLocation(); FarCamp.Foes.Add(Far);
                        Mode->Camps.Add(NearCamp); Mode->Camps.Add(FarCamp);
                        Mode->Order={Hero,TestFoe.Get()}; Mode->ActiveIndex=0;
                        Mode->bStarted=true; Mode->bFinished=false;
                        TestFoe->Health=100;
                        TestFoe->ReceiveHit(1);
                        Test->TestTrue(TEXT("Damage alerts reachable nearby ally"),Mode->Order.Contains(Ally));
                        Test->TestFalse(TEXT("Damage does not alert distant camp"),Mode->Order.Contains(Far));
                        Test->TestTrue(TEXT("Alert preserves current actor"),Mode->ActiveCharacter()==Hero);
                        const int32 JoinedCount=Mode->Order.Num();
                        TestFoe->ReceiveHit(1);
                        Test->TestEqual(TEXT("Repeated damage does not duplicate initiative entries"),Mode->Order.Num(),JoinedCount);
                    }
                    if(Ally) Ally->Destroy();
                    if(Far) Far->Destroy();
                }
                else Test->AddError(TEXT("No reachable nearby alert fixture location"));
                TestFoe->Destroy(); return true;
            }
            if(FPlatformTime::Seconds()-ChaseAt>30)
            {
                Test->AddError(FString::Printf(TEXT("Enemy failed to exit dungeon; remaining distance %.0f"),
                    FVector::Dist2D(TestFoe->GetActorLocation(),Hero->GetActorLocation())));
                TestFoe->Destroy(); return true;
            }
            return false;
        }
        const FVector Centre=Dungeons[Index].Where;
        if(MovedAt==0)
        {
            const FAHArenaPiece* Gate=nullptr;
            for(const auto& Piece:Mode->Plan.Pieces)
                if(Piece.MeshPath.EndsWith(TEXT("SM_Kit_portao")) && FVector::Dist2D(Piece.Location,Centre)<2200)
                    { Gate=&Piece; break; }
            if(!Gate) { Test->AddError(TEXT("Dungeon missing gateway")); ++Index; return false; }
            const FVector Axis=Gate->Rotation.Vector()*650;
            Outside=Gate->Location+(FVector::Dist2D(Gate->Location+Axis,Centre)>FVector::Dist2D(Gate->Location-Axis,Centre)?Axis:-Axis);
            Outside.Z=Mode->TerrainZ(Outside.X,Outside.Y);
            if(Index==0) FirstOutside=Outside;
            Hero->SetActorLocation(Outside+FVector(0,0,110),false);
            MovedAt=FPlatformTime::Seconds();
            return false;
        }
        if(FPlatformTime::Seconds()-MovedAt<5 || (Nav && Nav->IsNavigationBuildInProgress() && FPlatformTime::Seconds()-MovedAt<45)) return false;
        if(!Nav) { Test->AddError(TEXT("Navigation missing")); return true; }
        for(const auto& Camp:Mode->Plan.Camps)
        {
            if(!Camp.bIndoor || FVector::Dist2D(Camp.Where,Centre)>2200) continue;
            FVector Target=Camp.Where; Target.Z=Mode->TerrainZ(Target.X,Target.Y);
            if(Index==0 && FirstRoom.IsNearlyZero()) FirstRoom=Target;
            for(int32 Direction=0;Direction<2;++Direction)
            {
                UNavigationPath* Path=Nav->FindPathToLocationSynchronously(World,Direction?Target:Outside,Direction?Outside:Target);
                Test->TestTrue(FString::Printf(TEXT("Dungeon %d room reachable %s"),Index,Direction?TEXT("outward"):TEXT("inward")),Path && Path->IsValid() && !Path->IsPartial());
            }
        }
        ++Index; MovedAt=0;
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHDungeonPathsTest,"AshenHollow.WorldAccess.Dungeons",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FAHDungeonPathsTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FAHCheckDungeonPaths(this));
    return true;
}
#endif
