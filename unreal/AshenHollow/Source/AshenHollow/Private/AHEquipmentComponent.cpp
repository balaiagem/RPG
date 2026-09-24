#include "AHEquipmentComponent.h"
#include "AHCharacter.h"
#include "AHClassData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

// ── Weapon art table ─────────────────────────────────────────────────────────
// One row per EAHWeaponKind, in enum order: Sword, Axe, Mace, Staff, Bow.
//
// Leave MeshPath as TEXT("") and that weapon is built from engine primitives
// exactly as it always was. An empty or wrong path can therefore never break a
// build, crash the game, or leave a character empty-handed — it just falls back.
//
// Static and skeletal meshes are both accepted -- AttachArt tries each in turn --
// so a path can name either without the caller caring which a pack happened to ship.
//
// To use a different mesh: in the Content Browser, right-click the asset, choose
// "Copy Reference", and paste the part between the quotes, e.g.
//   /Game/StylizedCharacter/Meshes/Item/Weapons/Sword/SK_Sword_1H_Newbie_02
// Then tune the row:
//   bAutoUpright       leave true and the rotation is measured from the mesh (see
//                      UprightRotation below) instead of guessed. Set it false only
//                      to pin an awkward asset by hand, and then Rotation is used.
//   Offset / Rotation  where the grip sits in the hand. Rotation is ignored while
//                      bAutoUpright is true.
//   Scale              pack meshes are usually 1.0; drop toward .8 if it looks big.
//   TipHeight          centimetres from the grip to the business end. This only
//                      moves where spell and impact effects spawn. Nothing in the
//                      combat rules reads it.
// Shield fields work the same way and are used only by Sword and Mace.
struct FAHWeaponArt
{
    const TCHAR* MeshPath;
    FVector      Offset;
    FRotator     Rotation;
    /** True: derive the rotation from the mesh itself and ignore Rotation. */
    bool         bAutoUpright;
    float        Scale;
    float        TipHeight;
    const TCHAR* ShieldMeshPath;
    FVector      ShieldOffset;
    FRotator     ShieldRotation;
    float        ShieldScale;
    /**
     * Clip played on the weapon's own skeleton when it fires. The bow meshes ship
     * with a draw-and-release animation on SKEL_Weapon_Bow, which is what makes an
     * arrow look like it was shot rather than spawned. Empty for weapons that have
     * none, and the call then does nothing.
     */
    const TCHAR* ShotAnimation;
};

/**
 * Rotation that stands an authored weapon up along the grip's +Z, which is the
 * axis the primitive weapons were built on and the one the hand animations were
 * tuned against.
 *
 * Packs disagree about which axis a blade runs along, and guessing costs a whole
 * build to find out, so measure the mesh instead of assuming. Two layouts cover
 * essentially everything: the handle at the asset origin, where the bounds centre
 * sits up the weapon's length and its sign tells us which way is up; and a mesh
 * centred on itself, where the offset says nothing and the longest side of the
 * box is the length. A weapon that is already vertical measures as vertical and
 * gets an identity rotation, so this is safe to leave on.
 */
static FRotator UprightRotation(const FBoxSphereBounds& Bounds)
{
    FVector Along=Bounds.Origin;
    if(Along.Size()<Bounds.SphereRadius*0.25)
    {
        const FVector Extent=Bounds.BoxExtent;
        Along = (Extent.X>=Extent.Y && Extent.X>=Extent.Z) ? FVector(1,0,0)
              : (Extent.Y>=Extent.Z)                       ? FVector(0,1,0)
              :                                              FVector(0,0,1);
    }
    const double AbsX=FMath::Abs(Along.X),AbsY=FMath::Abs(Along.Y),AbsZ=FMath::Abs(Along.Z);
    if(AbsZ>=AbsX && AbsZ>=AbsY) return Along.Z>=0 ? FRotator::ZeroRotator : FRotator(180.f,0.f,0.f);
    // Pitch +90 carries +X onto +Z; roll -90 carries +Y onto +Z.
    if(AbsX>=AbsY)               return FRotator(Along.X>=0?90.f:-90.f,0.f,0.f);
    return FRotator(0.f,0.f,Along.Y>=0?-90.f:90.f);
}

