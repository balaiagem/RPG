#include "AHGameMode.h"
#include "AHCharacter.h"
#include "AHVillager.h"
#include "Kismet/GameplayStatics.h"

void AAHGameMode::InitializeSideQuests()
{
    SideQuests.Reset();
    TSet<int32> UsedFolk;
    for(int32 Kind=0;Kind<4;++Kind)
    {
        const EAHFolk Preferred=Kind==0?EAHFolk::Guarda:Kind==1?EAHFolk::Lenhador:Kind==2?EAHFolk::Pedreiro:EAHFolk::Mercador;
        int32 Person=INDEX_NONE;
        double Best=MAX_dbl;
        for(int32 I=0;I<Plan.Folk.Num();++I)
        {
            const auto& Spot=Plan.Folk[I];
            if(Spot.bGiver || UsedFolk.Contains(I)) continue;
            const double Score=FVector::Dist2D(Spot.Where,HeroSpawn)+(Spot.Trade==Preferred?0:200000);
            if(Score<Best) { Best=Score; Person=I; }
        }
        if(Person==INDEX_NONE) continue;
        FAHSideQuest Q; Q.Giver=Plan.Folk[Person].Where;
        Best=MAX_dbl;
        if(Kind<3)
        {
            for(int32 I=0;I<Camps.Num();++I)
            {
                const auto& Camp=Camps[I];
                if(Camp.bIndoor || Camp.WildlifeKind!=Kind) continue;
                const double Score=FVector::Dist2D(Q.Giver,Camp.Centre);
                if(Score<Best) { Best=Score; Q.CampIndex=I; Q.Target=Camp.Centre; }
            }
            if(Q.CampIndex==INDEX_NONE) continue;
        }
        else
        {
            Q.bSurvey=true;
            bool Found=false;
            for(const auto& Mark:Plan.Landmarks)
            {
                if(Mark.Kind!=EAHSite::Ruina && Mark.Kind!=EAHSite::Circulo) continue;
                const double Score=FVector::Dist2D(Q.Giver,Mark.Where);
                if(Score<Best) { Best=Score; Q.Target=Mark.Where; Found=true; }
            }
            if(!Found) continue;
        }
        static const TCHAR* Titles[]={TEXT("Estrada segura"),TEXT("Uivos na trilha"),TEXT("Pedra e garras"),TEXT("Memorias do vale")};
        static const TCHAR* Objectives[]={TEXT("Derrote o grupo de bandidos marcado e volte ao morador."),TEXT("Afaste a alcateia marcada e volte ao morador."),TEXT("Derrote o urso perto da pedreira e volte ao morador."),TEXT("Visite o marco antigo indicado e relate o que encontrou.")};
        Q.Title=Titles[Kind]; Q.Objective=Objectives[Kind];
        Q.RewardXP=Kind==2?250:150;
        Q.RewardItem=TEXT("pocao_cura");
        SideQuests.Add(Q); UsedFolk.Add(Person);
    }
}

void AAHGameMode::UpdateSideQuests()
{
    auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    for(auto& Q:SideQuests)
    {
        if(Q.Stage!=1) continue;
        const bool Done=Q.bSurvey ? Hero && FVector::Dist2D(Hero->GetActorLocation(),Q.Target)<700
            : Camps.IsValidIndex(Q.CampIndex) && Camps[Q.CampIndex].bCleared;
        if(!Done) continue;
        Q.Stage=2;
        if(Hero) Hero->AddLog(Q.Title+TEXT(": objetivo concluido. Volte ao morador para receber a recompensa."));
    }
}

bool AAHGameMode::TalkSideQuest(AAHVillager* Villager,AAHCharacter* Hero)
{
    if(!Villager || !Hero || !Hero->IsAlive() || !bExploring) return false;
    UpdateSideQuests();
    for(auto& Q:SideQuests)
    {
        if(FVector::Dist2D(Q.Giver,Villager->Casa)>50) continue;
        if(Q.Stage==0)
        {
            Q.Stage=1;
            Villager->Diga(Q.Objective,10);
            Hero->AddLog(TEXT("MISSAO: ")+Q.Title+TEXT(". ")+Q.Objective);
            UpdateSideQuests();
        }
        else if(Q.Stage==1) Villager->Diga(TEXT("Marquei o local no seu mapa. Boa viagem!"),6);
        else if(Q.Stage==2)
        {
            Q.Stage=3; // Commit before granting rewards: repeated conversation cannot duplicate them.
            Hero->GainExperience(Q.RewardXP);
            Hero->Carry(Q.RewardItem.ToString(),1);
            Villager->Diga(TEXT("Obrigado! Leve esta pocao para a proxima viagem."),8);
            Hero->AddLog(FString::Printf(TEXT("MISSAO CUMPRIDA: %s. +%d XP e uma pocao."),*Q.Title,Q.RewardXP));
        }
        else Villager->Diga(TEXT("O vale esta mais seguro gracas a voce."),5);
        return true;
    }
    return false;
}
