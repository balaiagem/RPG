#include "AHLifeAudioComponent.h"
#include "AHCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"

UAHLifeAudioComponent::UAHLifeAudioComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=.05f;
}
void UAHLifeAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    Previous=GetOwner()->GetActorLocation();
    for(int32 I=1;I<=3;++I)
        Steps.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/AshenHollow/Audio/Footstep%d"),I)));
    HitSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/AshenHollow/Audio/Impact"));
    Falloff=NewObject<USoundAttenuation>(this);
    // The isometric listener is well above the feet. Keep nearby Foley audible
    // while distant camps fade away instead of mixing every actor at full volume.
    Falloff->Attenuation.bAttenuate=true;
    Falloff->Attenuation.bSpatialize=true;
    Falloff->Attenuation.AttenuationShapeExtents=FVector(1800,0,0);
    Falloff->Attenuation.FalloffDistance=2400;
}
void UAHLifeAudioComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime,TickType,Function);
    const FVector Now=GetOwner()->GetActorLocation();
    const float Travel=FVector::Dist2D(Now,Previous);
    Previous=Now;
    auto* Character=Cast<AAHCharacter>(GetOwner());
    if(!Character || !Character->IsAlive() || !Character->GetCharacterMovement()->IsMovingOnGround()
        || Travel<1 || Travel>120) { Distance=0; return; }
    Distance+=Travel;
    const float Stride=Character->GetVelocity().Size2D()>350?170.f:90.f;
    if(Distance<Stride || Steps.IsEmpty()) return;
    Distance=FMath::Fmod(Distance,Stride);
    USoundBase* Sound=Steps[StepIndex++%Steps.Num()];
    if(Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,Now,.32f,FMath::FRandRange(.94f,1.06f),0,Falloff);
}
void UAHLifeAudioComponent::Impact()
{
    const float Now=GetWorld()->GetTimeSeconds();
    if(!HitSound || Now-LastImpact<.08f) return;
    LastImpact=Now;
    UGameplayStatics::PlaySoundAtLocation(this,HitSound,GetOwner()->GetActorLocation(),.45f,FMath::FRandRange(.94f,1.06f),0,Falloff);
}