static const FAHWeaponArt& WeaponArt(EAHWeaponKind Kind)
{
    // Every path below points at the "Newbie" stylized set, so the weapons and the
    // shield read as one family. Mace is the one gap: neither pack ships a blunt
    // weapon, so it stays on the primitive fallback until we find one.
    //
    // The variant follows the sheet, not taste: the barbarian rolls 1d12, which is a
    // greataxe, so Axe points at the two-handed model rather than the one-handed one.
    static const FAHWeaponArt Table[]=
    {
        // Sword
        { TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Sword/SK_Sword_1H_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, true, 1.f,  85.f,
          TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Shield/SK_Shield_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, 1.f, TEXT("") },
        // Axe -- 1d12 on the barbarian's sheet, so the two-handed model.
        { TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Axe/SK_Axe_2HL_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, true, 1.f,  75.f,
          TEXT(""), FVector::ZeroVector, FRotator::ZeroRotator, 1.f, TEXT("") },
        // Mace -- no blunt mesh in either pack; primitive fallback on purpose.
        { TEXT(""), FVector::ZeroVector, FRotator::ZeroRotator, true, 1.f,  68.f,
          TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Shield/SK_Shield_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, 1.f, TEXT("") },
        // Staff
        { TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Staff/SK_Staff_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, true, 1.f, 110.f,
          TEXT(""), FVector::ZeroVector, FRotator::ZeroRotator, 1.f, TEXT("") },
        // Bow -- rogue and ranger. No shield: both hands are on the bow, and a
        // shield was wrong on the rogue even before it had one.
        // TipHeight is low because an arrow leaves from the grip, not from a tip.
        { TEXT("/Game/StylizedCharacter/Meshes/Item/Weapons/Bow/SK_Bow_Newbie_01"),
          FVector::ZeroVector, FRotator::ZeroRotator, true, 1.f,  30.f,
          TEXT(""), FVector::ZeroVector, FRotator::ZeroRotator, 1.f,
          TEXT("/Game/StylizedCharacter/Animations/Item/Weapons/Bow/A_Bow_Attack") },
    };
    static_assert(UE_ARRAY_COUNT(Table)==static_cast<int32>(EAHWeaponKind::Count),
        "One weapon art row per EAHWeaponKind, in enum order.");
    const int32 Index=FMath::Clamp(static_cast<int32>(Kind),0,static_cast<int32>(EAHWeaponKind::Count)-1);
    return Table[Index];
}

