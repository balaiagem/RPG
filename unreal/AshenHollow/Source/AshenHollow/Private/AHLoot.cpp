#include "AHGameMode.h"
#include "AHCharacter.h"
#include "AHWildEnemy.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

void AAHGameMode::DropLoot(AAHCharacter* Foe)
{
    if(!Foe || !Foe->bEnemy) return;
    FAHLoot Drop; Drop.Where=Foe->GetActorLocation(); Drop.Where.Z-=75;
    const auto* Beast=Cast<AAHWildEnemy>(Foe);
    Drop.Gold=Beast?0:FMath::RandRange(5,18);
    static const TCHAR* Items[]={TEXT("pocao_cura"),TEXT("couro_batido"),TEXT("capuz"),TEXT("espada_curta"),TEXT("botas"),TEXT("escudo")};
    Drop.Item=Beast?TEXT("racao"):Items[FMath::RandRange(0,UE_ARRAY_COUNT(Items)-1)];
    if(!AHItems::Find(Drop.Item)) Drop.Item=TEXT("pocao_cura");
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(auto* Marker=GetWorld()->SpawnActor<AStaticMeshActor>(Drop.Where,FRotator::ZeroRotator,P))
    {
        auto* Mesh=Marker->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/AshenHollow/LifeKit/Materials/M_Life_Leather")));
        Mesh->SetWorldScale3D(FVector(.3,.22,.2)); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCanEverAffectNavigation(false);
        Drop.Marker=Marker; ObstacleActors.Add(Marker);
    }
    Loot.Add(Drop);
}

bool AAHGameMode::TakeNearbyLoot(AAHCharacter* Hero)
{
    if(!Hero || !Hero->IsAlive() || !Hero->bCharacterReady || !bExploring || Hero->IsBusy()) return false;
    int32 Closest=INDEX_NONE; double Distance=240;
    for(int32 I=0;I<Loot.Num();++I)
    {
        const double Away=FVector::Dist(Hero->GetActorLocation(),Loot[I].Where);
        if(!Loot[I].bTaken && Away<Distance) { Closest=I; Distance=Away; }
    }
    if(Closest==INDEX_NONE) return false;
    auto& Drop=Loot[Closest]; Drop.bTaken=true;
    Hero->Gold+=Drop.Gold; Hero->Carry(Drop.Item);
    const auto* Item=AHItems::Find(Drop.Item);
    Hero->Feedback=FString::Printf(TEXT("Saque: %d ouro + %s"),Drop.Gold,Item?Item->Nome:*Drop.Item);
    Hero->AddLog(Hero->Feedback);
    if(Drop.Marker.IsValid()) { ObstacleActors.Remove(Drop.Marker.Get()); Drop.Marker->Destroy(); }
    return true;
}
