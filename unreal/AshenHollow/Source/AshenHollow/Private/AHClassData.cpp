#include "AHClassData.h"

namespace
{
    // Order must match EAHHeroClass.
    const FAHClassSheet GClasses[] =
    {
        {   // Fighter
            TEXT("GUERREIRO"), TEXT("ESPADACHIM"), TEXT("FÔLEGO"),
            TEXT("Segundo Fôlego / bônus / cura 1d10+1 / 1 uso por encontro"),
            TEXT("SURTO"),
            TEXT("SEGUNDO FÔLEGO"), TEXT("Bônus: cura 1d10+1"), TEXT("Armadura pesada + marcial"),
            12, 16, 5, 8, 3, 1, 1, 8, false,
            EAHWeaponKind::Sword, TEXT("SecondWind"), FLinearColor(.92f,.46f,.13f,1.f),
            0, 0, 0, nullptr
        },
        {   // Barbarian
            TEXT("BÁRBARO"), TEXT("SAQUEADOR"), TEXT("FÚRIA"),
            TEXT("Fúria / bônus / +2 dano corpo a corpo, resistência física / 2 usos por encontro"),
            TEXT("TEMERARIO"),
            TEXT("FÚRIA"), TEXT("Bônus: +2 dano físico"), TEXT("Resistência física em fúria"),
            14, 14, 5, 12, 3, 2, 2, 9, false,
            EAHWeaponKind::Axe, TEXT("Rage"), FLinearColor(.75f,.12f,.10f,1.f),
            0, 0, 0, nullptr
        },
        {   // Cleric
            TEXT("CLÉRIGO"), TEXT("ORÁCULO"), TEXT("CURAR"),
            TEXT("Curar Ferimentos / ação / cura própria 1d8+3 / 2 espaços por encontro"),
            TEXT("ESCUDO"),
            TEXT("CURAR FERIMENTOS"), TEXT("Ação: cura 1d8+3"), TEXT("Armadura, escudo e magia"),
            10, 18, 4, 6, 2, 0, 2, 7, true,
            EAHWeaponKind::Mace, TEXT("Heal"), FLinearColor(.15f,.62f,.55f,1.f),
            0, 0, 0, nullptr
        },
        {   // Wizard
            TEXT("MAGO"), TEXT("FEITICEIRO"), TEXT("MÍSSEIS"),
            TEXT("Mísseis Mágicos / ação / 3 dardos de 1d4+1 / alvo mais próximo até 18 m / 2 espaços"),
            TEXT("VIDA FALSA"),
            TEXT("MÍSSEIS MÁGICOS"), TEXT("Ação: 3 dardos de força"), TEXT("Magia sem teste de ataque"),
            8, 12, 2, 6, 0, 2, 2, 6, true,
            EAHWeaponKind::Staff, TEXT("Missiles"), FLinearColor(.52f,.22f,.72f,1.f),
            1800, 10, 0, TEXT("Raio de Fogo")
        },
        {
            TEXT("FEITICEIRO"),TEXT("PIROMANTE"),TEXT("CONJURAR"),TEXT("Magias conhecidas e pontos de feiticaria"),TEXT("CONVERTER"),
            TEXT("MAGIA INATA"),TEXT("Nivel 2: pontos de feiticaria"),TEXT("Nivel 3: magia potencializada"),
            8,12,2,6,0,2,2,6,true, EAHWeaponKind::Staff,TEXT("Missiles"),FLinearColor(.9f,.3f,.45f,1),1800,10,0,TEXT("Raio de Fogo")
        },
        {
            TEXT("LADINO"),TEXT("ASSASSINO"),TEXT("ESCAPAR"),TEXT("Nivel 2: desengajar com acao bonus"),TEXT("CORRER"),
            TEXT("ATAQUE FURTIVO"),TEXT("+1d6; +2d6 no nivel 3"),TEXT("Vantagem ou aliado junto ao alvo"),
            10,14,5,8,3,3,0,7,false, EAHWeaponKind::Sword,TEXT("Attack"),FLinearColor(.45f,.65f,.6f,1),2400,6,3,TEXT("Arco curto")
        },
        {
            TEXT("PALADINO"),TEXT("CRUZADO"),TEXT("CONJURAR"),TEXT("Cura pelas maos; magias a partir do nivel 2"),TEXT("PUNIR"),
            TEXT("IMPOSICAO DAS MAOS"),TEXT("Reserva de cura: 5 x nivel"),TEXT("Nivel 2: punicao divina opcional"),
            12,18,5,8,3,0,0,8,true, EAHWeaponKind::Sword,TEXT("Heal"),FLinearColor(.95f,.8f,.3f,1),0,0,0,nullptr
        },
        {
            TEXT("PATRULHEIRO"),TEXT("CACADOR"),TEXT("CONJURAR"),TEXT("Arco e marca do cacador a partir do nivel 2"),TEXT("MARCAR"),
            TEXT("ARQUEIRO"),TEXT("Arco longo: 1d8+3"),TEXT("Nivel 2: marca +1d6 por acerto"),
            12,15,5,6,3,3,0,8,true, EAHWeaponKind::Sword,TEXT("Attack"),FLinearColor(.25f,.7f,.3f,1),3600,8,3,TEXT("Arco longo")
        },
    };

