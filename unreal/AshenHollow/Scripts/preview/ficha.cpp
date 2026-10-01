// A ficha de personagem, conferida sem abrir a Unreal.
//
// Compila AHAbilities.cpp, AHItems.cpp e AHClassData.cpp de verdade -- os
// mesmos arquivos que o jogo compila -- e responde tres perguntas que so
// apareceriam depois de um build e de uma partida:
//
//   1. toda distribuicao sugerida cabe nos 27 pontos e respeita o teto de 15?
//   2. todo item citado nos kits existe no catalogo?
//   3. os numeros derivados (CA, PV, ataque, dano) ficam perto dos numeros
//      fixos que a tabela de classes usava antes -- ou seja, o jogo continua
//      equilibrado depois da troca?
//
// A terceira e a que importa. Trocar constantes escolhidas a mao por contas
// de 5e pode dobrar a CA de todo mundo sem que nada de errado aconteca, e o
// unico jeito de ver isso e por as duas colunas lado a lado.
#include "AHAbilities.h"
#include "AHItems.h"
#include <cstdio>
#include <cstring>

namespace
{
    const char* const GClassName[] =
    { "guerreiro", "barbaro", "clerigo", "mago", "feiticeiro", "ladino",
      "paladino", "patrulheiro" };
    const char* const GBloodName[] =
    { "humano", "elfo", "anao", "halfling", "meio-orc", "tiefling", "draconato" };

    int Falhas = 0;
    void Falha(const char* What) { std::printf("  FALHOU: %s\n", What); ++Falhas; }
}

