#include "AHCombatHUD.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "RenderUtils.h"

namespace AHUI
{
    const FLinearColor Gold(.62f,.39f,.15f,1), Bright(.95f,.74f,.36f,1), Text(.84f,.81f,.71f,1);
    const FLinearColor Dim(.33f,.35f,.36f,1), Ink(.009f,.013f,.019f,.97f), Green(.20f,.65f,.39f,1), Amber(.92f,.45f,.13f,1);
}
void AAHCombatHUD::Label(const FString& Text,float X,float Y,float Size,FLinearColor Color,bool Center)
{
    Size*=1.25f;
    float TW=0,TH=0;
    if(Center) GetTextSize(Text,TW,TH,GEngine->GetSmallFont(),Size*Scale);
    DrawText(Text,FLinearColor(0,0,0,Color.A*.9f),OffsetX+X*Scale-TW*.5f+1.5f*Scale,OffsetY+Y*Scale+1.5f*Scale,GEngine->GetSmallFont(),Size*Scale);
    DrawText(Text,Color,OffsetX+X*Scale-TW*.5f,OffsetY+Y*Scale,GEngine->GetSmallFont(),Size*Scale);
}
void AAHCombatHUD::Panel(float X,float Y,float W,float H,bool Active)
{
    const float PX=OffsetX+X*Scale,PY=OffsetY+Y*Scale;
    DrawRect(FLinearColor(0,0,0,.35f),PX+3*Scale,PY+5*Scale,W*Scale,H*Scale);
    DrawRect(AHUI::Ink,PX,PY,W*Scale,H*Scale);
    const FLinearColor C=Active?AHUI::Bright:AHUI::Gold;
    DrawLine(PX,PY,PX+W*Scale,PY,C,Scale);
    DrawLine(PX,PY+H*Scale,PX+W*Scale,PY+H*Scale,C,Scale);
    DrawLine(PX,PY,PX,PY+H*Scale,C*.6f,Scale);
    DrawLine(PX+W*Scale,PY,PX+W*Scale,PY+H*Scale,C*.6f,Scale);
    for(float CX:{PX,PX+W*Scale}) for(float CY:{PY,PY+H*Scale})
    { DrawLine(CX-3*Scale,CY,CX,CY-3*Scale,C,Scale); DrawLine(CX,CY-3*Scale,CX+3*Scale,CY,C,Scale); }
}
void AAHCombatHUD::Icon(FName Type,float X,float Y,FLinearColor Color)
{
    auto L=[&](float A,float B,float C,float D){DrawLine(OffsetX+(X+A)*Scale,OffsetY+(Y+B)*Scale,OffsetX+(X+C)*Scale,OffsetY+(Y+D)*Scale,Color,2*Scale);};
    if(Type==TEXT("Attack")) { L(-15,16,15,-16); L(15,-16,13,-5); L(15,-16,4,-14); L(-11,4,-2,13); L(-17,18,-13,21); }
    else if(Type==TEXT("Dodge")) { L(-15,-15,0,-20); L(0,-20,15,-15); L(15,-15,12,7); L(12,7,0,20); L(0,20,-12,7); L(-12,7,-15,-15); L(0,-14,0,12); }
    else if(Type==TEXT("Heal")) { L(-16,0,16,0); L(0,-16,0,16); L(-10,-10,-5,-15); L(5,15,10,10); }
    else if(Type==TEXT("Dash")) { L(-17,-14,-3,0); L(-3,0,-17,14); L(0,-14,14,0); L(14,0,0,14); }
    else { L(0,-20,17,0); L(17,0,0,20); L(0,20,-17,0); L(-17,0,0,-20); L(-17,0,17,0); L(0,-20,-6,0); L(-6,0,0,20); }
}
void AAHCombatHUD::Button(FName Name,const FString& Key,const FString& Title,float X,float Y,bool Enabled)
{
    const bool Hover=HoveredBox==Name;
    Panel(X,Y,82,78,Hover && Enabled);
    if(Hover) DrawRect(FLinearColor(.20f,.13f,.05f,.22f),OffsetX+(X+2)*Scale,OffsetY+(Y+2)*Scale,78*Scale,74*Scale);
    Icon(Name,X+41,Y+32,Enabled?AHUI::Bright:AHUI::Dim);
    Label(Key,X+6,Y+5,.75f,AHUI::Text);
    Label(Title,X+41,Y+58,.78f,Enabled?AHUI::Text:AHUI::Dim,true);
    AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(82*Scale,78*Scale),Name,true,1);
}
void AAHCombatHUD::NotifyHitBoxClick(FName Name)
{
    if(auto* PC=Cast<AAHPlayerController>(GetOwningPlayerController())) PC->CombatCommand(Name);
}
void AAHCombatHUD::NotifyHitBoxBeginCursorOver(FName Name) { HoveredBox=Name; }
void AAHCombatHUD::NotifyHitBoxEndCursorOver(FName Name) { if(HoveredBox==Name) HoveredBox=NAME_None; }
bool AAHCombatHUD::IsPointerOverInterface() const
{
    float X,Y; if(!GetOwningPlayerController()->GetMousePosition(X,Y)) return false;
    X=(X-OffsetX)/Scale; Y=(Y-OffsetY)/Scale;
    return Y>740 || (Y<120 && X>525 && X<1075) || (X>1250 && Y>185 && Y<480);
}
void AAHCombatHUD::DrawDie(float X,float Y,float Radius,float Angle,FLinearColor Color)
{
    const float P=(1.f+FMath::Sqrt(5.f))*.5f;
    const FVector Vertices[]={{-1,P,0},{1,P,0},{-1,-P,0},{1,-P,0},{0,-1,P},{0,1,P},{0,-1,-P},{0,1,-P},{P,0,-1},{P,0,1},{-P,0,-1},{-P,0,1}};
    FVector V[12]; for(int I=0;I<12;++I) V[I]=FRotator(18,Angle,12).RotateVector(Vertices[I])/1.91f;
    struct FFace {int A,B,C; float Z;}; TArray<FFace> Faces;
    for(int A=0;A<12;++A) for(int B=A+1;B<12;++B) for(int C=B+1;C<12;++C)
    {
        if(FMath::IsNearlyEqual(FVector::DistSquared(Vertices[A],Vertices[B]),4.f,.01f) && FMath::IsNearlyEqual(FVector::DistSquared(Vertices[A],Vertices[C]),4.f,.01f) && FMath::IsNearlyEqual(FVector::DistSquared(Vertices[B],Vertices[C]),4.f,.01f)) Faces.Add({A,B,C,(float)(V[A].Z+V[B].Z+V[C].Z)});
    }
    Faces.Sort([](const FFace& A,const FFace& B){return A.Z<B.Z;});
    auto Point=[&](int I){return FVector2D(OffsetX+(X+V[I].X*Radius)*Scale,OffsetY+(Y+V[I].Y*Radius)*Scale);};
    for(const auto& F:Faces)
    {
        FCanvasTriangleItem Face(Point(F.A),Point(F.B),Point(F.C),GWhiteTexture);
        Face.SetColor(FLinearColor(.035f,.10f,.095f,1)*FMath::Clamp(.7f+F.Z*.2f,.25f,1.f));
        Canvas->DrawItem(Face);
        for(auto E:{TPair<int,int>(F.A,F.B),TPair<int,int>(F.B,F.C),TPair<int,int>(F.C,F.A)})
        {const auto A=Point(E.Key),B=Point(E.Value); DrawLine(A.X,A.Y,B.X,B.Y,Color*.75f,1.2f*Scale);}
    }
}
void AAHCombatHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Hero=Cast<AAHCharacter>(GetOwningPawn()); auto* PC=Cast<AAHPlayerController>(GetOwningPlayerController());
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if(!Canvas || !Hero || !PC) return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);
    OffsetX=(Canvas->SizeX-1600*Scale)*.5f; OffsetY=(Canvas->SizeY-900*Scale)*.5f;
    const float Now=GetWorld()->GetTimeSeconds(); const bool Ready=Hero->CanAct();
    Label(PC->PerformanceLabel,1410,28,.82f,AHUI::Text,true);
    SmoothedFrameMs=FMath::Lerp(SmoothedFrameMs,GetWorld()->GetDeltaSeconds()*1000.f,.05f);
    Label(FString::Printf(TEXT("%.0f FPS / %.1f ms"),1000.f/FMath::Max(SmoothedFrameMs,.1f),SmoothedFrameMs),1410,50,.8f,AHUI::Gold,true);
    if(!Hero->bCharacterReady)
    {
        Panel(200,155,1200,580,true);
        Label(TEXT("ESCOLHA SUA CLASSE"),800,187,2.f,AHUI::Bright,true);
        Label(TEXT("Nivel 1 / quatro arquetipos jogaveis / atributos predefinidos"),800,238,1.f,AHUI::Text,true);
        const TCHAR* Stats[]={TEXT("12 PV / CA 16 / +5 ataque"),TEXT("14 PV / CA 14 / +5 ataque"),TEXT("10 PV / CA 18 / +4 ataque"),TEXT("8 PV / CA 12 / +2 ataque")};
        const TCHAR* Skills[]={TEXT("SEGUNDO FOLEGO"),TEXT("FURIA"),TEXT("CURAR FERIMENTOS"),TEXT("MISSEIS MAGICOS")};
        const TCHAR* Details[]={TEXT("Bonus: cura 1d10+1"),TEXT("Bonus: +2 dano fisico"),TEXT("Acao: cura 1d8+3"),TEXT("Acao: 3 dardos de forca")};
        const TCHAR* Passive[]={TEXT("Armadura e arma marcial"),TEXT("Resistencia fisica em furia"),TEXT("Armadura, escudo e magia"),TEXT("Magia sem teste de ataque")};
        for(int32 I=0;I<4;++I)
        {
            const float X=230+I*290;
            const FName Name(*FString::Printf(TEXT("Class%d"),I));
            Panel(X,290,270,330,HoveredBox==Name);
            Icon(I==3?TEXT("Arcana"):I==2?TEXT("Heal"):I==1?TEXT("Attack"):TEXT("Dodge"),X+135,347,AHUI::Bright);
            Label(AAHCharacter::ClassName(static_cast<EAHHeroClass>(I)),X+135,397,1.2f,AHUI::Text,true);
            Label(Stats[I],X+135,433,.9f,AHUI::Text,true);
            Label(Skills[I],X+135,477,.9f,AHUI::Bright,true);
            Label(Details[I],X+135,510,.82f,AHUI::Text,true);
            Label(Passive[I],X+135,541,.75f,AHUI::Text,true);
            Label(TEXT("JOGAR"),X+135,585,1.f,AHUI::Green,true);
            AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+290*Scale),FVector2D(270*Scale,330*Scale),Name,true,2);
        }
        Label(TEXT("Habilidades limitadas ao duelo / modelos e animacoes ainda provisorios"),800,676,.9f,AHUI::Gold,true);
        return;
    }
    Label(TEXT("A S H E N   H O L L O W"),32,28,1.1f,AHUI::Bright);
    Label(TEXT("PATIO DOS JURAMENTOS"),33,51,.76f,AHUI::Dim);
    if(Mode && Mode->bStarted)
    {
        Label(FString::Printf(TEXT("RODADA %d  /  INICIATIVA"),Mode->Round),800,19,.85f,AHUI::Text,true);
        for(int I=0;I<Mode->Order.Num();++I)
        {
            const auto* Actor=Mode->Order[I].Get(); const float X=630+I*178;
            Panel(X,45,164,63,Actor->bTurnActive); Icon(Actor->bEnemy?TEXT("Attack"):TEXT("Dodge"),X+25,76,Actor->bEnemy?AHUI::Amber:AHUI::Green);
            Label(Actor->bEnemy?FString(TEXT("THORNBOUND")):AAHCharacter::ClassName(Actor->HeroClass),X+49,57,.85f,Actor->IsAlive()?AHUI::Text:AHUI::Dim);
            Label(FString::Printf(TEXT("%d  /  %d PV"),Actor->Initiative,Actor->Health),X+49,80,.78f,AHUI::Gold);
        }
    }
    Panel(28,738,276,130,Hero->bTurnActive);
    Panel(42,752,74,101); Icon(TEXT("Dodge"),79,790,AHUI::Bright);
    Label(TEXT("I"),79,829,1.1f,AHUI::Bright,true);
    Label(AAHCharacter::ClassName(Hero->HeroClass),132,752,1.04f,AHUI::Text);
    Label(FString::Printf(TEXT("%d / %d PV    CA %d"),Hero->Health,Hero->MaxHealth,Hero->ArmorClass),132,779,.9f,AHUI::Text);
    DrawRect(FLinearColor(.1f,.014f,.018f,1),OffsetX+132*Scale,OffsetY+805*Scale,153*Scale,7*Scale);
    DrawRect(FLinearColor(.6f,.045f,.04f,1),OffsetX+132*Scale,OffsetY+805*Scale,153*Scale*Hero->Health/Hero->MaxHealth,7*Scale);
    Label(Hero->bDodging?TEXT("ESQUIVANDO"):Hero->IsAlive()?TEXT("NIVEL 1"):TEXT("CAIDO"),132,832,.8f,AHUI::Gold);
    Panel(326,738,908,130,Hero->bTurnActive);
    Label(Hero->bTurnActive?TEXT("SEU TURNO"):TEXT("AGUARDE O INIMIGO"),345,750,.95f,Hero->bTurnActive?AHUI::Bright:AHUI::Dim);
    Label(Hero->Turn.bAction?TEXT("ACAO  [1]"):TEXT("ACAO  [0]"),563,750,.87f,Hero->Turn.bAction?AHUI::Green:AHUI::Dim);
    Label(Hero->Turn.bBonus?TEXT("BONUS  [1]"):TEXT("BONUS  [0]"),682,750,.87f,Hero->Turn.bBonus?AHUI::Amber:AHUI::Dim);
    Label(FString::Printf(TEXT("MOVIMENTO  %.1f m"),Hero->Turn.Movement/100.f),824,750,.87f,AHUI::Text);
    Button(TEXT("Attack"),TEXT("Q"),TEXT("ATAQUE"),345,780,Ready&&Hero->Turn.bAction);
    Button(TEXT("Dodge"),TEXT("SPACE"),TEXT("ESQUIVA"),437,780,Ready&&Hero->Turn.bAction);
    Button(TEXT("Heal"),TEXT("E"),Hero->ClassAbilityName(),529,780,Hero->CanUseClassAbility());
    Label(Hero->bRaging?TEXT("FURIA ATIVA"):FString::Printf(TEXT("USOS %d"),Hero->HeroClass==EAHHeroClass::Fighter?(Hero->bSecondWindUsed?0:1):Hero->ClassCharges),902,827,.78f,AHUI::Gold,true);
    Button(TEXT("Arcana"),TEXT("C"),TEXT("ARCANA"),621,780,Ready&&Hero->Turn.bAction);
    Button(TEXT("Dash"),TEXT("R"),TEXT("DISPARADA"),713,780,Ready&&Hero->Turn.bAction);
    Panel(1010,784,204,70,HoveredBox==TEXT("EndTurn")&&Ready);
    Label(TEXT("ENCERRAR TURNO"),1112,801,.95f,Ready?AHUI::Bright:AHUI::Dim,true);
    Label(TEXT("ENTER"),1112,830,.75f,AHUI::Text,true);
    AddHitBox(FVector2D(OffsetX+1010*Scale,OffsetY+784*Scale),FVector2D(204*Scale,70*Scale),TEXT("EndTurn"),true,1);
    Label(TEXT("Botao direito: mover / selecionar alvo     F5: reiniciar encontro"),800,880,.75f,AHUI::Dim,true);
    FString Tooltip=Hero->Feedback;
    if(HoveredBox==TEXT("Attack")) Tooltip=FString::Printf(TEXT("Ataque / 1 acao / +%d para acertar / 1d%d+%d de dano"),Hero->AttackBonus,Hero->DamageSides,Hero->DamageModifier+(Hero->bRaging?2:0));
    if(HoveredBox==TEXT("Dodge")) Tooltip=TEXT("Esquiva / 1 acao / desvantagem nos ataques recebidos ate seu proximo turno");
    if(HoveredBox==TEXT("Heal")) Tooltip=Hero->ClassAbilityDescription();
    if(HoveredBox==TEXT("Dash")) Tooltip=TEXT("Disparada / 1 acao / ganha mais 9 metros de movimento neste turno");
    if(HoveredBox==TEXT("Arcana")) Tooltip=TEXT("Teste de Arcana / 1 acao / d20+1 contra dificuldade 12");
    if(HoveredBox==TEXT("EndTurn")) Tooltip=TEXT("Encerra seu turno. Acao, bonus e movimento renovam no proximo turno.");
    Label(Tooltip,780,707,.91f,AHUI::Text,true);
    Panel(1254,738,318,130); Label(TEXT("REGISTRO DE COMBATE"),1270,749,.79f,AHUI::Gold);
    for(int I=0;I<Hero->CombatLog.Num();++I) Label(Hero->CombatLog[I],1270,773+I*17,.68f,AHUI::Text);
    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
    {
        if(It->IsAlive())
        {
            const FVector Feet=It->GetActorLocation()-FVector(0,0,91);
            const FLinearColor Team=It->bEnemy?AHUI::Amber:AHUI::Green;
            for(int I=0;I<40;++I)
            {
                const float A=I*2*PI/40,B=(I+1)*2*PI/40;
                FVector2D PA,PB;
                if(PC->ProjectWorldLocationToScreen(Feet+FVector(FMath::Cos(A)*55,FMath::Sin(A)*55,0),PA) && PC->ProjectWorldLocationToScreen(Feet+FVector(FMath::Cos(B)*55,FMath::Sin(B)*55,0),PB))
                    if(PA.Y>OffsetY+120*Scale && PB.Y>OffsetY+120*Scale && PA.Y<OffsetY+700*Scale && PB.Y<OffsetY+700*Scale)
                        DrawLine(PA.X,PA.Y,PB.X,PB.Y,Team,It->bTurnActive?2.f*Scale:Scale);
            }
        }
        if(!It->bEnemy && Now-It->ImpactTextTime>1.6f && !It->bIsAttacking) continue;
        FVector2D Pos; if(!PC->ProjectWorldLocationToScreen(It->GetActorLocation()+FVector(0,0,165),Pos)) continue;
        const float X=(Pos.X-OffsetX)/Scale,Y=(Pos.Y-OffsetY)/Scale;
        // World labels stay clear of the initiative strip and action bar.
        if(Y<120 || Y>680 || X<30 || X>1220) continue;
        if(It->bEnemy && It->IsAlive())
        {
            Label(TEXT("THORNBOUND"),X,Y,.84f,AHUI::Text,true);
            DrawRect(FLinearColor(.075f,.013f,.013f,1),Pos.X-52*Scale,Pos.Y+19*Scale,104*Scale,4*Scale);
            DrawRect(FLinearColor(.62f,.075f,.03f,1),Pos.X-52*Scale,Pos.Y+19*Scale,104*Scale*It->Health/It->MaxHealth,4*Scale);
        }
        const float ImpactAge=Now-It->ImpactTextTime;
        if(ImpactAge>=0.f && ImpactAge<1.6f && !It->ImpactText.IsEmpty())
        {
            FLinearColor Color=It->bImpactHealing?AHUI::Green:It->bLastImpactCritical?AHUI::Bright:AHUI::Amber;
            Color.A=1.f-FMath::Clamp((ImpactAge-1.f)/.6f,0.f,1.f);
            Label(It->ImpactText,X,Y-30-ImpactAge*24,It->bLastImpactCritical?1.65f:1.4f,Color,true);
        }
        else if(It->bIsAttacking) Label(TEXT("ATACANDO"),X,Y-28,.85f,AHUI::Gold,true);
    }
    if(PC->HoveredEnemy && PC->HoveredEnemy->IsAlive())
    {
        const float BaseChance=FMath::Clamp(21+Hero->AttackBonus-PC->HoveredEnemy->ArmorClass,1,19)*.05f;
        const int Chance=FMath::RoundToInt(100*(PC->HoveredEnemy->bDodging?BaseChance*BaseChance:BaseChance));
        Panel(655,133,290,63); Label(TEXT("THORNBOUND"),800,143,1.f,AHUI::Bright,true);
        Label(FString::Printf(TEXT("%d%% de acerto  /  CA %d"),Chance,PC->HoveredEnemy->ArmorClass),800,170,.88f,AHUI::Text,true);
    }
    const float Age=Now-Hero->LastRollTime;
    if(Hero->LastRollTime>=0 && Age<8.f)
    {
        const auto& Roll=Hero->LastRoll; Panel(1270,194,288,282);
        Label(Hero->LastRollLabel,1414,210,.79f,AHUI::Gold,true);
        const float T=FMath::Clamp(Age/.7f,0.f,1.f); const float Angle=30+300*(1-FMath::Pow(1-T,3));
        DrawDie(1414,314,75,Angle,AHUI::Bright);
        Label(FString::FromInt(Roll.NaturalRoll),1414,297,2.8f,FLinearColor::White,true);
        Label(FString::Printf(TEXT("%d + %d = %d"),Roll.NaturalRoll,Roll.Modifier,Roll.Total),1414,407,1.3f,AHUI::Text,true);
        Label(FString::Printf(TEXT("%s  /  alvo %d  /  dano %d"),Roll.bCritical?TEXT("CRITICO"):Roll.bSuccess?TEXT("ACERTO"):TEXT("FALHA"),Roll.Target,Roll.Damage),1414,444,.82f,Roll.bSuccess?AHUI::Green:AHUI::Amber,true);
    }
    if(Mode && Mode->bFinished)
    {
        Panel(590,205,420,94,true); Label(Hero->IsAlive()?TEXT("V I T O R I A"):TEXT("D E R R O T A"),800,224,1.5f,AHUI::Bright,true);
        Label(TEXT("F5  /  reiniciar encontro"),800,269,.9f,AHUI::Text,true);
    }
}