    // Order must match EAHAncestry.
    const FAHAncestrySheet GAncestries[] =
    {
        //  name          full trait line                                     card line                        move   scale  ini arm hp dmg  lucky  fire   relent breath
        { TEXT("HUMANO"),    TEXT("Versatilidade: +1 iniciativa | 9 m"),            TEXT("+1 iniciativa"),            900.f, 1.00f, 1, 0, 0, 0,  false, false, false, false },
        { TEXT("ELFO"),      TEXT("Agilidade: +1 CA | 9 m"),                        TEXT("+1 classe de armadura"),    900.f, 1.00f, 0, 1, 0, 0,  false, false, false, false },
        { TEXT("ANÃO"),      TEXT("Tenacidade: +1 PV | 7,5 m"),                     TEXT("+1 ponto de vida"),         750.f, 0.82f, 0, 0, 1, 0,  false, false, false, false },
        { TEXT("HALFLING"),  TEXT("Sorte: rerrola 1 natural uma vez | 7,5 m"),      TEXT("Sorte: rerrola 1 uma vez"), 750.f, 0.65f, 0, 0, 0, 0,  true,  false, false, false },
        { TEXT("MEIO-ORC"),  TEXT("Perseverança: recusa uma queda por descanso | 9 m"), TEXT("Recusa cair uma vez"),  900.f, 1.12f, 0, 0, 0, 1,  false, false, true,  false },
        { TEXT("TIEFLING"),  TEXT("Legado infernal: resistência a fogo | 9 m"),     TEXT("Resistência a fogo"),       900.f, 1.00f, 0, 1, 0, 0,  false, true,  false, false },
        { TEXT("DRACONATO"), TEXT("Escamas e sopro: resiste a fogo, sopro 2d6 | 9 m"), TEXT("Sopro dracônico 2d6"),   900.f, 1.10f, 0, 0, 0, 1,  false, true,  false, true  },
    };

    static_assert(UE_ARRAY_COUNT(GClasses)    == static_cast<int32>(EAHHeroClass::Count),
        "Every EAHHeroClass needs a row in GClasses.");
    static_assert(UE_ARRAY_COUNT(GAncestries) == static_cast<int32>(EAHAncestry::Count),
        "Every EAHAncestry needs a row in GAncestries.");
}

namespace AHRules
{
    int32 ClassCount()    { return UE_ARRAY_COUNT(GClasses); }
    int32 AncestryCount() { return UE_ARRAY_COUNT(GAncestries); }

    const FAHClassSheet& Class(EAHHeroClass Which)
    {
        return GClasses[FMath::Clamp(static_cast<int32>(Which), 0, ClassCount() - 1)];
    }
    const FAHAncestrySheet& Ancestry(EAHAncestry Which)
    {
        return GAncestries[FMath::Clamp(static_cast<int32>(Which), 0, AncestryCount() - 1)];
    }
}