UAHEquipmentComponent::UAHEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> C(TEXT("/Engine/BasicShapes/Cube")),S(TEXT("/Engine/BasicShapes/Sphere")),Y(TEXT("/Engine/BasicShapes/Cylinder"));
    Cube=C.Object; Sphere=S.Object; Cylinder=Y.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/AshenHollow/FX/M_WeaponSteel")),G(TEXT("/Game/AshenHollow/FX/M_WeaponGold")),L(TEXT("/Game/AshenHollow/FX/M_WeaponLeather")),E(TEXT("/Game/AshenHollow/FX/M_CombatGlow"));
    Steel=M.Object; Gold=G.Object; Leather=L.Object; Glow=E.Object;
}
USceneComponent* UAHEquipmentComponent::Anchor(FName Bone)
{
    auto* Character=CastChecked<AAHCharacter>(GetOwner());
    auto* Node=NewObject<USceneComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Node);
    Node->SetupAttachment(Character->GetMesh(),Bone); Node->RegisterComponent();
    // Establish a vertical resting grip; subsequent wrist motion drives the weapon.
    Node->SetWorldRotation(Character->GetActorRotation());
    Parts.Add(Node); return Node;
}
void UAHEquipmentComponent::Part(USceneComponent* Parent,UStaticMesh* Mesh,UMaterialInterface* Material,FVector Position,FVector Scale,FRotator Rotation)
{
    auto* P=NewObject<UStaticMeshComponent>(GetOwner()); GetOwner()->AddInstanceComponent(P);
    // SetStaticMesh updates navigation even before registration. Disable it first
    // so a cosmetic weapon cannot enqueue geometry at the character's spawn.
    P->SetCanEverAffectNavigation(false);
    P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetGenerateOverlapEvents(false);
    P->SetupAttachment(Parent); P->SetStaticMesh(Mesh);
    // A null material means "keep whatever the asset ships with". Only the
    // primitive fallback passes one of our four project materials.
    if(Material) P->SetMaterial(0,Material);
    P->SetRelativeLocation(Position); P->SetRelativeScale3D(Scale); P->SetRelativeRotation(Rotation);
    P->RegisterComponent(); Parts.Add(P); LastAttached=P;
}
void UAHEquipmentComponent::SkeletalPart(USceneComponent* Parent,USkeletalMesh* Mesh,FVector Position,FVector Scale,FRotator Rotation)
{
    auto* P=NewObject<USkeletalMeshComponent>(GetOwner()); GetOwner()->AddInstanceComponent(P);
    P->SetCanEverAffectNavigation(false);
    P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetGenerateOverlapEvents(false);
    P->SetupAttachment(Parent); P->SetSkinnedAssetAndUpdate(Mesh,true);
    P->SetRelativeLocation(Position); P->SetRelativeScale3D(Scale); P->SetRelativeRotation(Rotation);
    P->RegisterComponent(); Parts.Add(P); LastAttached=P;
}
bool UAHEquipmentComponent::AttachArt(USceneComponent* Parent,const TCHAR* Path,FVector Offset,FRotator Rotation,float Scale,bool bAutoUpright)
{
    if(!Parent || !Path || !*Path) return false;
    // Quiet flags: an unfilled, renamed or not-yet-imported path is an expected
    // state, not an error. Weapon packs ship either kind of mesh, so try both.
    if(UStaticMesh* AsStatic=LoadObject<UStaticMesh>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet))
    {
        const FRotator StaticGrip=bAutoUpright?UprightRotation(AsStatic->GetBounds()):Rotation;
        Part(Parent,AsStatic,nullptr,Offset,FVector(Scale),StaticGrip); return true;
    }
    if(USkeletalMesh* AsSkeletal=LoadObject<USkeletalMesh>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet))
    {
        const FRotator SkeletalGrip=bAutoUpright?UprightRotation(AsSkeletal->GetBounds()):Rotation;
        SkeletalPart(Parent,AsSkeletal,Offset,FVector(Scale),SkeletalGrip); return true;
    }
    return false;
}
void UAHEquipmentComponent::BuildPrimitiveWeapon(EAHWeaponKind Kind)
{
    const bool Staff=Kind==EAHWeaponKind::Staff;
    Part(Grip,Cylinder,Leather,FVector(0,0,Staff?15:0),FVector(.045,.045,Staff?1.45:.3));
    for(float Z:{-12.f,12.f}) Part(Grip,Cylinder,Gold,FVector(0,0,Z),FVector(.065,.065,.035));
    if(Kind==EAHWeaponKind::Sword)
    {
        Part(Grip,Cube,Gold,FVector(0,0,17),FVector(.05,.27,.045));
        Part(Grip,Cube,Steel,FVector(0,0,48),FVector(.024,.085,.60));
        Part(Grip,Cube,Gold,FVector(0,0,45),FVector(.03,.015,.47));
        Part(Grip,Sphere,Gold,FVector(0,0,-17),FVector(.09));
    }
    else if(Kind==EAHWeaponKind::Axe)
    {
        Part(Grip,Cylinder,Leather,FVector(0,0,27),FVector(.055,.055,.85));
        for(float Sign:{-1.f,1.f}) Part(Grip,Cube,Steel,FVector(0,Sign*16,60),FVector(.045,.29,.28),FRotator(0,0,Sign*18));
        Part(Grip,Sphere,Gold,FVector(0,0,61),FVector(.10));
    }
    else if(Kind==EAHWeaponKind::Mace)
    {
        Part(Grip,Cylinder,Steel,FVector(0,0,30),FVector(.04,.04,.55));
        for(int I=0;I<4;++I) Part(Grip,Cube,Gold,FVector(0,0,57),FVector(.22,.035,.23),FRotator(0,I*45,0));
        TipHeight=68;
    }
    else // Staff, and Bow if its mesh ever goes missing: a pole reads better than nothing.
    {
        Part(Grip,Cylinder,Gold,FVector(0,0,83),FVector(.13,.13,.09));
        Part(Grip,Sphere,Glow,FVector(0,0,94),FVector(.17,.17,.31));
        TipHeight=110;
    }
}
void UAHEquipmentComponent::BuildPrimitiveShield(USceneComponent* Parent)
{
    Part(Parent,Cylinder,Gold,FVector(0,0,0),FVector(.50,.50,.055),FRotator(90,0,0));
    Part(Parent,Cylinder,Steel,FVector(4,0,0),FVector(.44,.44,.035),FRotator(90,0,0));
    Part(Parent,Sphere,Gold,FVector(7,0,0),FVector(.1));
}
void UAHEquipmentComponent::Configure(EAHWeaponKind Kind)
{
    for(int32 I=Parts.Num()-1;I>=0;--I) if(IsValid(Parts[I])) Parts[I]->DestroyComponent();
    Parts.Reset(); Grip=Anchor(TEXT("hand_r")); TipHeight=75;

    WeaponMesh=nullptr; ShotPath=nullptr;
    const FAHWeaponArt& Art=WeaponArt(Kind);
    if(AttachArt(Grip,Art.MeshPath,Art.Offset,Art.Rotation,Art.Scale,Art.bAutoUpright))
    {
        TipHeight=Art.TipHeight;
        // Only a skeletal weapon can be animated, so the handle stays null for the
        // primitive fallback and PlayShot quietly does nothing.
        WeaponMesh=Cast<USkeletalMeshComponent>(LastAttached);
        ShotPath=Art.ShotAnimation;
    }
    else BuildPrimitiveWeapon(Kind);

    if(Kind==EAHWeaponKind::Sword || Kind==EAHWeaponKind::Mace)
    {
        auto* ShieldArm=Anchor(TEXT("hand_l"));
        // A shield is a disc, so its longest axis is its diameter: standing that
        // upright would turn it edge-on. The off hand is never auto-uprighted.
        if(!AttachArt(ShieldArm,Art.ShieldMeshPath,Art.ShieldOffset,Art.ShieldRotation,Art.ShieldScale,false))
            BuildPrimitiveShield(ShieldArm);
    }
}
void UAHEquipmentComponent::PlayShot()
{
    if(!WeaponMesh || !ShotPath || !*ShotPath) return;
    UAnimationAsset* Release=LoadObject<UAnimationAsset>(nullptr,ShotPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
    if(!Release) return;
    // The weapon carries no animation blueprint of its own, so single-node
    // playback is exactly the right mode here.
    WeaponMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    WeaponMesh->PlayAnimation(Release,false);
}
FVector UAHEquipmentComponent::Tip() const { return Grip?Grip->GetComponentTransform().TransformPosition(FVector(0,0,TipHeight)):GetOwner()->GetActorLocation(); }
