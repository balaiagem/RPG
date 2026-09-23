#include "AHCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHAdvancedClassesTest,"AshenHollow.Rules.AdvancedClasses",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHAdvancedClassesTest::RunTest(const FString& Parameters)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Spawn=[&](EAHHeroClass Class,float X) { auto* H=World->SpawnActor<AAHCharacter>(FVector(X,0,100),FRotator::ZeroRotator,P); H->ChooseClass(Class); return H; };
    auto* S=Spawn(EAHHeroClass::Sorcerer,0); auto* R=Spawn(EAHHeroClass::Rogue,3000);
    auto* Pala=Spawn(EAHHeroClass::Paladin,6000); auto* Hunter=Spawn(EAHHeroClass::Ranger,9000);
    auto* Foe=Spawn(EAHHeroClass::Fighter,9600); Foe->bEnemy=true; Foe->Health=Foe->MaxHealth=1000;
    TestEqual(TEXT("Sorcerer level one known limit"),S->PreparedLimit(),2);
    TestTrue(TEXT("Sorcerer arcane list"),S->IsSpellAvailable(EAHSpell::MagicMissile));
    TestFalse(TEXT("Sorcerer excludes divine spells"),S->IsSpellAvailable(EAHSpell::HealingWord));
    S->GainExperience(900); S->Rest(); S->StartTurn(); TestEqual(TEXT("Level three sorcery pool"),S->SorceryPoints,3);
    S->ClassCharges=0; S->UseProgressionAbility(); TestEqual(TEXT("Conversion restores slot"),S->ClassCharges,1); TestEqual(TEXT("Conversion costs two points"),S->SorceryPoints,1);
    S->StartTurn(); S->ToggleEmpower(); Foe->SetActorLocation(FVector(600,0,100));
    TestTrue(TEXT("Empowered spell cast"),S->CastSpell(EAHSpell::MagicMissile,Foe));
    TestEqual(TEXT("Empower costs one point"),S->SorceryPoints,0); TestFalse(TEXT("Empower only next spell"),S->bEmpowerNext); S->ResolveImpact();
    TestTrue(TEXT("Empower keeps valid missile damage"),Foe->Health>=985 && Foe->Health<=994);
    Pala->StartTurn(); Pala->Health=1; Pala->UseClassUtility(); TestEqual(TEXT("Lay on hands level one pool"),Pala->Health,6); TestEqual(TEXT("Lay on hands pool consumed"),Pala->LayOnHands,0);
    TestEqual(TEXT("Paladin no level one slots"),Pala->MaxSpellSlots(1),0);
    Pala->GainExperience(300); Pala->Rest(); Pala->StartTurn(); TestEqual(TEXT("Paladin level two slots"),Pala->ClassCharges,2);
    Pala->UseProgressionAbility(); FAHDiceOutcome Miss; Pala->AddWeaponRiders(Foe,Miss,false);
    TestEqual(TEXT("Miss preserves smite slot"),Pala->ClassCharges,2); TestTrue(TEXT("Miss preserves armed smite"),Pala->bSmiteArmed);
    FAHDiceOutcome Hit; Hit.bSuccess=true; Hit.bCritical=true; Hit.Damage=5;
    const int32 Radiant=Pala->AddWeaponRiders(Foe,Hit,false);
    TestTrue(TEXT("Critical smite four dice"),Radiant>=4 && Radiant<=32); TestEqual(TEXT("Smite spends exactly one slot"),Pala->ClassCharges,1);
    Foe->bRaging=true; Foe->Health=1000; Foe->ReceiveHit(10,EAHDamageType::Physical,10); TestEqual(TEXT("Rage does not halve radiant smite"),Foe->Health,985); Foe->bRaging=false;
    R->GainExperience(900); R->Rest(); R->StartTurn(); R->SteadyAim(); TestTrue(TEXT("Steady aim grants next attack advantage"),R->bSteadyAim); TestEqual(TEXT("Steady aim forbids movement"),R->Turn.Movement,0.f);
    R->Dash(); TestEqual(TEXT("Dash cannot bypass steady aim"),R->Turn.Movement,0.f);
    Hit=FAHDiceOutcome(); Hit.bSuccess=true; Hit.Advantage=1; Hit.Damage=5; R->AddWeaponRiders(Foe,Hit,true);
    TestTrue(TEXT("Level three sneak adds two dice"),Hit.Damage>=7 && Hit.Damage<=17); const int32 Once=Hit.Damage; R->AddWeaponRiders(Foe,Hit,true); TestEqual(TEXT("Sneak only once per turn"),Hit.Damage,Once);
    R->StartTurn(); R->UseClassUtility(); TestTrue(TEXT("Cunning disengage works"),R->bDisengaging && !R->Turn.bBonus && R->Turn.bAction);
    // This is a separate action case; the previous evade montage must finish first.
    R->Rest(); R->StartTurn(); R->UseProgressionAbility(); TestEqual(TEXT("Cunning dash adds movement"),R->Turn.Movement,R->BaseMovement*2);
    Hunter->GainExperience(300); Hunter->Rest(); Hunter->InitializeSpellbook(); Hunter->StartTurn(); Foe->SetActorLocation(FVector(9600,0,100));
    TestTrue(TEXT("Hunter mark cast"),Hunter->CastSpell(EAHSpell::HuntersMark,Foe));
    TestTrue(TEXT("Mark is concentration on target"),Hunter->MarkedTarget.Get()==Foe && Hunter->MarkTurns>0);
    Hit=FAHDiceOutcome(); Hit.bSuccess=true; Hit.Damage=5; Hunter->AddWeaponRiders(Foe,Hit,true); TestTrue(TEXT("Mark adds one die"),Hit.Damage>=6 && Hit.Damage<=11);
    for(auto* H:{S,R,Pala,Hunter}) { H->GainExperience(2700); H->Rest(); TestEqual(TEXT("New class level cap"),H->Level,4); TestTrue(TEXT("New class feat"),H->ChooseFeat(2)); }
    TestEqual(TEXT("Paladin capped half-caster slots"),Pala->ClassCharges,3); TestEqual(TEXT("Ranger has no second-circle slots by four"),Hunter->SpellSlots2,0);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
