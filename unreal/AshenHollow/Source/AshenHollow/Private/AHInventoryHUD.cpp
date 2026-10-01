#include "AHCombatHUD.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHGameMode.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

namespace { const FLinearColor Gold(.8f,.58f,.27f), Text(.86f,.85f,.79f), Dim(.46f,.49f,.49f), Teal(.21f,.7f,.63f); }

void AAHCombatHUD::ReleaseInventoryDrag()
{
    if(DragSource.IsNone()) return;
    auto* PC=Cast<AAHPlayerController>(GetOwningPlayerController());
    auto* Hero=PC?Cast<AAHCharacter>(PC->GetPawn()):nullptr;
    FVector2D Mouse; if(PC) PC->GetMousePosition(Mouse.X,Mouse.Y);
    const FString From=DragSource.ToString(), To=HoveredBox.ToString();
    DragSource=NAME_None;
    if(!Hero || !PC->bSheetOpen || PC->bQuestJournal || PC->bProgressionSheet || FVector2D::Distance(Mouse,DragStart)<8) return;
    if(From.StartsWith(TEXT("PackCell")) && To.StartsWith(TEXT("GearSlot")))
        Hero->DropBackpackOnSlot(FCString::Atoi(*From.Mid(8)),static_cast<EAHSlot>(FCString::Atoi(*To.Mid(8))));
    else if(From.StartsWith(TEXT("GearSlot")) && (To.StartsWith(TEXT("PackCell")) || To==TEXT("PackArea")))
        Hero->Unequip(static_cast<EAHSlot>(FCString::Atoi(*From.Mid(8))));
    else Hero->Feedback=TEXT("Solte no espaco compativel. Para guardar, solte na mochila.");
    NextPortrait=0;
}

void AAHCombatHUD::DrawItemIcon(const FAHItemData* Item,float X,float Y,float Size)
{
    if(!Item) return;
    auto Line=[&](float A,float B,float C,float D,float Width=3.f){DrawLine(OffsetX+(X+A*Size)*Scale,OffsetY+(Y+B*Size)*Scale,OffsetX+(X+C*Size)*Scale,OffsetY+(Y+D*Size)*Scale,Gold,Width*Scale);};
    if(Item->Cura>0) { Line(.4,.1,.6,.1,5); Line(.4,.1,.4,.35); Line(.6,.1,.6,.35); Line(.4,.35,.25,.55); Line(.6,.35,.75,.55); Line(.25,.55,.25,.85); Line(.75,.55,.75,.85); Line(.25,.85,.75,.85); Line(.32,.62,.68,.62,8); }
    else if(Item->Slot==EAHSlot::MaoPrincipal) { Line(.2,.85,.8,.15,5); Line(.22,.55,.47,.8); Line(.68,.13,.83,.13); }
    else if(Item->Slot==EAHSlot::Armadura) { Line(.3,.18,.13,.34); Line(.13,.34,.3,.5); Line(.3,.5,.25,.85); Line(.25,.85,.75,.85); Line(.75,.85,.7,.5); Line(.7,.5,.87,.34); Line(.87,.34,.7,.18); Line(.3,.18,.5,.3); Line(.5,.3,.7,.18); }
    else if(Item->Slot==EAHSlot::MaoSecundaria) { Line(.2,.2,.8,.2); Line(.2,.2,.25,.65); Line(.8,.2,.75,.65); Line(.25,.65,.5,.9); Line(.75,.65,.5,.9); Line(.5,.3,.5,.7); }
    else { Line(.25,.25,.75,.25); Line(.75,.25,.85,.65); Line(.85,.65,.5,.85); Line(.5,.85,.15,.65); Line(.15,.65,.25,.25); }
}

