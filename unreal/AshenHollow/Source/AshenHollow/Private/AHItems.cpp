#include "AHItems.h"

/**
 * The catalogue.
 *
 * Deliberately short. Thirty-two items a player can tell apart beats a hundred
 * he cannot, and every row here changes a number he can see on the sheet --
 * there is no "dagger, but slightly worse". The damage dice, armour bases and
 * Dexterity caps are the SRD's; the prices are there for the day there is
 * somebody to buy from, and are ignored by everything today.
 */
namespace
{
    using K = EAHItemKind;
    using S = EAHSlot;
    using W = EAHWeaponKind;

    const FAHItemData GCatalogue[] =
    {
    // Id                  Nome                  Linha
    //   kind        slot              arte      dano alc  sutil duas  base teto bonus forca  mov dano+ cura preco
    { TEXT("adaga"), TEXT("Adaga"), TEXT("Leve e rapida. Usa Destreza se voce preferir."),
        K::Arma, S::MaoPrincipal, W::Sword,  4,    0,  true,  false,  0,  0,  0,  0,   0,  0,  0,   2 },
    { TEXT("espada_curta"), TEXT("Espada curta"), TEXT("A arma do ladino: leve, sutil, sempre pronta."),
        K::Arma, S::MaoPrincipal, W::Sword,  6,    0,  true,  false,  0,  0,  0,  0,   0,  0,  0,  10 },
    { TEXT("espada_longa"), TEXT("Espada longa"), TEXT("Uma mao, um bom dado, nada de especial. E isso e uma virtude."),
        K::Arma, S::MaoPrincipal, W::Sword,  8,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,  15 },
    { TEXT("montante"), TEXT("Montante"), TEXT("Duas maos, 2d6 de dano. Nao sobra mao para escudo."),
        K::Arma, S::MaoPrincipal, W::Sword,  6,    0,  false, true,   0,  0,  0,  0,   0,  0,  0,  50, 2 },
    { TEXT("machado"), TEXT("Machado de batalha"), TEXT("Pesado no golpe, honesto no preco."),
        K::Arma, S::MaoPrincipal, W::Axe,    8,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,  10 },
    { TEXT("machado_grande"), TEXT("Machado grande"), TEXT("Duas maos, 1d12. O maior dado do jogo."),
        K::Arma, S::MaoPrincipal, W::Axe,   12,    0,  false, true,   0,  0,  0,  0,   0,  0,  0,  30 },
    { TEXT("maca"), TEXT("Maca"), TEXT("Nao corta: quebra. Boa contra armadura."),
        K::Arma, S::MaoPrincipal, W::Mace,   6,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   5 },
    { TEXT("martelo"), TEXT("Martelo de guerra"), TEXT("Uma mao, 1d8, e o barulho que faz."),
        K::Arma, S::MaoPrincipal, W::Mace,   8,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,  15 },
    { TEXT("lanca"), TEXT("Lanca"), TEXT("Uma mao, 1d6 de dano corpo a corpo."),
        K::Arma, S::MaoPrincipal, W::Staff,  6,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("bordao"), TEXT("Bordao"), TEXT("Um pau. Tambem serve para andar e para apontar."),
        K::Arma, S::MaoPrincipal, W::Staff,  6,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("cajado"), TEXT("Cajado arcano"), TEXT("Duas maos, e a magia sai mais firme por ele."),
        K::Arma, S::MaoPrincipal, W::Staff,  8,    0,  false, true,   0,  0,  0,  0,   0,  1,  0,  60 },
    { TEXT("arco_curto"), TEXT("Arco curto"), TEXT("Vinte e quatro metros. Usa Destreza."),
        K::Arma, S::MaoPrincipal, W::Bow,    6, 2400,  true,  true,   0,  0,  0,  0,   0,  0,  0,  25 },
    { TEXT("arco_longo"), TEXT("Arco longo"), TEXT("Trinta e seis metros, 1d8. O alcance do patrulheiro."),
        K::Arma, S::MaoPrincipal, W::Bow,    8, 3600,  true,  true,   0,  0,  0,  0,   0,  0,  0,  50 },
    { TEXT("besta"), TEXT("Besta leve"), TEXT("Lenta de recarregar, nao perdoa o alvo."),
        K::Arma, S::MaoPrincipal, W::Bow,    8, 2400,  true,  true,   0,  0,  0,  0,   0,  0,  0,  25 },

    // ── Armour ───────────────────────────────────────────────────────────
    { TEXT("roupa"), TEXT("Roupa de viagem"), TEXT("Nao e armadura. CA 10 mais sua Destreza."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 10, -1,  0,  0,   0,  0,  0,   1 },
    { TEXT("couro"), TEXT("Armadura de couro"), TEXT("CA 11 + Destreza inteira. Leve e silenciosa."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 11, -1,  0,  0,   0,  0,  0,  10 },
    { TEXT("couro_batido"), TEXT("Couro batido"), TEXT("CA 12 + Destreza inteira. O melhor que um ladino usa."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 12, -1,  0,  0,   0,  0,  0,  45 },
    { TEXT("gibao"), TEXT("Gibao de peles"), TEXT("CA 12 + Destreza ate +2. Barata e quente."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 12,  2,  0,  0,   0,  0,  0,  10 },
    { TEXT("escamas"), TEXT("Cota de escamas"), TEXT("CA 14 + Destreza ate +2. Faz barulho."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 14,  2,  0,  0,   0,  0,  0,  50 },
    { TEXT("peitoral"), TEXT("Peitoral"), TEXT("CA 14 + Destreza ate +2, e nao atrapalha."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 14,  2,  0,  0,   0,  0,  0, 400 },
    { TEXT("cota_malha"), TEXT("Cota de malha"), TEXT("CA 16 fixa. Precisa Forca 13 para andar direito."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 16,  0,  0, 13,   0,  0,  0,  75 },
    { TEXT("placas"), TEXT("Armadura de placas"), TEXT("CA 18 fixa. Forca 15, e voce anda mais devagar sem ela."),
        K::Armadura, S::Armadura, W::Sword,  0,    0,  false, false, 18,  0,  0, 15,   0,  0,  0,1500 },

    // ── Off hand, head, feet, fingers ────────────────────────────────────
    { TEXT("escudo"), TEXT("Escudo"), TEXT("+2 de CA. Ocupa a outra mao."),
        K::Escudo, S::MaoSecundaria, W::Sword, 0,  0,  false, false,  0,  0,  2,  0,   0,  0,  0,  10 },
    { TEXT("elmo"), TEXT("Elmo de ferro"), TEXT("+1 de CA, e voce ouve menos."),
        K::Vestimenta, S::Elmo, W::Sword,    0,    0,  false, false,  0,  0,  1,  0,   0,  0,  0,  20 },
    { TEXT("capuz"), TEXT("Capuz de couro"), TEXT("Nao protege quase nada, mas esconde o rosto."),
        K::Vestimenta, S::Elmo, W::Sword,    0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   5 },
    { TEXT("botas"), TEXT("Botas de marcha"), TEXT("+1,5 m de deslocamento. Vale mais do que parece."),
        K::Vestimenta, S::Botas, W::Sword,   0,    0,  false, false,  0,  0,  0,  0, 150,  0,  0,  30 },
    { TEXT("sandalias"), TEXT("Sandalias"), TEXT("Confortaveis. E so isso."),
        K::Vestimenta, S::Botas, W::Sword,   0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("anel_protecao"), TEXT("Anel de protecao"), TEXT("+1 de CA. Antigo, e ninguem sabe de quem era."),
        K::Joia, S::Anel, W::Sword,          0,    0,  false, false,  0,  0,  1,  0,   0,  0,  0, 300 },
    { TEXT("anel_forca"), TEXT("Anel do punho firme"), TEXT("+1 de dano em todo golpe que acerta."),
        K::Joia, S::Anel, W::Sword,          0,    0,  false, false,  0,  0,  0,  0,   0,  1,  0, 300 },

    // ── In the pack ──────────────────────────────────────────────────────
    { TEXT("pocao_cura"), TEXT("Pocao de cura"), TEXT("Acao: bebe e recupera 7 pontos de vida."),
        K::Consumivel, S::Nenhum, W::Sword,  0,    0,  false, false,  0,  0,  0,  0,   0,  0,  7,  50 },
    { TEXT("racao"), TEXT("Racao de viagem"), TEXT("Pao duro e carne seca. Um dia de estrada."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("tocha"), TEXT("Tocha"), TEXT("Uma hora de luz, e um porrete que queima."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("corda"), TEXT("Corda de canhamo"), TEXT("Quinze metros. Resolve mais problema que espada."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   1 },
    { TEXT("simbolo"), TEXT("Simbolo sagrado"), TEXT("Foco para as magias do clerigo e do paladino."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,   5 },
    { TEXT("grimorio"), TEXT("Grimorio"), TEXT("Suas magias escritas. Sem ele voce nao prepara nada."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,  50 },
    { TEXT("gazuas"), TEXT("Gazuas"), TEXT("Ferramentas de ladrao. Nao pergunte onde arrumou."),
        K::Miudeza, S::Nenhum, W::Sword,     0,    0,  false, false,  0,  0,  0,  0,   0,  0,  0,  25 },
    };

    constexpr int32 GCount = static_cast<int32>(sizeof(GCatalogue) / sizeof(GCatalogue[0]));

    const FAHItemData GNothing =
    { TEXT(""), TEXT("(vazio)"), TEXT(""), EAHItemKind::Miudeza, EAHSlot::Nenhum,
      EAHWeaponKind::Sword, 0, 0, false, false, 0, 0, 0, 0, 0, 0, 0, 0 };

    /**
     * Two kits per class.
     *
     * Each pair is a real fork rather than a better and a worse: the fighter
     * chooses between standing still behind sixteen points of armour and
     * hitting for 2d6 with nothing between him and the axe. What decides it is
     * how he spent his twenty-seven points, which is exactly the connection
     * this whole feature exists to make.
     */
    const AHItems::FAHKit GKits[static_cast<int32>(EAHHeroClass::Count)][AHItems::KitsPerClass] =
    {
        {   // Guerreiro
            { TEXT("SOLDADO DE LINHA"), TEXT("Cota de malha e escudo: CA 18, e voce aguenta ficar na frente."),
              { TEXT("espada_longa"), TEXT("escudo"), TEXT("cota_malha"), TEXT("botas"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("MERCENARIO"), TEXT("Montante a duas maos: 2d6 por golpe e nada no outro braco."),
              { TEXT("montante"), TEXT("couro_batido"), TEXT("botas"),
                TEXT("racao"), TEXT("pocao_cura"), TEXT("corda"), nullptr } },
        },
        {   // Barbaro
            { TEXT("DESTRUIDOR"), TEXT("Machado grande e peles: o maior dado do jogo, e pouca armadura."),
              { TEXT("machado_grande"), TEXT("gibao"), TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("BATEDOR DAS ALTURAS"), TEXT("Machado e escudo, e botas para chegar primeiro."),
              { TEXT("machado"), TEXT("escudo"), TEXT("gibao"), TEXT("botas"),
                TEXT("racao"), TEXT("corda"), nullptr } },
        },
        {   // Clerigo
            { TEXT("SACERDOTE DE ARMADURA"), TEXT("Escamas, escudo e maca. CA alta, magia intacta."),
              { TEXT("maca"), TEXT("escudo"), TEXT("escamas"), TEXT("simbolo"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("ANDARILHO DA FE"), TEXT("Couro batido, escudo e bordao: anda rapido e ainda apara."),
              { TEXT("bordao"), TEXT("escudo"), TEXT("couro_batido"), TEXT("botas"),
                TEXT("simbolo"), TEXT("racao"), TEXT("pocao_cura"), nullptr } },
        },
        {   // Mago
            { TEXT("ESTUDANTE"), TEXT("Cajado arcano, grimorio e roupa. Fragil e perigoso."),
              { TEXT("cajado"), TEXT("roupa"), TEXT("grimorio"), TEXT("pocao_cura"),
                TEXT("racao"), TEXT("tocha"), nullptr } },
            { TEXT("MAGO DE ESTRADA"), TEXT("Couro e besta: sabe que magia acaba e a estrada nao."),
              { TEXT("besta"), TEXT("couro"), TEXT("adaga"), TEXT("grimorio"),
                TEXT("racao"), TEXT("corda"), nullptr } },
        },
        {   // Feiticeiro
            { TEXT("SANGUE ANTIGO"), TEXT("Cajado e roupa. Tudo vem de dentro."),
              { TEXT("cajado"), TEXT("roupa"), TEXT("anel_protecao"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("CHAMA CONTIDA"), TEXT("Couro e duas adagas: para quando a magia acabar."),
              { TEXT("adaga"), TEXT("couro"), TEXT("capuz"), TEXT("pocao_cura"),
                TEXT("racao"), TEXT("tocha"), nullptr } },
        },
        {   // Ladino
            { TEXT("CORTADOR DE BOLSAS"), TEXT("Espada curta, couro batido e gazuas. Destreza em tudo."),
              { TEXT("espada_curta"), TEXT("couro_batido"), TEXT("gazuas"), TEXT("capuz"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("OLHO NA TRILHA"), TEXT("Arco curto e botas: bate de longe e nao deixa alcancar."),
              { TEXT("arco_curto"), TEXT("couro"), TEXT("botas"), TEXT("adaga"),
                TEXT("racao"), TEXT("corda"), nullptr } },
        },
        {   // Paladino
            { TEXT("JURAMENTO DE FERRO"), TEXT("Placas nao, ainda: malha, escudo e espada longa."),
              { TEXT("espada_longa"), TEXT("escudo"), TEXT("cota_malha"), TEXT("simbolo"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
            { TEXT("LAMINA DO VOTO"), TEXT("Montante e peitoral: menos CA, muito mais dano."),
              { TEXT("montante"), TEXT("peitoral"), TEXT("elmo"), TEXT("simbolo"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
        },
        {   // Patrulheiro
            { TEXT("ARQUEIRO"), TEXT("Arco longo e couro batido. Trinta e seis metros de vantagem."),
              { TEXT("arco_longo"), TEXT("couro_batido"), TEXT("botas"), TEXT("adaga"),
                TEXT("racao"), TEXT("corda"), nullptr } },
            { TEXT("DUAS LAMINAS"), TEXT("Espada curta e adaga, de perto, e couro para correr."),
              { TEXT("espada_curta"), TEXT("adaga"), TEXT("couro"), TEXT("capuz"),
                TEXT("racao"), TEXT("pocao_cura"), nullptr } },
        },
    };
    static_assert(sizeof(GKits) / sizeof(GKits[0]) == static_cast<int32>(EAHHeroClass::Count),
                  "dois kits para cada classe");
}

int32 AHItems::Count() { return GCount; }

const FAHItemData& AHItems::At(int32 Index)
{
    return (Index >= 0 && Index < GCount) ? GCatalogue[Index] : GNothing;
}

const FAHItemData& AHItems::Nothing() { return GNothing; }

const FAHItemData* AHItems::Find(const FString& Id)
{
    if (Id.IsEmpty()) return nullptr;
    for (int32 I = 0; I < GCount; ++I)
        if (Id == FString(GCatalogue[I].Id)) return &GCatalogue[I];
    return nullptr;
}

const AHItems::FAHKit& AHItems::Kit(EAHHeroClass Class, int32 Which)
{
    const int32 Row = FMath::Clamp(static_cast<int32>(Class), 0,
                                   static_cast<int32>(EAHHeroClass::Count) - 1);
    return GKits[Row][FMath::Clamp(Which, 0, KitsPerClass - 1)];
}
