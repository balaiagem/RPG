#include "AHSpellData.h"
namespace {
const FAHSpellDefinition Spells[]={
{EAHSpell::SacredFlame,EAHHeroClass::Cleric,TEXT("CHAMA SAGRADA"),TEXT("Truque | acao | 18 m | DES CD 13; falha: 1d8 radiante"),0,false,true,1800},
{EAHSpell::CureWounds,EAHHeroClass::Cleric,TEXT("CURAR FERIMENTOS"),TEXT("I | acao | pessoal | cura 1d8+3; +1d8 por circulo"),1,false,false,0},
{EAHSpell::HealingWord,EAHHeroClass::Cleric,TEXT("PALAVRA CURATIVA"),TEXT("I | bonus | pessoal | cura 1d4+3; +1d4 por circulo"),1,true,false,0},
{EAHSpell::GuidingBolt,EAHHeroClass::Cleric,TEXT("RAIO GUIADOR"),TEXT("I | acao | 36 m | ataque +5; 4d6; proximo ataque com vantagem"),1,false,true,3600},
{EAHSpell::ShieldOfFaith,EAHHeroClass::Cleric,TEXT("ESCUDO DA FE"),TEXT("I | bonus | pessoal | +2 CA; concentracao, 100 turnos"),1,true,false,0},
{EAHSpell::Aid,EAHHeroClass::Cleric,TEXT("AUXILIO"),TEXT("II | acao | pessoal | +5 PV atuais e maximos ate descanso"),2,false,false,0},
{EAHSpell::FireBolt,EAHHeroClass::Wizard,TEXT("RAIO DE FOGO"),TEXT("Truque | acao | 36 m | ataque +5; 1d10 fogo"),0,false,true,3600},
{EAHSpell::RayOfFrost,EAHHeroClass::Wizard,TEXT("RAIO DE GELO"),TEXT("Truque | acao | 18 m | ataque +5; 1d8 frio; -3 m por 1 turno"),0,false,true,1800},
{EAHSpell::MagicMissile,EAHHeroClass::Wizard,TEXT("MISSEIS MAGICOS"),TEXT("I | acao | 36 m | 3 dardos de 1d4+1; +1 dardo por circulo"),1,false,true,3600},
{EAHSpell::FalseLife,EAHHeroClass::Wizard,TEXT("VIDA FALSA"),TEXT("I | acao | pessoal | 1d4+4 PV temporarios; +5 por circulo"),1,false,false,0},
{EAHSpell::MageArmor,EAHHeroClass::Wizard,TEXT("ARMADURA ARCANA"),TEXT("I | acao | pessoal | CA base 13+DES ate descanso"),1,false,false,0},
{EAHSpell::ScorchingRay,EAHHeroClass::Wizard,TEXT("RAIO ARDENTE"),TEXT("II | acao | 36 m | 3 ataques +5; 2d6 fogo cada"),2,false,true,3600},
{EAHSpell::InflictWounds,EAHHeroClass::Cleric,TEXT("INFLIGIR FERIMENTOS"),TEXT("I | acao | toque | ataque +5; 3d10 necrotico; +1d10 por circulo"),1,false,true,190},
{EAHSpell::Bless,EAHHeroClass::Cleric,TEXT("BENCAO"),TEXT("I | acao | pessoal | +1d4 ataques e salvaguardas; concentracao"),1,false,false,0},
{EAHSpell::BurningHands,EAHHeroClass::Wizard,TEXT("MAOS FLAMEJANTES"),TEXT("I | acao | cone 4,5 m | DES CD13; 3d6 fogo, metade ao salvar"),1,false,true,450},
{EAHSpell::Thunderwave,EAHHeroClass::Wizard,TEXT("ONDA TROVEJANTE"),TEXT("I | acao | cubo frontal 4,5 m | CON CD13; 2d8, metade ao salvar"),1,false,true,450},
{EAHSpell::HuntersMark,EAHHeroClass::Ranger,TEXT("MARCA DO CACADOR"),TEXT("I | bonus | 27 m | +1d6 em ataques de arma; concentracao"),1,true,true,2700},
{EAHSpell::Goodberry,EAHHeroClass::Ranger,TEXT("BOM FRUTO"),TEXT("I | acao | cria 10 frutos; consumir um usa acao e cura 1 PV"),1,false,false,0}
};
static_assert(UE_ARRAY_COUNT(Spells)==static_cast<int32>(EAHSpell::Count));
}
const FAHSpellDefinition& AHSpells::Get(EAHSpell Id) { return Spells[FMath::Clamp(static_cast<int32>(Id),0,Count()-1)]; }
int32 AHSpells::Count() { return UE_ARRAY_COUNT(Spells); }

bool AHSpells::ForClass(EAHSpell Id, EAHHeroClass Class)
{
    if(Class==EAHHeroClass::Sorcerer) return Get(Id).Class==EAHHeroClass::Wizard;
    if(Class==EAHHeroClass::Paladin) return Id==EAHSpell::CureWounds || Id==EAHSpell::Bless || Id==EAHSpell::ShieldOfFaith || Id==EAHSpell::Aid;
    if(Class==EAHHeroClass::Ranger) return Get(Id).Class==Class || Id==EAHSpell::CureWounds;
    return Get(Id).Class==Class;
}
