#include "AHGameMode.h"
#include "AHCharacter.h"
#include "Engine/World.h"

bool AAHGameMode::CanWitness(const AAHCharacter* Observer,const AAHCharacter* Source,float Range) const
{
    if(!IsValid(Observer) || !IsValid(Source) || !Observer->IsAlive()) return false;
    if(FVector::DistSquared(Observer->GetActorLocation(),Source->GetActorLocation())>FMath::Square(Range)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AHPerception),false,Observer);
    Query.AddIgnoredActor(Source);
    for(const auto& Camp:Camps) for(const auto& Foe:Camp.Foes) if(Foe.IsValid()) Query.AddIgnoredActor(Foe.Get());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Observer->GetActorLocation()+FVector(0,0,35),
        Source->GetActorLocation()+FVector(0,0,35),ECC_Visibility,Query);
}

bool AAHGameMode::NoticesHero(AAHCharacter* Observer,AAHCharacter* Hero,float Range)
{
    if(!CanWitness(Observer,Hero,Range)) return false;
    if(!Hero->bSneaking) return true;
    const float Now=GetWorld()->GetTimeSeconds();
    float& Next=PerceptionChecks.FindOrAdd(Observer);
    if(Now<Next) return false;
    Next=Now+4.f;
    const int32 Wisdom=AHSheet::Total(Observer->Abilities,Observer->Ancestry).Mod(EAHAbility::Sabedoria);
    const int32 DC=10+AHSheet::Total(Hero->Abilities,Hero->Ancestry).Mod(EAHAbility::Destreza)
        +(Hero->HeroClass==EAHHeroClass::Rogue?AHSheet::Proficiency(Hero->Level):0);
    const int32 Roll=FMath::RandRange(1,20)+Wisdom;
    Hero->AddLog(FString::Printf(TEXT("%s: percepcao %d contra furtividade %d - %s"),*Observer->EnemyName,Roll,DC,Roll>=DC?TEXT("avistado"):TEXT("nao percebeu")));
    return Roll>=DC;
}