int main()
{
    const int Classes = static_cast<int>(EAHHeroClass::Count);
    const int Bloods  = static_cast<int>(EAHAncestry::Count);
    const int Slots   = static_cast<int>(EAHSlot::Count);

    // ── 1. As distribuicoes sugeridas sao legais? ────────────────────────
    std::printf("== compra por pontos ==\n");
    for (int C = 0; C < Classes; ++C)
    {
        const EAHHeroClass Which = static_cast<EAHHeroClass>(C);
        FAHAbilities Base;
        const int32* Suggested = AHSheet::Recommended(Which);
        for (int A = 0; A < 6; ++A) Base.Score[A] = Suggested[A];
        const int32 Spent = AHSheet::Spent(Base);
        std::printf("  %-12s %2d %2d %2d %2d %2d %2d  = %2d/27 pontos%s\n",
                    GClassName[C], Base.Score[0], Base.Score[1], Base.Score[2],
                    Base.Score[3], Base.Score[4], Base.Score[5], Spent,
                    AHSheet::Legal(Base) ? "" : "   <-- ILEGAL");
        if (!AHSheet::Legal(Base)) Falha("distribuicao sugerida fora das regras");
        // Gastar menos que o orcamento e desperdicio: sobra ponto na tela.
        if (Spent < AHSheet::BuyBudget - 1) Falha("distribuicao sugerida deixa pontos na mesa");
    }

    // O preco tem que ser o do SRD, nao um parecido.
    const int32 Wanted[8] = { 0, 1, 2, 3, 4, 5, 7, 9 };
    for (int V = 8; V <= 15; ++V)
        if (AHSheet::BuyCost(V) != Wanted[V - 8]) Falha("tabela de precos errada");
    // E o modificador arredonda PARA BAIXO, inclusive nos negativos.
    {
        FAHAbilities Check;
        // 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 -- floor((valor - 10) / 2).
        // A primeira versao desta linha estava deslocada em um, e o teste
        // acusou o codigo sete vezes por um erro que era dele mesmo. Um teste
        // errado custa mais caro que nenhum teste.
        const int32 Expect[] = { -4, -3, -3, -2, -2, -1, -1, 0, 0, 1, 1, 2, 2, 3, 3 };
        for (int Score = 3; Score <= 17; ++Score)
        {
            Check.Set(EAHAbility::Forca, Score);
            if (Check.Mod(EAHAbility::Forca) != Expect[Score - 3])
                Falha("modificador arredondado errado");
        }
    }

    // ── 2. Todo item dos kits existe? ────────────────────────────────────
    std::printf("\n== kits ==\n");
    for (int C = 0; C < Classes; ++C)
        for (int K = 0; K < AHItems::KitsPerClass; ++K)
        {
            const AHItems::FAHKit& Kit = AHItems::Kit(static_cast<EAHHeroClass>(C), K);
            int Many = 0;
            for (int I = 0; I < 8 && Kit.Itens[I]; ++I)
            {
                ++Many;
                if (!AHItems::Find(FString(Kit.Itens[I])))
                {
                    std::printf("  %s / %s: item inexistente '%s'\n",
                                GClassName[C], Kit.Nome, Kit.Itens[I]);
                    Falha("kit cita item que nao esta no catalogo");
                }
            }
            if (Many < 3) Falha("kit com menos de tres itens");
        }
    std::printf("  %d itens no catalogo, %d kits, todos conferidos\n",
                AHItems::Count(), Classes * AHItems::KitsPerClass);

    // Nenhum item pode ocupar um espaco que nao combina com o que ele e.
    for (int I = 0; I < AHItems::Count(); ++I)
    {
        const FAHItemData& Thing = AHItems::At(I);
        if (Thing.Dano > 0 && Thing.Slot != EAHSlot::MaoPrincipal)
            Falha("arma que nao vai na mao principal");
        if (Thing.Base > 0 && Thing.Slot != EAHSlot::Armadura)
            Falha("armadura que nao vai no corpo");
        if (Thing.Kind == EAHItemKind::Consumivel && Thing.Cura <= 0)
            Falha("consumivel que nao faz nada");
    }

    // ── 3. Os numeros derivados contra os numeros antigos ────────────────
    std::printf("\n== ficha derivada (humano, nivel 1, distribuicao sugerida) ==\n");
    std::printf("  %-12s %-18s  CA      PV      ataque   dano\n", "classe", "kit");
    for (int C = 0; C < Classes; ++C)
    {
        const EAHHeroClass Which = static_cast<EAHHeroClass>(C);
        const FAHClassSheet& Old = AHRules::Class(Which);
        for (int K = 0; K < AHItems::KitsPerClass; ++K)
        {
            FAHAbilities Scores;
            const int32* Suggested = AHSheet::Recommended(Which);
            for (int A = 0; A < 6; ++A) Scores.Score[A] = Suggested[A];

            FString Equipped[static_cast<int32>(EAHSlot::Count)];
            const AHItems::FAHKit& Kit = AHItems::Kit(Which, K);
            for (int I = 0; I < 8 && Kit.Itens[I]; ++I)
            {
                const FAHItemData* Thing = AHItems::Find(FString(Kit.Itens[I]));
                if (!Thing || Thing->Slot == EAHSlot::Nenhum) continue;
                const int32 Where = static_cast<int32>(Thing->Slot);
                if (Equipped[Where].IsEmpty()) Equipped[Where] = FString(Thing->Id);
            }

            const AHSheet::FAHDerived Now =
                AHSheet::Derive(Which, EAHAncestry::Human, 1, Scores, Equipped);

            std::printf("  %-12s %-18s %2d (%2d) %2d (%2d)  +%d (+%d)  1d%-2d%+d (1d%d%+d)%s\n",
                        GClassName[C], Kit.Nome,
                        Now.ArmorClass, Old.ArmorClass,
                        Now.MaxHealth,  Old.MaxHealth,
                        Now.AttackBonus, Old.AttackBonus,
                        Now.DamageSides, Now.DamageModifier,
                        Old.DamageSides, Old.DamageModifier,
                        Now.bOverloaded ? "  [armadura pesada demais]" : "");

            // Os limites que importam: nada pode sair absurdo.
            if (Now.ArmorClass < 10 || Now.ArmorClass > 21) Falha("CA fora de 10..21");
            if (Now.MaxHealth  <  5 || Now.MaxHealth  > 20) Falha("PV de nivel 1 fora de 5..20");
            /**
             * O piso do ataque com ARMA e +1, nao +2.
             *
             * Um mago com Forca 9 batendo de cajado a +1 nao e um erro, e a
             * regra: ele nao deveria estar batendo de cajado. Quem checa a
             * competencia de um conjurador e o ataque de magia, logo abaixo.
             */
            if (Now.AttackBonus < 1 || Now.AttackBonus >  7) Falha("ataque de arma fora de +1..+7");
            if (AHRules::Class(Which).bCaster
                && (Now.SpellAttack < 3 || Now.SpellAttack > 7))
                Falha("ataque de magia fora de +3..+7");
            if (Now.bOverloaded) Falha("kit inicial que a propria classe nao consegue usar");
        }
    }

    // Um nivel 4 nao pode virar outro jogo.
    std::printf("\n== nivel 4, mesmo kit ==\n");
    for (int C = 0; C < Classes; ++C)
    {
        const EAHHeroClass Which = static_cast<EAHHeroClass>(C);
        FAHAbilities Scores;
        const int32* Suggested = AHSheet::Recommended(Which);
        for (int A = 0; A < 6; ++A) Scores.Score[A] = Suggested[A];
        FString Equipped[static_cast<int32>(EAHSlot::Count)];
        const AHItems::FAHKit& Kit = AHItems::Kit(Which, 0);
        for (int I = 0; I < 8 && Kit.Itens[I]; ++I)
        {
            const FAHItemData* Thing = AHItems::Find(FString(Kit.Itens[I]));
            if (!Thing || Thing->Slot == EAHSlot::Nenhum) continue;
            const int32 Where = static_cast<int32>(Thing->Slot);
            if (Equipped[Where].IsEmpty()) Equipped[Where] = FString(Thing->Id);
        }
        const AHSheet::FAHDerived Four =
            AHSheet::Derive(Which, EAHAncestry::Human, 4, Scores, Equipped);
        std::printf("  %-12s CA %2d, PV %2d, ataque +%d, CD de magia %d\n",
                    GClassName[C], Four.ArmorClass, Four.MaxHealth,
                    Four.AttackBonus, Four.SpellDC);
        if (Four.MaxHealth < 14 || Four.MaxHealth > 60) Falha("PV de nivel 4 fora de 14..60");
    }

    // ── 4. As ancestralidades ────────────────────────────────────────────
    std::printf("\n== ancestralidade (guerreiro, kit 0) ==\n");
    for (int B = 0; B < Bloods; ++B)
    {
        FAHAbilities Scores;
        const int32* Suggested = AHSheet::Recommended(EAHHeroClass::Fighter);
        for (int A = 0; A < 6; ++A) Scores.Score[A] = Suggested[A];
        const FAHAbilities Shown = AHSheet::Total(Scores, static_cast<EAHAncestry>(B));
        FString Equipped[static_cast<int32>(EAHSlot::Count)];
        const AHItems::FAHKit& Kit = AHItems::Kit(EAHHeroClass::Fighter, 0);
        for (int I = 0; I < 8 && Kit.Itens[I]; ++I)
        {
            const FAHItemData* Thing = AHItems::Find(FString(Kit.Itens[I]));
            if (!Thing || Thing->Slot == EAHSlot::Nenhum) continue;
            const int32 Where = static_cast<int32>(Thing->Slot);
            if (Equipped[Where].IsEmpty()) Equipped[Where] = FString(Thing->Id);
        }
        const AHSheet::FAHDerived Made =
            AHSheet::Derive(EAHHeroClass::Fighter, static_cast<EAHAncestry>(B), 1, Scores, Equipped);
        std::printf("  %-10s FOR %2d DES %2d CON %2d -> CA %2d, PV %2d, ini %+d, desl %.0f\n",
                    GBloodName[B], Shown.Raw(EAHAbility::Forca),
                    Shown.Raw(EAHAbility::Destreza), Shown.Raw(EAHAbility::Constituicao),
                    Made.ArmorClass, Made.MaxHealth, Made.InitiativeBonus, Made.Movement);
        // O bonus de ancestralidade nao pode passar do teto do 5e.
        for (int A = 0; A < 6; ++A)
            if (Shown.Score[A] > 17) Falha("atributo inicial acima de 17");
    }

    // ── 5. Um inimigo sem kit nenhum luta como antes ─────────────────────
    std::printf("\n== inimigo sem equipamento (tem que bater com a tabela antiga) ==\n");
    for (int C = 0; C < Classes; ++C)
    {
        const EAHHeroClass Which = static_cast<EAHHeroClass>(C);
        const FAHClassSheet& Old = AHRules::Class(Which);
        FAHAbilities Flat;                    // dez em tudo: modificador zero
        const AHSheet::FAHDerived Foe =
            AHSheet::Derive(Which, EAHAncestry::Human, 1, Flat, nullptr);
        const bool bSame = Foe.ArmorClass == Old.ArmorClass + AHRules::Ancestry(EAHAncestry::Human).ArmorBonus
                        && Foe.AttackBonus == Old.AttackBonus
                        && Foe.DamageSides == Old.DamageSides;
        std::printf("  %-12s CA %2d/%2d  ataque +%d/+%d  d%d/d%d  %s\n",
                    GClassName[C], Foe.ArmorClass, Old.ArmorClass,
                    Foe.AttackBonus, Old.AttackBonus,
                    Foe.DamageSides, Old.DamageSides, bSame ? "igual" : "MUDOU");
        if (!bSame) Falha("inimigo sem kit mudou de numero");
    }

    std::printf("\n%s (%d falha(s))\n", Falhas ? "TEM COISA ERRADA" : "tudo certo", Falhas);
    return Falhas ? 1 : 0;
}