void AAHCombatHUD::DrawInventorySheet(AAHCharacter* Hero,AAHPlayerController* PC,AAHGameMode* Mode)
{
    auto ButtonAt=[&](FName Name,const FString& Caption,float X,float Y,float W,bool Enabled=true)
    {
        Panel(X,Y,W,34,Enabled && HoveredBox==Name);
        Label(Caption,X+W/2,Y+9,.68f,Enabled?Gold:Dim,true,true);
        if(Enabled) AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(W*Scale,34*Scale),Name,true,25);
    };
    ButtonAt(TEXT("Ficha"),TEXT("FECHAR [P]"),1280,86,140);
    ButtonAt(TEXT("Journal"),TEXT("MISSOES [J]"),1070,86,180);
    ButtonAt(TEXT("Progression"),PC->bProgressionSheet?TEXT("INVENTARIO"):Hero->Level==4&&Hero->Feat==0?TEXT("EVOLUIR !"):TEXT("EVOLUCAO"),865,86,180);
    Label(AAHCharacter::AncestryName(Hero->Ancestry)+TEXT("  ")+AHRules::Class(Hero->HeroClass).Name,194,91,1.12f,Text,false,true);
    Label(FString::Printf(TEXT("NIVEL %d   |   %d / 2700 XP   |   %d OURO"),Hero->Level,Hero->Experience,Hero->Gold),194,129,.74f,Gold);
    if(PC->bProgressionSheet)
    {
        DragSource=NAME_None;
        Label(TEXT("SEU CAMINHO"),205,184,1.2f,Gold,false,true);
        const auto& Style=AHRules::Style(Hero->HeroClass); const auto& Sub=AHRules::Subclass(Hero->HeroClass);
        Label(FString::Printf(TEXT("Nivel 2: %s"),*Hero->ProgressionAbilityName()),205,234,.85f,Hero->Level>=2?Text:Dim);
        Label(FString::Printf(TEXT("Nivel %d: %s"),Sub.Level,Sub.Name),205,282,.85f,Hero->Level>=Sub.Level?Text:Dim);
        Label(Sub.Detail,205,317,.7f,Text);
        if(Style.Level>0) Label(FString::Printf(TEXT("Estilo: %s (nivel %d)"),Style.Name,Style.Level),205,356,.8f,Text);
        Label(FString::Printf(TEXT("Magias: %d espacos de circulo 1 / %d de circulo 2   |   Dados de vida: %d"),Hero->MaxSpellSlots(1),Hero->MaxSpellSlots(2),Hero->Level),205,399,.78f,Text);
        Label(Hero->Feat?TEXT("NIVEL 4 - ESCOLHA CONCLUIDA"):TEXT("NIVEL 4 - ESCOLHA UM TALENTO OU +2 EM UM ATRIBUTO"),205,468,.87f,Gold,false,true);
        const bool Can=Hero->Level==4 && Hero->Feat==0;
        const TCHAR* Feats[]={TEXT("ROBUSTO  +8 PV"),TEXT("ALERTA  +5 iniciativa"),TEXT("MOVEL  +3 m")};
        for(int32 I=0;I<3;++I) ButtonAt(FName(*FString::Printf(TEXT("Feat%d"),I+1)),Feats[I],205+I*395,514,375,Can);
        const auto Scores=AHSheet::Total(Hero->Abilities,Hero->Ancestry);
        for(int32 I=0;I<6;++I)
            ButtonAt(FName(*FString::Printf(TEXT("Feat%d"),I+4)),FString::Printf(TEXT("%s +2 (%d > %d)"),AHSheet::Short(static_cast<EAHAbility>(I)),Scores.Score[I],FMath::Min(20,Scores.Score[I]+2)),205+(I%3)*395,572+(I/3)*52,375,Can&&Scores.Score[I]<20);
        Label(TEXT("Escolha permanente para esta jornada. Atributos limitados a 20. K abre as magias fora da ficha."),205,716,.75f,Dim);
        Label(Hero->Feedback.Left(110),800,786,.74f,Text,true);
        return;
    }

    const auto Scores=AHSheet::Total(Hero->Abilities,Hero->Ancestry);
    for(int32 I=0;I<6;++I)
    {
        const float X=194+I*104;
        Panel(X,171,94,64); Label(AHSheet::Short(static_cast<EAHAbility>(I)),X+47,180,.65f,Dim,true);
        Label(FString::Printf(TEXT("%d (%+d)"),Scores.Score[I],Scores.Mod(static_cast<EAHAbility>(I))),X+47,205,.88f,Text,true,true);
    }
    Label(FString::Printf(TEXT("PV %d/%d   CA %d   ATAQUE %+d   DANO %dd%d%+d"),Hero->Health,Hero->MaxHealth,Hero->ArmorClass,Hero->AttackBonus,Hero->WeaponDice(),Hero->DamageSides,Hero->DamageModifier),194,251,.8f,Text);
    Label(FString::Printf(TEXT("INICIATIVA %+d   MOVIMENTO %.0f m   MAGIA %+d / CD %d"),Hero->InitiativeBonus,Hero->BaseMovement/100,Hero->SpellAttack,Hero->SpellDC),194,281,.72f,Dim);

    // A capture of the actual character and attached equipment, updated only while inspecting.
    if(!Portrait)
    {
        Portrait=NewObject<UTextureRenderTarget2D>(this); Portrait->InitAutoFormat(384,512); Portrait->ClearColor=FLinearColor(.015,.025,.035,1);
        PortraitCamera=NewObject<USceneCaptureComponent2D>(this); AddInstanceComponent(PortraitCamera);
        PortraitCamera->bCaptureEveryFrame=false; PortraitCamera->bCaptureOnMovement=false;
        PortraitCamera->TextureTarget=Portrait; PortraitCamera->CaptureSource=SCS_FinalColorLDR;
        PortraitCamera->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        PortraitCamera->FOVAngle=38; PortraitCamera->PostProcessSettings.bOverride_AutoExposureBias=true;
        PortraitCamera->PostProcessSettings.AutoExposureBias=0; PortraitCamera->RegisterComponent();
    }
    if(FPlatformTime::Seconds()>=NextPortrait)
    {
        NextPortrait=FPlatformTime::Seconds()+.5;
        const FVector Aim=Hero->GetActorLocation()+FVector(0,0,0);
        const FVector View=Aim+Hero->GetActorForwardVector()*340+Hero->GetActorRightVector()*80+FVector(0,0,25);
        PortraitCamera->SetWorldLocationAndRotation(View,(Aim-View).Rotation());
        PortraitCamera->ClearShowOnlyComponents(); PortraitCamera->ShowOnlyActorComponents(Hero,true); PortraitCamera->CaptureScene();
    }
    Panel(340,324,320,404);
    DrawTexture(Portrait,OffsetX+344*Scale,OffsetY+328*Scale,312*Scale,396*Scale,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    const TCHAR* Slots[]={TEXT(""),TEXT("ARMA"),TEXT("OUTRA MAO"),TEXT("CORPO"),TEXT("CABECA"),TEXT("PES"),TEXT("ANEL")};
    for(int32 I=1;I<7;++I)
    {
        const float X=I<=3?194:686, Y=330+((I-1)%3)*126;
        const FName Name(*FString::Printf(TEXT("GearSlot%d"),I));
        const auto* Item=AHItems::Find(Hero->Equipped[I]);
        Panel(X,Y,126,111,HoveredBox==Name); DrawItemIcon(Item,X+35,Y+25,55);
        Label(Slots[I],X+63,Y+7,.6f,Dim,true);
        Label(Item?FString(Item->Nome).Left(18):TEXT("vazio"),X+63,Y+91,.53f,Item?Text:Dim,true);
        AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(126*Scale,111*Scale),Name,true,25);
    }
    Label(TEXT("Arraste entre a mochila e os espacos de equipamento."),194,749,.66f,Dim);

    Label(TEXT("MOCHILA"),865,178,1.f,Gold,false,true);
    const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Hero->Backpack.Num(),15));
    PC->BackpackPage=FMath::Clamp(PC->BackpackPage,0,Pages-1);
    AddHitBox(FVector2D(OffsetX+854*Scale,OffsetY+211*Scale),FVector2D(560*Scale,351*Scale),TEXT("PackArea"),true,21);
    for(int32 Cell=0;Cell<15;++Cell)
    {
        const int32 Index=PC->BackpackPage*15+Cell;
        const float X=864+(Cell%5)*108, Y=216+(Cell/5)*114;
        const FName Name(*FString::Printf(TEXT("PackCell%d"),Index));
        const auto* Item=Hero->Backpack.IsValidIndex(Index)?AHItems::Find(Hero->Backpack[Index].Id):nullptr;
        Panel(X,Y,99,104,SelectedPack==Index || HoveredBox==Name);
        DrawItemIcon(Item,X+22,Y+9,57);
        if(Item)
        {
            Label(FString(Item->Nome).Left(16),X+49,Y+73,.50f,Text,true);
            Label(FString::Printf(TEXT("x%d"),Hero->Backpack[Index].Many),X+82,Y+8,.62f,Teal,true);
        }
        AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(99*Scale,104*Scale),Name,true,25);
    }
    ButtonAt(TEXT("PackPrev"),TEXT("<"),864,568,60,PC->BackpackPage>0);
    Label(FString::Printf(TEXT("PAGINA %d / %d"),PC->BackpackPage+1,Pages),1115,577,.65f,Dim,true);
    ButtonAt(TEXT("PackNext"),TEXT(">"),1357,568,50,PC->BackpackPage+1<Pages);
    if(Hero->Backpack.IsValidIndex(SelectedPack))
    {
        const auto* Item=AHItems::Find(Hero->Backpack[SelectedPack].Id);
        if(Item)
        {
            Label(Item->Nome,865,625,.85f,Gold,false,true);
            Label(FString(Item->Linha).Left(72),865,657,.57f,Text);
            ButtonAt(FName(*FString::Printf(TEXT("PackItem%d"),SelectedPack)),Item->Cura>0?TEXT("BEBER POCAO"):TEXT("EQUIPAR"),865,691,250,Item->Cura>0||Item->Slot!=EAHSlot::Nenhum);
        }
    }
    if(Mode)
    {
        const bool Town=Mode->IsInTown(Hero->GetActorLocation());
        ButtonAt(Town?TEXT("RestLong"):TEXT("RestShort"),Town?TEXT("DESCANSO LONGO"):TEXT("DESCANSO CURTO"),1130,737,278,Mode->RestRefusal(Town).IsEmpty());
        Label(Mode->ClockLabel()+FString::Printf(TEXT("  |  DADOS DE VIDA %d/%d"),Hero->HitDice,Hero->Level),865,780,.61f,Dim);
    }
    Label(Hero->Feedback.Left(90),194,794,.66f,Text);
    if(!DragSource.IsNone())
    {
        if(!PC->IsInputKeyDown(EKeys::LeftMouseButton)) ReleaseInventoryDrag();
        else
        {
            const FString From=DragSource.ToString(); const int32 Id=FCString::Atoi(*From.Mid(8));
            const auto* Item=From.StartsWith(TEXT("PackCell"))?(Hero->Backpack.IsValidIndex(Id)?AHItems::Find(Hero->Backpack[Id].Id):nullptr):Id>0&&Id<7?AHItems::Find(Hero->Equipped[Id]):nullptr;
            float MX=0,MY=0; PC->GetMousePosition(MX,MY);
            DrawItemIcon(Item,(MX-OffsetX)/Scale-25,(MY-OffsetY)/Scale-25,50);
        }
    }
}
