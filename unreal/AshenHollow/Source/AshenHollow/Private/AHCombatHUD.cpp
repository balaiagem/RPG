#include "AHCombatHUD.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Fonts/CompositeFont.h"

// ════════════════════════════════════════════════════════════════════════════
// PALETTE
// ════════════════════════════════════════════════════════════════════════════
namespace AHUI
{
    const FLinearColor Ink   (.008f,.011f,.017f,.96f);
    const FLinearColor Shadow(.0f,.0f,.0f,.55f);
    const FLinearColor Gold  (.62f,.39f,.15f,1.f);
    const FLinearColor Bright(.95f,.74f,.36f,1.f);
    const FLinearColor Copper(.48f,.26f,.10f,1.f);
    const FLinearColor Text  (.84f,.81f,.71f,1.f);
    const FLinearColor Dim   (.38f,.35f,.30f,1.f);
    const FLinearColor White (1.f,1.f,1.f,1.f);
    const FLinearColor Green (.22f,.68f,.40f,1.f);
    const FLinearColor Amber (.92f,.46f,.13f,1.f);
    const FLinearColor Red   (.75f,.12f,.10f,1.f);
    const FLinearColor Teal  (.15f,.62f,.55f,1.f);
    const FLinearColor Purple(.52f,.22f,.72f,1.f);

    FName AbilityIcon(EAHHeroClass C)  { return FName(AHRules::Class(C).Icon); }
    FLinearColor ClassColor(EAHHeroClass C) { return AHRules::Class(C).Colour; }
}

// ════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════
AAHCombatHUD::AAHCombatHUD()
{
    struct FL { static UFont* Try(const TCHAR* P)
    { return Cast<UFont>(StaticLoadObject(UFont::StaticClass(),nullptr,P,nullptr,LOAD_NoWarn|LOAD_Quiet)); }};
    CinzelFont     = FL::Try(TEXT("/Game/AshenHollow/Fonts/Cinzel-Regular"));
    CinzelBoldFont = FL::Try(TEXT("/Game/AshenHollow/Fonts/Cinzel-Bold"));
    if(!CinzelFont)
    {
        const FString File=FPaths::ProjectContentDir()/TEXT("AshenHollow/Fonts/Cinzel-Regular.ttf");
        if(IFileManager::Get().FileExists(*File))
        {
            CinzelFont=CreateDefaultSubobject<UFont>(TEXT("CinzelRuntime"));
            CinzelFont->FontCacheType=EFontCacheType::Runtime;
            CinzelFont->GetMutableInternalCompositeFont().DefaultTypeface.AppendFont(TEXT("Regular"),File,EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        }
    }
    if(!CinzelBoldFont)
    {
        const FString File=FPaths::ProjectContentDir()/TEXT("AshenHollow/Fonts/Cinzel-Bold.ttf");
        if(IFileManager::Get().FileExists(*File))
        {
            CinzelBoldFont=CreateDefaultSubobject<UFont>(TEXT("CinzelBoldRuntime"));
            CinzelBoldFont->FontCacheType=EFontCacheType::Runtime;
            CinzelBoldFont->GetMutableInternalCompositeFont().DefaultTypeface.AppendFont(TEXT("Regular"),File,EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        }
    }
}

UFont* AAHCombatHUD::F(bool bBold) const
{
    if(bBold && CinzelBoldFont) return CinzelBoldFont.Get();
    if(!bBold && CinzelFont)    return CinzelFont.Get();
    return GEngine->GetSmallFont();
}

// ════════════════════════════════════════════════════════════════════════════
// PRIMITIVE HELPERS
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawFilledCircle(float X,float Y,float R,FLinearColor Fill,FLinearColor Border,float Thickness)
{
    const int32 N=24;
    const FVector2D C(OffsetX+X*Scale,OffsetY+Y*Scale);
    const float RS=R*Scale;
    for(int32 I=0;I<N;++I)
    {
        const float A0=I*2.f*PI/N, A1=(I+1)*2.f*PI/N;
        FCanvasTriangleItem T(C,
            FVector2D(C.X+RS*FMath::Cos(A0),C.Y+RS*FMath::Sin(A0)),
            FVector2D(C.X+RS*FMath::Cos(A1),C.Y+RS*FMath::Sin(A1)),GWhiteTexture);
        T.SetColor(Fill); Canvas->DrawItem(T);
    }
    for(int32 I=0;I<N;++I)
    {
        const float A0=I*2.f*PI/N, A1=(I+1)*2.f*PI/N;
        DrawLine(C.X+RS*FMath::Cos(A0),C.Y+RS*FMath::Sin(A0),
                 C.X+RS*FMath::Cos(A1),C.Y+RS*FMath::Sin(A1),Border,Thickness*Scale);
    }
}

void AAHCombatHUD::Label(const FString& Txt,float X,float Y,float Size,FLinearColor Color,bool Center,bool bBold)
{
    Size*=1.25f; UFont* Font=F(bBold);
    float TW=0,TH=0;
    if(Center) GetTextSize(Txt,TW,TH,Font,Size*Scale);
    DrawText(Txt,FLinearColor(0,0,0,Color.A*.85f),OffsetX+X*Scale-TW*.5f+1.5f*Scale,OffsetY+Y*Scale+1.5f*Scale,Font,Size*Scale);
    DrawText(Txt,Color,OffsetX+X*Scale-TW*.5f,OffsetY+Y*Scale,Font,Size*Scale);
}

void AAHCombatHUD::Panel(float X,float Y,float W,float H,bool Active)
{
    const float PX=OffsetX+X*Scale,PY=OffsetY+Y*Scale,PW=W*Scale,PH=H*Scale;
    DrawRect(AHUI::Shadow,PX+4*Scale,PY+5*Scale,PW,PH);
    DrawRect(AHUI::Ink,PX,PY,PW,PH);
    DrawRect(FLinearColor(.09f,.07f,.05f,.18f),PX,PY,PW,PH*.35f);
    const FLinearColor B=Active?AHUI::Bright:AHUI::Gold;
    const FLinearColor S=Active?AHUI::Copper:FLinearColor(AHUI::Gold.R*.5f,AHUI::Gold.G*.5f,AHUI::Gold.B*.5f,1.f);
    const float T=FMath::Max(1.2f,Scale);
    DrawLine(PX,PY,PX+PW,PY,B,T); DrawLine(PX,PY+PH,PX+PW,PY+PH,B,T);
    DrawLine(PX,PY,PX,PY+PH,S,T); DrawLine(PX+PW,PY,PX+PW,PY+PH,S,T);
    const float Fg=5*Scale;
    for(float CX:{PX,PX+PW}) for(float CY:{PY,PY+PH})
    { float DX=CX==PX?1.f:-1.f,DY=CY==PY?1.f:-1.f;
      DrawLine(CX,CY,CX+DX*Fg,CY,B,T); DrawLine(CX,CY,CX,CY+DY*Fg,B,T); }
    if(Active) DrawRect(FLinearColor(.30f,.18f,.04f,.10f),PX,PY,PW,PH);
}

void AAHCombatHUD::Icon(FName Type,float X,float Y,FLinearColor Color)
{
    auto L=[&](float A,float B,float C,float D){DrawLine(OffsetX+(X+A)*Scale,OffsetY+(Y+B)*Scale,OffsetX+(X+C)*Scale,OffsetY+(Y+D)*Scale,Color,2*Scale);};
    if(Type==TEXT("Attack"))     { L(-15,16,15,-16);L(15,-16,13,-5);L(15,-16,4,-14);L(-11,4,-2,13);L(-17,18,-13,21); }
    else if(Type==TEXT("Dodge")) { L(-15,-15,0,-20);L(0,-20,15,-15);L(15,-15,12,7);L(12,7,0,20);L(0,20,-12,7);L(-12,7,-15,-15);L(0,-14,0,12); }
    else if(Type==TEXT("Heal"))  { L(-16,0,16,0);L(0,-16,0,16);L(-10,-10,-5,-15);L(5,15,10,10); }
    else if(Type==TEXT("Rage"))  { L(-17,-18,-13,7);L(-13,7,0,20);L(0,20,13,7);L(13,7,17,-18);L(-17,-18,-5,-7);L(17,-18,5,-7);L(-9,2,-3,5);L(3,5,9,2);L(-5,12,5,12); }
    else if(Type==TEXT("Missiles")) { for(int I=-1;I<=1;++I){const float D=I*12.f;L(-18,D+8,9,D-5);L(9,D-5,2,D-6);L(9,D-5,5,D+2);} }
    else if(Type==TEXT("SecondWind")) { L(0,18,-17,0);L(-17,0,-13,-12);L(-13,-12,-5,-14);L(-5,-14,0,-8);L(0,-8,5,-14);L(5,-14,13,-12);L(13,-12,17,0);L(17,0,0,18);L(-10,1,-4,1);L(-4,1,0,-5);L(0,-5,4,6);L(4,6,7,1);L(7,1,12,1); }
    else if(Type==TEXT("Dash"))  { L(-17,-14,-3,0);L(-3,0,-17,14);L(0,-14,14,0);L(14,0,0,14); }
    else if(Type==TEXT("Breath")) { L(-16,-10,10,-18);L(-16,10,10,18);L(-16,-10,-16,10);L(2,-12,8,-6);L(2,12,8,6);L(6,-2,12,0);L(6,2,12,0); }
    // Disengage: breaking away from a threat line
    else if(Type==TEXT("Disengage")) { L(-15,-16,-15,16);L(-9,-10,1,0);L(1,0,-9,10);L(3,-10,13,0);L(13,0,3,10); }
    // Shield (dodge status)
    else if(Type==TEXT("Shield")){ L(-10,-14,10,-14);L(10,-14,10,4);L(10,4,0,14);L(0,14,-10,4);L(-10,4,-10,-14); }
    // Flame (rage)
    else if(Type==TEXT("Flame")) { L(0,14,0,-4);L(0,-4,-8,4);L(0,-4,8,4);L(0,-14,0,-4);L(-5,-8,0,-18);L(0,-18,5,-8); }
    // Skull (low HP)
    else if(Type==TEXT("Skull")) { L(-10,0,-10,-8);L(-10,-8,0,-16);L(0,-16,10,-8);L(10,-8,10,0);L(10,0,-10,0);L(-4,4,4,4);L(-4,4,-4,10);L(4,4,4,10); }
    else { L(0,-20,17,0);L(17,0,0,20);L(0,20,-17,0);L(-17,0,0,-20);L(-17,0,17,0);L(0,-20,-6,0);L(-6,0,0,20); }
}

void AAHCombatHUD::Button(FName Name,const FString& Key,const FString& Title,float X,float Y,bool Enabled,float TitleSize)
{
    const bool Hover=HoveredBox==Name;
    Panel(X,Y,82,78,Hover&&Enabled);
    if(Hover)  DrawRect(FLinearColor(.22f,.14f,.05f,.25f),OffsetX+(X+2)*Scale,OffsetY+(Y+2)*Scale,78*Scale,74*Scale);
    if(!Enabled) DrawRect(FLinearColor(0,0,0,.20f),OffsetX+X*Scale,OffsetY+Y*Scale,82*Scale,78*Scale);
    FName Symbol=Name;
    if(Name==TEXT("Heal"))
        if(const auto* Hero=Cast<AAHCharacter>(GetOwningPawn())) Symbol=AHUI::AbilityIcon(Hero->HeroClass);
    Icon(Symbol,X+41,Y+32,Enabled?AHUI::Bright:AHUI::Dim);
    DrawRect(FLinearColor(.0f,.0f,.0f,.55f),OffsetX+X*Scale,OffsetY+Y*Scale,20*Scale,16*Scale);
    Label(Key,X+3,Y+4,.7f,Enabled?AHUI::Gold:AHUI::Dim);
    Label(Title,X+41,Y+60,TitleSize,Enabled?AHUI::Text:AHUI::Dim,true);
    AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(82*Scale,78*Scale),Name,true,1);
}

// ════════════════════════════════════════════════════════════════════════════
// HP ORB  — circular arc drain + optional TempHP white overlay arc
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawHPOrb(float CX,float CY,float R,int32 HP,int32 MaxHP,bool bEnemy,int32 TempHP)
{
    const float Frac=MaxHP>0?FMath::Clamp((float)HP/(float)MaxHP,0.f,1.f):0.f;
    DrawFilledCircle(CX,CY,R,FLinearColor(.04f,.02f,.02f,1.f),FLinearColor(.18f,.10f,.05f,1.f),1.2f);
    DrawFilledCircle(CX,CY,R*.75f,FLinearColor(.025f,.012f,.012f,1.f),FLinearColor(0,0,0,0),0.f);

    const FLinearColor ArcColor=HP<=0?AHUI::Red:Frac>.55f?AHUI::Green:Frac>.28f?AHUI::Amber:AHUI::Red;
    const FLinearColor ArcBG(0.1f,0.05f,0.03f,1.f);
    const int32 N=48;
    const float RingR=R*0.87f;
    const float Thick=FMath::Max(R*0.18f*Scale, 2.f*Scale);
    const float CXS=OffsetX+CX*Scale, CYS=OffsetY+CY*Scale;

    // Main HP arc
    for(int32 I=0;I<N;++I)
    {
        const float A0=-PI*.5f+I*2.f*PI/N;
        const float A1=-PI*.5f+(I+1)*2.f*PI/N;
        const bool  bFilled=(float)I/N < Frac;
        DrawLine(CXS+RingR*Scale*FMath::Cos(A0),CYS+RingR*Scale*FMath::Sin(A0),
                 CXS+RingR*Scale*FMath::Cos(A1),CYS+RingR*Scale*FMath::Sin(A1),
                 bFilled?ArcColor:ArcBG, Thick);
    }

    // TempHP white outer arc — fills from top clockwise proportional to TempHP/MaxHP
    if(TempHP > 0)
    {
        const float TFrac=FMath::Clamp((float)TempHP/FMath::Max(MaxHP,1),0.f,1.f);
        const float TRingR=R*1.04f;
        const FLinearColor TC(1.f,1.f,1.f,0.80f);
        for(int32 I=0;I<N;++I)
        {
            if((float)I/N >= TFrac) break;
            const float A0=-PI*.5f+I*2.f*PI/N;
            const float A1=-PI*.5f+(I+1)*2.f*PI/N;
            DrawLine(CXS+TRingR*Scale*FMath::Cos(A0),CYS+TRingR*Scale*FMath::Sin(A0),
                     CXS+TRingR*Scale*FMath::Cos(A1),CYS+TRingR*Scale*FMath::Sin(A1),
                     TC, Thick*.55f);
        }
    }

    DrawFilledCircle(CX,CY-R*.72f,R*.09f,FLinearColor(1,1,1,.18f),FLinearColor(0,0,0,0),0.f);

    if(HP <= 0)
        Label(TEXT("KO"),CX,CY-7,1.1f,AHUI::Red,true,true);
    else
        Label(FString::FromInt(HP),CX,CY-7,1.1f,ArcColor,true,true);
    Label(FString::Printf(TEXT("/%d"),MaxHP),CX,CY+10,.65f,AHUI::Dim,true);
    if(TempHP>0) Label(FString::Printf(TEXT("+%d tmp"),TempHP),CX,CY+22,.55f,FLinearColor(1,1,1,.70f),true);
}

// ════════════════════════════════════════════════════════════════════════════
// CHARGE ORBS  — ability uses / spell slots
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawChargeOrbs(const AAHCharacter* Hero,float X,float Y)
{
    int32 Total=1, Remaining=1;
    switch(Hero->HeroClass)
    {
    case EAHHeroClass::Fighter:
        Total=1; Remaining=Hero->bSecondWindUsed?0:1; break;
    case EAHHeroClass::Barbarian:
        Total=2; Remaining=Hero->ClassCharges; break;
    case EAHHeroClass::Cleric:
        Total=Hero->MaxSpellSlots(1); Remaining=Hero->ClassCharges; break;
    case EAHHeroClass::Wizard:
        Total=Hero->MaxSpellSlots(1); Remaining=Hero->ClassCharges; break;
    }
    const float Spacing=13.f;
    const float StartX=X-(Total-1)*Spacing*.5f;
    for(int32 I=0;I<Total;++I)
    {
        const bool bFull=I<Remaining;
        const FLinearColor CC=AHUI::ClassColor(Hero->HeroClass);
        DrawFilledCircle(StartX+I*Spacing,Y,5.f,
            bFull?CC:FLinearColor(.04f,.04f,.04f,1.f),
            bFull?CC:AHUI::Dim, 1.2f);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// STATUS ICONS  — small icons above character head
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawStatusIcons(float SX,float SY,const AAHCharacter* Actor)
{
    float IX=SX-22.f;
    auto NextIcon=[&](FName Type,FLinearColor Color)
    {
        // Tiny icon with background pill
        DrawRect(FLinearColor(0,0,0,.55f),OffsetX+(IX-10)*Scale,OffsetY+(SY-12)*Scale,20*Scale,20*Scale);
        Icon(Type,IX,SY,Color);
        IX+=22.f;
    };
    if(Actor->bDodging)                                   NextIcon(TEXT("Shield"),AHUI::Teal);
    if(Actor->bRaging)                                    NextIcon(TEXT("Flame"),AHUI::Red);
    if(Actor->bDashing)                                   NextIcon(TEXT("Dash"),AHUI::Green);
    if(Actor->Health>0 && Actor->Health*4<Actor->MaxHealth) NextIcon(TEXT("Skull"),AHUI::Red);
}

// ════════════════════════════════════════════════════════════════════════════
// DEATH SAVES  — 3-heart / 3-skull tracker above a downed character
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawDeathSaves(float SX,float SY,const AAHCharacter* Actor)
{
    if(!Actor->bDowned && !Actor->bStabilized) return;
    // Flash on recent roll
    const float RollAge=GetWorld()->GetTimeSeconds()-Actor->DeathSaveRollTime;
    const float Flash=RollAge<0.6f?FMath::Clamp(1.f-RollAge/.6f,0.f,1.f):0.f;

    const float StartX=SX-32.f, GapX=14.f;
    // Successes (hearts) — top row
    for(int32 I=0;I<3;++I)
    {
        const bool bFull=I<Actor->DeathSuccesses;
        FLinearColor C=bFull?AHUI::Green:AHUI::Dim;
        if(bFull && I==Actor->DeathSuccesses-1) C=FLinearColor::LerpUsingHSV(C,AHUI::White,Flash);
        DrawFilledCircle(StartX+I*GapX,SY-8,4.f,bFull?C:FLinearColor(.04f,.04f,.04f,1.f),C,1.f);
    }
    // Failures (skulls) — bottom row
    for(int32 I=0;I<3;++I)
    {
        const bool bFull=I<Actor->DeathFailures;
        FLinearColor C=bFull?AHUI::Red:AHUI::Dim;
        if(bFull && I==Actor->DeathFailures-1 && !Actor->bLastDeathSaveSuccess) C=FLinearColor::LerpUsingHSV(C,AHUI::White,Flash);
        DrawFilledCircle(StartX+I*GapX,SY+4,4.f,bFull?C:FLinearColor(.04f,.04f,.04f,1.f),C,1.f);
    }
    // Label
    if(Actor->bStabilized)
        Label(TEXT("ESTÁVEL"),SX,SY+18,.68f,AHUI::Teal,true);
    else
        Label(TEXT("NOCAUTEADO"),SX,SY+18,.65f,AHUI::Amber,true);
}

// ════════════════════════════════════════════════════════════════════════════
// TARGET BRACKETS  — animated corner brackets around hovered enemy
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawTargetBrackets(const AAHCharacter* Target,const AAHPlayerController* PC,float Now)
{
    FVector2D Centre; if(!PC->ProjectWorldLocationToScreen(Target->GetActorLocation()+FVector(0,0,50),Centre)) return;
    const float Pulse=FMath::Sin(Now*4.f)*.04f;
    const float BW=54.f*Scale*(1.f+Pulse), BH=74.f*Scale*(1.f+Pulse);
    const float L=Centre.X-BW, R=Centre.X+BW, T=Centre.Y-BH, Bo=Centre.Y+BH;
    const float Arm=14.f*Scale;
    const FLinearColor BC(0.95f,0.74f,0.36f,0.85f);
    const float Th=1.8f*Scale;
    // Top-left
    DrawLine(L,T,L+Arm,T,BC,Th); DrawLine(L,T,L,T+Arm,BC,Th);
    // Top-right
    DrawLine(R-Arm,T,R,T,BC,Th); DrawLine(R,T,R,T+Arm,BC,Th);
    // Bottom-left
    DrawLine(L,Bo,L+Arm,Bo,BC,Th); DrawLine(L,Bo-Arm,L,Bo,BC,Th);
    // Bottom-right
    DrawLine(R-Arm,Bo,R,Bo,BC,Th); DrawLine(R,Bo-Arm,R,Bo,BC,Th);
    // Centre crosshair dots
    DrawLine(Centre.X-4*Scale,Centre.Y,Centre.X-2*Scale,Centre.Y,BC,Th);
    DrawLine(Centre.X+2*Scale,Centre.Y,Centre.X+4*Scale,Centre.Y,BC,Th);
    DrawLine(Centre.X,Centre.Y-4*Scale,Centre.X,Centre.Y-2*Scale,BC,Th);
    DrawLine(Centre.X,Centre.Y+2*Scale,Centre.X,Centre.Y+4*Scale,BC,Th);
}

// ════════════════════════════════════════════════════════════════════════════
// MOVEMENT RING  — BG3 dashed ground circle
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawMovementRing(const AAHCharacter* Hero,const AAHPlayerController* PC,float Now)
{
    if(!Hero->bTurnActive || Hero->Turn.Movement<=1.f) return;
    const FVector Origin=Hero->GetActorLocation();
    const float   Radius=Hero->Turn.Movement;
    const int32   Segs=72;
    const FLinearColor Ring(0.15f,0.75f,0.85f,0.80f);
    TArray<FVector2D> Pts; Pts.Reserve(Segs);
    for(int32 I=0;I<Segs;++I)
    {
        const float A=(I*2.f*PI/Segs)+Now*.3f;   // slowly rotates
        FVector2D P; PC->ProjectWorldLocationToScreen(Origin+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,-88.f),P);
        Pts.Add(P);
    }
    // Dashed outline (every 3rd segment solid)
    for(int32 I=0;I<Segs;++I)
    {
        if(I%3==2) continue;
        const FVector2D& A=Pts[I], &B=Pts[(I+1)%Segs];
        if(A.Y<OffsetY+120*Scale||B.Y<OffsetY+120*Scale||A.Y>OffsetY+700*Scale||B.Y>OffsetY+700*Scale) continue;
        DrawLine(A.X,A.Y,B.X,B.Y,Ring,1.8f*Scale);
    }
    // Label
    FVector2D TopEdge; if(PC->ProjectWorldLocationToScreen(Origin+FVector(0,Radius,-88.f),TopEdge))
        if(TopEdge.Y>OffsetY+130*Scale&&TopEdge.Y<OffsetY+690*Scale)
            Label(FString::Printf(TEXT("%.0f m"),Hero->Turn.Movement/100.f),
                  (TopEdge.X-OffsetX)/Scale,(TopEdge.Y-OffsetY)/Scale-14.f,
                  .72f,FLinearColor(0.15f,0.75f,0.85f,0.75f),true);
}

// ════════════════════════════════════════════════════════════════════════════
// SCREEN FX
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawDamageVignette(float Age)
{
    float A=Age<.15f?Age/.15f:1.f-FMath::Clamp((Age-.15f)/.55f,0.f,1.f); A*=.72f;
    if(A<=0.f) return;
    const float W=Canvas->SizeX,H=Canvas->SizeY,EW=W*.22f,EH=H*.22f;
    const FLinearColor V(.55f,0.f,0.f,A);
    DrawRect(V,0,0,EW,H); DrawRect(V,W-EW,0,EW,H);
    DrawRect(V,EW,0,W-2*EW,EH); DrawRect(V,EW,H-EH,W-2*EW,EH);
}

void AAHCombatHUD::DrawCritFlash(float Age)
{
    if(Age>.35f) return;
    float A=Age<.06f?Age/.06f:1.f-(Age-.06f)/.29f; A*=.50f;
    DrawRect(FLinearColor(1.f,.9f,.6f,A),0,0,Canvas->SizeX,Canvas->SizeY);
    // Gold inner burst rings
    const float CX=Canvas->SizeX*.5f,CY=Canvas->SizeY*.5f;
    for(int32 RI=0;RI<3;++RI)
    {
        const float RR=(80+RI*90)*Scale*(1.f+Age*4.f);
        const FLinearColor RC(1.f,.74f,.2f,A*.6f*(1.f-(float)RI/3.f));
        const int32 SN=32;
        for(int32 I=0;I<SN;++I)
        {
            const float A0=I*2.f*PI/SN,A1=(I+1)*2.f*PI/SN;
            DrawLine(CX+RR*FMath::Cos(A0),CY+RR*FMath::Sin(A0),CX+RR*FMath::Cos(A1),CY+RR*FMath::Sin(A1),RC,2.f*Scale);
        }
    }
}

void AAHCombatHUD::DrawTurnBanner(float Age,bool bHero)
{
    if(Age>1.9f) return;
    float Slide=FMath::Clamp(Age/.35f,0.f,1.f); Slide=1.f-FMath::Pow(1.f-Slide,3.f);
    float Alpha=Age>1.4f?1.f-FMath::Clamp((Age-1.4f)/.5f,0.f,1.f):1.f;
    const float BY=310.f-(1.f-Slide)*200.f;
    const float BX=560.f,BW=480.f,BH=64.f;
    DrawRect(FLinearColor(0,0,0,.78f*Alpha),OffsetX+BX*Scale,OffsetY+BY*Scale,BW*Scale,BH*Scale);
    const FLinearColor BC=bHero?FLinearColor(.15f,.75f,.85f,Alpha):FLinearColor(.80f,.20f,.10f,Alpha);
    DrawRect(BC,OffsetX+BX*Scale,OffsetY+BY*Scale,5*Scale,BH*Scale);
    DrawRect(BC,OffsetX+(BX+BW-5)*Scale,OffsetY+BY*Scale,5*Scale,BH*Scale);
    DrawLine(OffsetX+BX*Scale,OffsetY+BY*Scale,OffsetX+(BX+BW)*Scale,OffsetY+BY*Scale,BC,Scale);
    DrawLine(OffsetX+BX*Scale,OffsetY+(BY+BH)*Scale,OffsetX+(BX+BW)*Scale,OffsetY+(BY+BH)*Scale,BC,Scale);
    FLinearColor TC=AHUI::Bright; TC.A=Alpha;
    FLinearColor DC=AHUI::Dim;   DC.A=Alpha;
    Label(bHero?TEXT("SEU TURNO"):TEXT("TURNO DO INIMIGO"),800.f,BY+13.f,1.6f,TC,true,true);
    Label(bHero?TEXT("Escolha uma ação"):TEXT("Aguarde..."),              800.f,BY+41.f,.80f,DC,true);
}

void AAHCombatHUD::DrawRoundBanner(float Age,int32 Round)
{
    if(Age>2.2f) return;
    float Slide=FMath::Clamp(Age/.3f,0.f,1.f); Slide=1.f-FMath::Pow(1.f-Slide,3.f);
    float Alpha=Age>1.6f?1.f-FMath::Clamp((Age-1.6f)/.6f,0.f,1.f):1.f;
    const float BY=245.f-(1.f-Slide)*120.f;
    DrawRect(FLinearColor(0,0,0,.70f*Alpha),OffsetX+680*Scale,OffsetY+BY*Scale,240*Scale,36*Scale);
    DrawLine(OffsetX+680*Scale,OffsetY+BY*Scale,OffsetX+920*Scale,OffsetY+BY*Scale,AHUI::Gold*Alpha,Scale);
    DrawLine(OffsetX+680*Scale,OffsetY+(BY+36)*Scale,OffsetX+920*Scale,OffsetY+(BY+36)*Scale,AHUI::Gold*Alpha,Scale);
    FLinearColor TC=AHUI::Bright; TC.A=Alpha;
    Label(FString::Printf(TEXT("— RODADA %d —"),Round),800,BY+9,1.f,TC,true,true);
}

// ════════════════════════════════════════════════════════════════════════════
// HIT-BOX CALLBACKS
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::NotifyHitBoxClick(FName N)
{ if(auto* PC=Cast<AAHPlayerController>(GetOwningPlayerController())) PC->CombatCommand(N); }
void AAHCombatHUD::NotifyHitBoxBeginCursorOver(FName N) { HoveredBox=N; }
void AAHCombatHUD::NotifyHitBoxEndCursorOver(FName N)   { if(HoveredBox==N) HoveredBox=NAME_None; }
bool AAHCombatHUD::IsPointerOverInterface() const
{
    float X,Y; if(!GetOwningPlayerController()->GetMousePosition(X,Y)) return false;
    X=(X-OffsetX)/Scale; Y=(Y-OffsetY)/Scale;
    return Y>730||(Y<120&&X>525&&X<1100)||(X>1250&&Y>185&&Y<480);
}

// ════════════════════════════════════════════════════════════════════════════
// 3-D DIE
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawDie(float X,float Y,float Radius,float Angle,FLinearColor Color)
{
    const float P=(1.f+FMath::Sqrt(5.f))*.5f;
    const FVector Verts[]={{-1,P,0},{1,P,0},{-1,-P,0},{1,-P,0},{0,-1,P},{0,1,P},{0,-1,-P},{0,1,-P},{P,0,-1},{P,0,1},{-P,0,-1},{-P,0,1}};
    FVector V[12]; for(int I=0;I<12;++I) V[I]=FRotator(18,Angle,12).RotateVector(Verts[I])/1.91f;
    static const TArray<FIntVector> Topo=[&](){
        TArray<FIntVector> R; R.Reserve(20);
        for(int A=0;A<12;++A) for(int B=A+1;B<12;++B) for(int C=B+1;C<12;++C)
            if(FMath::IsNearlyEqual(FVector::DistSquared(Verts[A],Verts[B]),4.f,.01f)&&
               FMath::IsNearlyEqual(FVector::DistSquared(Verts[A],Verts[C]),4.f,.01f)&&
               FMath::IsNearlyEqual(FVector::DistSquared(Verts[B],Verts[C]),4.f,.01f))
                R.Add(FIntVector(A,B,C));
        return R;
    }();
    struct FF{int A,B,C;float Z;}; TArray<FF,TInlineAllocator<20>> Fs;
    for(const FIntVector& F:Topo) Fs.Add({F.X,F.Y,F.Z,(float)(V[F.X].Z+V[F.Y].Z+V[F.Z].Z)});
    Fs.Sort([](const FF& A,const FF& B){return A.Z<B.Z;});
    auto Pt=[&](int I){return FVector2D(OffsetX+(X+V[I].X*Radius)*Scale,OffsetY+(Y+V[I].Y*Radius)*Scale);};
    for(const auto& Fc:Fs)
    {
        FCanvasTriangleItem Fi(Pt(Fc.A),Pt(Fc.B),Pt(Fc.C),GWhiteTexture);
        Fi.SetColor(FLinearColor(.028f,.08f,.075f,1)*FMath::Clamp(.7f+Fc.Z*.25f,.2f,1.f));
        Canvas->DrawItem(Fi);
        for(auto E:{TPair<int,int>(Fc.A,Fc.B),TPair<int,int>(Fc.B,Fc.C),TPair<int,int>(Fc.C,Fc.A)})
        { const auto A=Pt(E.Key),B=Pt(E.Value); DrawLine(A.X,A.Y,B.X,B.Y,Color*.8f,1.3f*Scale); }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// DRAW HUD
// ════════════════════════════════════════════════════════════════════════════
void AAHCombatHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Hero=Cast<AAHCharacter>(GetOwningPawn());
    auto* PC  =Cast<AAHPlayerController>(GetOwningPlayerController());
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if(!Canvas||!Hero||!PC) return;

    Scale  =FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);
    OffsetX=(Canvas->SizeX-1600*Scale)*.5f;
    OffsetY=(Canvas->SizeY-900*Scale)*.5f;

    const float Now=GetWorld()->GetTimeSeconds();
    const bool  Ready=Hero->CanAct();
    if(PC->IsReactionPending())
    {
        DrawRect(FLinearColor(0,0,0,.7f),OffsetX,OffsetY,1600*Scale,900*Scale);
        Panel(470,290,660,250,true);
        Label(TEXT("ATAQUE DE OPORTUNIDADE"),800,320,1.4f,AHUI::Gold,true);
        Label(TEXT("O inimigo saiu do alcance. Gastar sua reacao?"),800,365,.9f,AHUI::Text,true);
        Button(TEXT("ReactYes"),TEXT("Y"),TEXT("ATACAR"),690,410,true);
        Button(TEXT("ReactNo"),TEXT("N"),TEXT("PASSAR"),825,410,true);
        return;
    }
    if(Hero->bCharacterReady && (Hero->bPreparingSpells || PC->bSpellbookOpen))
    {
        Panel(230,90,1140,740,true);
        Label(AAHCharacter::ClassName(Hero->HeroClass)+TEXT(" / MAGIAS"),800,118,1.7f,AHUI::Gold,true);
        Label(Hero->bPreparingSpells?TEXT("Escolha seu repertorio. Truques disponiveis nao ocupam preparacao."):TEXT("Selecione uma magia preparada. E conjura no alvo sob o cursor ou mais proximo."),800,157,.85f,AHUI::Text,true);
        Label(FString::Printf(TEXT("Preparadas: %d / %d | Espacos I: %d | II: %d | Circulo escolhido: %d"),Hero->PreparedSpells.Num(),Hero->PreparedLimit(),Hero->ClassCharges,Hero->SpellSlots2,Hero->SelectedSpellLevel),800,187,.85f,AHUI::Gold,true);
        int32 Row=0;
        for(int32 I=0;I<AHSpells::Count();++I)
        {
            const auto& S=AHSpells::Get(static_cast<EAHSpell>(I)); if(!AHSpells::ForClass(S.Id,Hero->HeroClass)) continue;
            const float Y=226+Row++*55.f;
            const bool Available=Hero->IsSpellAvailable(S.Id),Prepared=S.Rank==0 || Hero->PreparedSpells.Contains(S.Id);
            const FName Name(*FString::Printf(TEXT("Spell_%d"),I));
            Panel(260,Y,1080,50,Available && (HoveredBox==Name || Hero->SelectedSpell==S.Id));
            Label(Available?(Prepared?TEXT("[+] "):TEXT("[ ] ")):TEXT("[NV 3]"),274,Y+8,.75f,Prepared?AHUI::Gold:AHUI::Dim);
            Label(S.Name,355,Y+6,.88f,Available?AHUI::Text:AHUI::Dim);
            Label(S.Description,355,Y+28,.68f,Available?AHUI::Text:AHUI::Dim);
            if(Available) AddHitBox(FVector2D(OffsetX+260*Scale,OffsetY+Y*Scale),FVector2D(1080*Scale,50*Scale),Name,true,10);
        }
        Label(Hero->Feedback,800,681,.75f,AHUI::Text,true);
        Label(TEXT("Buffs e curas: pessoais nesta arena. Magias de bonus limitam outras magias no turno."),800,703,.73f,AHUI::Dim,true);
        Button(TEXT("ReadySpells"),TEXT(""),Hero->bPreparingSpells?TEXT("PRONTO"):TEXT("VOLTAR"),755,731,true);
        if(Hero->MaxSpellSlots(2)>0) Button(TEXT("Slot"),TEXT("I / II"),TEXT("CIRCULO"),860,731,true,.65f);
        return;
    }
    if(Hero->bCharacterReady)
    {
        Label(FString::Printf(TEXT("NIVEL %d | XP %d / 2700"),Hero->Level,Hero->Experience),35,195,.85f,AHUI::Gold);
        if(Hero->MaxSpellSlots(1)>0)
            Label(FString::Printf(TEXT("Espacos I: %d/%d II: %d/%d | circulo: %d"),Hero->ClassCharges,Hero->MaxSpellSlots(1),Hero->SpellSlots2,Hero->MaxSpellSlots(2),Hero->SelectedSpellLevel),35,220,.75f,AHUI::Text);
        if(AHRules::Class(Hero->HeroClass).bCaster) Button(TEXT("Spells"),TEXT("K"),TEXT("MAGIAS"),215,250,true,.65f);
        if(Hero->Level>=2 && Hero->HeroClass!=EAHHeroClass::Cleric && Hero->HeroClass!=EAHHeroClass::Wizard) Button(TEXT("Feature"),TEXT(""),Hero->ProgressionAbilityName(),35,250,Ready,.65f);
        if(Hero->MaxSpellSlots(2)>0) Button(TEXT("Slot"),TEXT(""),TEXT("CIRCULO"),315,250,Ready,.65f);
        if(Hero->HeroClass==EAHHeroClass::Sorcerer)
        {
            Label(FString::Printf(TEXT("Feiticaria: %d / %d | Potencializar: %s"),Hero->SorceryPoints,Hero->Level>=2?Hero->Level:0,Hero->bEmpowerNext?TEXT("SIM"):TEXT("NAO")),35,435,.7f,AHUI::Gold);
            if(Hero->Level>=3) Button(TEXT("Empower"),TEXT("1 PF"),TEXT("POTENCIA"),125,250,Ready,.65f);
        }
        if(Hero->HeroClass==EAHHeroClass::Paladin)
        {
            Button(TEXT("Utility"),FString::FromInt(Hero->LayOnHands),TEXT("CURAR"),125,250,Ready);
            Label(Hero->bSmiteArmed?TEXT("Punicao armada: espaco I no acerto"):TEXT("Punicao desativada"),35,435,.7f,AHUI::Gold);
        }
        if(Hero->HeroClass==EAHHeroClass::Rogue && Hero->Level>=3) Button(TEXT("Aim"),TEXT("BONUS"),TEXT("MIRA"),125,250,Ready);
        if(Hero->Goodberries>0) Button(TEXT("Berry"),FString::FromInt(Hero->Goodberries),TEXT("FRUTO"),405,250,Ready);
        if(!Hero->RacialAbilityName().IsEmpty())
            Button(TEXT("Breath"),TEXT("T"),Hero->RacialAbilityName(),35,336,Hero->CanUseRacialAbility(),.65f);
        if(Hero->MaxSpellSlots(1)>0) Label(Hero->ClassAbilityName(),35,465,.75f,AHUI::Gold);
        if(Hero->GuardTurns>0) Label(FString::Printf(TEXT("Escudo +2 CA: %d turnos"),Hero->GuardTurns),35,340,.75f,AHUI::Gold);
    }

    // ── Detect events ────────────────────────────────────────────────────────
    // Turn banner
    bool bAnyTurnStart=false, bHeroTurnStart=false;
    if(Mode&&Mode->bStarted)
        for(const auto& P:Mode->Order)
            if(P.Get()&&P->bTurnActive&&!bPrevTurnActive)
            { bAnyTurnStart=true; bHeroTurnStart=!P->bEnemy; break; }
    if(bAnyTurnStart){ TurnBannerTime=Now; bBannerHeroTurn=bHeroTurnStart; }
    bPrevTurnActive=Hero->bTurnActive;

    // Round banner
    if(Mode&&Mode->Round!=PrevRound&&Mode->bStarted){ RoundBannerTime=Now; PrevRound=Mode->Round; }

    // Crit flash
    if(Hero->bLastImpactCritical&&!Hero->bEnemy)
    { const float ImpAge=Now-Hero->ImpactTextTime; if(ImpAge>=0.f&&ImpAge<.1f) CritFlashTime=Now; }

    // Damage vignette (drawn first — bottom of stack)
    const float HitAge=Now-Hero->LastDamageTime;
    if(HitAge>=0.f&&HitAge<0.7f) DrawDamageVignette(HitAge);

    // ═══════════════════════════════════════════════════════════════════════
    // CLASS SELECTION
    // ═══════════════════════════════════════════════════════════════════════
    if(!Hero->bCharacterReady)
    {
        // The cards used to be a fixed row of four at X=225+I*288. Twelve
        // archetypes or seven ancestries would have drawn straight off the
        // right edge, so the layout is derived from the table size instead.
        struct FGrid
        {
            int32 Cols=1, Rows=1, Total=1;
            float W=268.f, H=336.f, StepX=288.f, StepY=356.f, TopY=268.f, Unit=1.f;
            FVector2D At(int32 Index) const
            {
                const int32 Row=Index/Cols, Col=Index%Cols;
                const int32 InRow=FMath::Min(Cols,Total-Row*Cols);
                const float RowWidth=InRow*StepX-(StepX-W);
                return FVector2D(800.f-RowWidth*.5f+Col*StepX, TopY+Row*StepY);
            }
            float Bottom() const { return TopY+Rows*StepY-(StepY-H); }
        };
        auto Layout=[](int32 Total,float AreaW,float AreaH,float TopY,float BaseW,float BaseH)
        {
            const float Gap=20.f;
            FGrid G; G.Total=FMath::Max(Total,1);
            G.Cols=G.Total<=8?FMath::Min(G.Total,4):6;
            G.Rows=FMath::DivideAndRoundUp(G.Total,G.Cols);
            G.W=FMath::Min(BaseW,(AreaW-Gap*(G.Cols-1))/G.Cols);
            G.H=FMath::Min(BaseH,(AreaH-Gap*(G.Rows-1))/G.Rows);
            G.StepX=G.W+Gap; G.StepY=G.H+Gap; G.TopY=TopY;
            G.Unit=G.W/BaseW;                     // scales text and inner offsets
            return G;
        };

        if(!Hero->bAncestrySelected)
        {
            Panel(200,140,1200,600,true);
            Label(TEXT("A S H E N   H O L L O W"),800,160,1.8f,AHUI::Bright,true,true);
            Label(TEXT("1 / 2  ·  ESCOLHA SUA ANCESTRALIDADE"),800,210,1.f,AHUI::Gold,true);
            Label(TEXT("Traços iniciais do protótipo · modelos compartilhados"),800,245,.82f,AHUI::Dim,true);
            const int32 Total=AHRules::AncestryCount();
            const FGrid G=Layout(Total,1170.f,400.f,300.f,268.f,270.f);
            for(int32 I=0;I<Total;++I)
            {
                const FVector2D P=G.At(I);
                const auto Race=static_cast<EAHAncestry>(I);
                const FAHAncestrySheet& Blood=AHRules::Ancestry(Race);
                const FName Name(*FString::Printf(TEXT("Race%d"),I));
                Panel(P.X,P.Y,G.W,G.H,HoveredBox==Name);
                const float Mid=P.X+G.W*.5f;
                Label(AAHCharacter::AncestryName(Race),Mid,P.Y+G.H*.167f,1.15f*G.Unit,AHUI::Bright,true,true);
                Label(Blood.SelectionPassive,Mid,P.Y+G.H*.4f,.80f*G.Unit,AHUI::Text,true);
                const float Metres=Blood.Movement/100.f;
                Label(FMath::IsNearlyEqual(Metres,FMath::RoundToFloat(Metres))
                        ? FString::Printf(TEXT("Movimento: %.0f m"),Metres)
                        : FString::Printf(TEXT("Movimento: %.0f,%.0f m"),FMath::FloorToFloat(Metres),(Metres-FMath::FloorToFloat(Metres))*10.f),
                      Mid,P.Y+G.H*.526f,.82f*G.Unit,AHUI::Dim,true);
                Label(TEXT("ESCOLHER"),Mid,P.Y+G.H*.793f,1.f*G.Unit,AHUI::Gold,true);
                AddHitBox(FVector2D(OffsetX+P.X*Scale,OffsetY+P.Y*Scale),FVector2D(G.W*Scale,G.H*Scale),Name,true,2);
            }
            Label(FString::Printf(TEXT("Depois, escolha uma das %d classes."),AHRules::ClassCount()),
                  800,FMath::Min(G.Bottom()+24.f,712.f),.85f,AHUI::Dim,true);
            return;
        }

        Panel(200,140,1200,600,true);
        Label(TEXT("A S H E N   H O L L O W"),800,160,1.8f,AHUI::Bright,true,true);
        Label(TEXT("2 / 2  ·  ESCOLHA SUA CLASSE"),800,198,1.f,AHUI::Gold,true);
        Label(AAHCharacter::AncestryName(Hero->Ancestry)+TEXT("  ·  ")+AAHCharacter::AncestryTrait(Hero->Ancestry),800,230,.80f,AHUI::Dim,true);
        const FAHAncestrySheet& Blood=AHRules::Ancestry(Hero->Ancestry);
        const int32 Total=AHRules::ClassCount();
        const FGrid G=Layout(Total,1170.f,388.f,268.f,268.f,336.f);
        for(int32 I=0;I<Total;++I)
        {
            const FVector2D P=G.At(I);
            const FAHClassSheet& Sheet=AHRules::Class(static_cast<EAHHeroClass>(I));
            const FName Name(*FString::Printf(TEXT("Class%d"),I));
            const bool Hover=HoveredBox==Name;
            const float Mid=P.X+G.W*.5f;
            Panel(P.X,P.Y,G.W,G.H,Hover);
            if(Hover) DrawRect(FLinearColor(.22f,.14f,.05f,.15f),OffsetX+(P.X+2)*Scale,OffsetY+(P.Y+2)*Scale,(G.W-4)*Scale,(G.H-4)*Scale);
            const FLinearColor CC=Sheet.Colour;
            DrawRect(CC,OffsetX+(P.X+1)*Scale,OffsetY+P.Y*Scale,(G.W-2)*Scale,4*Scale);
            Icon(FName(Sheet.Icon),Mid,P.Y+G.H*.185f,AHUI::Bright);
            Label(Sheet.Name,Mid,P.Y+G.H*.339f,1.25f*G.Unit,AHUI::Bright,true,true);
            Label(FString::Printf(TEXT("%d PV · CA %d"),Sheet.MaxHealth+Blood.HealthPerLevel,Sheet.ArmorClass+Blood.ArmorBonus),
                  Mid,P.Y+G.H*.423f,.82f*G.Unit,AHUI::Text,true);
            DrawLine(OffsetX+(P.X+G.W*.075f)*Scale,OffsetY+(P.Y+G.H*.476f)*Scale,
                     OffsetX+(P.X+G.W*.925f)*Scale,OffsetY+(P.Y+G.H*.476f)*Scale,AHUI::Copper,.8f*Scale);
            Label(Sheet.SelectionAbility,Mid,P.Y+G.H*.506f,.88f*G.Unit,CC,true);
            Label(Sheet.SelectionDetail,Mid,P.Y+G.H*.583f,.78f*G.Unit,AHUI::Text,true);
            Label(Sheet.SelectionPassive,Mid,P.Y+G.H*.66f,.72f*G.Unit,AHUI::Dim,true);
            if(Sheet.RangedRange>0)
                Label(FString::Printf(TEXT("Alcance %.0f m"),Sheet.RangedRange/100.f),Mid,P.Y+G.H*.73f,.68f*G.Unit,AHUI::Teal,true);
            DrawRect(FLinearColor(.18f,.10f,.03f,.85f),OffsetX+(P.X+G.W*.127f)*Scale,OffsetY+(P.Y+G.H*.845f)*Scale,G.W*.746f*Scale,G.H*.101f*Scale);
            DrawLine(OffsetX+(P.X+G.W*.127f)*Scale,OffsetY+(P.Y+G.H*.845f)*Scale,
                     OffsetX+(P.X+G.W*.873f)*Scale,OffsetY+(P.Y+G.H*.845f)*Scale,CC,.9f*Scale);
            Label(TEXT("JOGAR"),Mid,P.Y+G.H*.863f,1.f*G.Unit,AHUI::Bright,true);
            AddHitBox(FVector2D(OffsetX+P.X*Scale,OffsetY+P.Y*Scale),FVector2D(G.W*Scale,G.H*Scale),Name,true,2);
        }
        Label(TEXT("VOLTAR: ANCESTRALIDADE"),800,FMath::Min(G.Bottom()+24.f,712.f),.88f,AHUI::Gold,true);
        AddHitBox(FVector2D(OffsetX+600*Scale,OffsetY+FMath::Min(G.Bottom()+8.f,696.f)*Scale),FVector2D(400*Scale,44*Scale),TEXT("BackAncestry"),true,2);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // IN-GAME HUD
    // ═══════════════════════════════════════════════════════════════════════

    // ── FPS ─────────────────────────────────────────────────────────────────
    SmoothedFrameMs=FMath::Lerp(SmoothedFrameMs,(float)(FApp::GetDeltaTime()*1000.0),.05f);
    const FLinearColor FPSCol=SmoothedFrameMs<20.f?AHUI::Green:SmoothedFrameMs<33.f?AHUI::Amber:AHUI::Red;
    Label(PC->PerformanceLabel,1405,26,.78f,AHUI::Dim,true);
    Label(FString::Printf(TEXT("%.0f FPS"),1000.f/FMath::Max(SmoothedFrameMs,.1f)),1405,45,.82f,FPSCol,true);
    // Compile stamp of this file. Cheapest possible answer to "did my build land?".
    static const FString BuildStamp=FString(ANSI_TO_TCHAR(__DATE__))+TEXT(" ")+FString(ANSI_TO_TCHAR(__TIME__));
    Label(BuildStamp,1405,64,.60f,AHUI::Dim,true);
    // The seed next to the build time. Two encounters showing the same number
    // means the layout really did repeat; two different numbers with the same
    // layout would mean the generator is broken. Either way it is answerable by
    // looking, instead of by rebuilding and hoping.
    const AAHGameMode* Arena=GetWorld()?GetWorld()->GetAuthGameMode<AAHGameMode>():nullptr;
    if(Arena)
        Label(FString::Printf(TEXT("%s  %d"),*Arena->Plan.Name,Arena->ArenaSeed),1405,80,.60f,AHUI::Dim,true);

    // ── Title ────────────────────────────────────────────────────────────────
    Label(TEXT("ASHEN  HOLLOW"),32,27,1.1f,AHUI::Bright,false,true);
    Label(TEXT("PÁTIO DOS JURAMENTOS"),33,50,.74f,AHUI::Dim);

    // ═══════════════════════════════════════════════════════════════════════
    // INITIATIVE STRIP
    // ═══════════════════════════════════════════════════════════════════════
    if(Mode&&Mode->bStarted)
    {
        const int32 Count=Mode->Order.Num();
        const float CardW=170.f,CardH=70.f,Gap=6.f;
        const float StartX=800.f-Count*(CardW+Gap)*.5f+Gap*.5f;
        Label(FString::Printf(TEXT("RODADA %d"),Mode->Round),800,14,.8f,AHUI::Dim,true);
        for(int32 I=0;I<Count;++I)
        {
            const auto* Actor=Mode->Order[I].Get(); if(!Actor) continue;
            const float X=StartX+I*(CardW+Gap);
            const bool  Active=Actor->bTurnActive;
            const bool  Dead=!Actor->IsAlive() && !Actor->IsDowned() && !Actor->bStabilized;
            const bool  Downed=Actor->IsDowned() || Actor->bStabilized;
            Panel(X,32,CardW,CardH,Active);
            if(Active){ const float Pulse=.5f+.5f*FMath::Sin(Now*4.f);
                DrawRect(FLinearColor(AHUI::Bright.R,AHUI::Bright.G,AHUI::Bright.B,.08f*Pulse),OffsetX+X*Scale,OffsetY+32*Scale,CardW*Scale,CardH*Scale); }
            const FLinearColor TC=Actor->bEnemy?AHUI::Red:AHUI::Teal;
            DrawRect(TC,OffsetX+X*Scale,OffsetY+32*Scale,3*Scale,CardH*Scale);
            Icon(Actor->bEnemy?TEXT("Attack"):TEXT("Dodge"),X+20,62,Dead?AHUI::Dim:TC);
            Label(Actor->bEnemy?Actor->EnemyName:AAHCharacter::ClassName(Actor->HeroClass),X+36,40,.82f,Dead?AHUI::Dim:AHUI::Text);
            if(Active && Actor->TurnStartTime > 0.f)
            {
                const float Elapsed=Now-Actor->TurnStartTime;
                const FLinearColor TC2=Elapsed<15.f?AHUI::Gold:Elapsed<30.f?AHUI::Amber:AHUI::Red;
                Label(FString::Printf(TEXT("INI %d  ·  %ds"),Actor->Initiative,FMath::FloorToInt(Elapsed)),X+36,57,.70f,TC2);
            }
            else
                Label(FString::Printf(TEXT("INI %d"),Actor->Initiative),X+36,57,.70f,Active?AHUI::Gold:AHUI::Dim);
            // Reaction dot — teal while this participant can still react
            if(!Dead)
                DrawFilledCircle(X+CardW-14,45,5.f,
                    Actor->Turn.bReaction?AHUI::Teal:FLinearColor(.04f,.04f,.04f,1.f),
                    Actor->Turn.bReaction?AHUI::Teal:AHUI::Dim,1.f);
            // Mini HP bar
            const float BPX=OffsetX+(X+5)*Scale,BPY=OffsetY+88*Scale,BPW=(CardW-10)*Scale,BPH=5*Scale;
            DrawRect(FLinearColor(.06f,.008f,.008f,1.f),BPX,BPY,BPW,BPH);
            const float Frac=Actor->MaxHealth>0?(float)Actor->Health/Actor->MaxHealth:0.f;
            DrawRect(Actor->bEnemy?FLinearColor::LerpUsingHSV(AHUI::Red,AHUI::Amber,Frac):FLinearColor::LerpUsingHSV(AHUI::Red,AHUI::Green,Frac),BPX,BPY,BPW*Frac,BPH);
            DrawRect(FLinearColor(1,1,1,.07f),BPX,BPY,BPW*Frac,BPH*.4f);
            Label(FString::Printf(TEXT("%d/%d"),Actor->Health,Actor->MaxHealth),X+CardW*.5f,78,.67f,AHUI::Dim,true);
            if(Downed && !Dead)
            {
                // Downed overlay: amber pulse, show death save counts
                DrawRect(FLinearColor(.25f,.12f,.00f,.35f),OffsetX+X*Scale,OffsetY+32*Scale,CardW*Scale,CardH*Scale);
                const float PX=OffsetX+(X+5)*Scale, PY2=OffsetY+93*Scale, Dot=4.f*Scale, PipGap=9.f*Scale;
                for(int32 S=0;S<3;++S) // successes green
                    DrawRect(S<Actor->DeathSuccesses?FLinearColor(AHUI::Green.R,AHUI::Green.G,AHUI::Green.B,1):FLinearColor(.04f,.04f,.04f,1.f),PX+S*PipGap,PY2,Dot,Dot);
                for(int32 F=0;F<3;++F) // failures red
                    DrawRect(F<Actor->DeathFailures?FLinearColor(AHUI::Red.R,AHUI::Red.G,AHUI::Red.B,1):FLinearColor(.04f,.04f,.04f,1.f),PX+(F+4)*PipGap,PY2,Dot,Dot);
            }
            if(Dead)
            {
                DrawRect(FLinearColor(0,0,0,.40f),OffsetX+X*Scale,OffsetY+32*Scale,CardW*Scale,CardH*Scale);
                const float CX2=X+CardW*.5f,CY2=32+CardH*.5f,R=17.f;
                DrawLine(OffsetX+(CX2-R)*Scale,OffsetY+(CY2-R)*Scale,OffsetX+(CX2+R)*Scale,OffsetY+(CY2+R)*Scale,AHUI::Red,2.f*Scale);
                DrawLine(OffsetX+(CX2+R)*Scale,OffsetY+(CY2-R)*Scale,OffsetX+(CX2-R)*Scale,OffsetY+(CY2+R)*Scale,AHUI::Red,2.f*Scale);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BOTTOM ACTION BAR
    // ═══════════════════════════════════════════════════════════════════════
    Panel(28,730,1218,142,Hero->bTurnActive);

    // ── HP Orb (portrait) ────────────────────────────────────────────────────
    Panel(36,738,118,126,Hero->bTurnActive);
    DrawHPOrb(95,790,44,Hero->Health,Hero->MaxHealth,false,Hero->TempHP);
    Label(AAHCharacter::ClassName(Hero->HeroClass),95,742,.92f,AHUI::ClassColor(Hero->HeroClass),true,true);
    if(Hero->bDowned || Hero->bStabilized)
    {
        // Show death save pips in the portrait area
        DrawDeathSaves(95, 835, Hero);
    }
    else
        Label(Hero->bDodging?TEXT("ESQUIVA"):Hero->IsAlive()?TEXT(""):TEXT("CAÍDO"),95,835,.72f,Hero->bDodging?AHUI::Teal:AHUI::Amber,true);

    // ── Stats block ──────────────────────────────────────────────────────────
    Panel(162,738,200,126);
    Label(TEXT("DEFESA"),263,742,.72f,AHUI::Dim,true);
    Label(FString::Printf(TEXT("CA  %d"),Hero->ArmorClass),263,762,1.f,AHUI::Text,true);
    DrawLine(OffsetX+172*Scale,OffsetY+780*Scale,OffsetX+352*Scale,OffsetY+780*Scale,AHUI::Copper,.7f*Scale);
    Label(TEXT("ATAQUES"),263,788,.72f,AHUI::Dim,true);
    Label(FString::Printf(TEXT("+%d  /  1d%d+%d"),Hero->AttackBonus,Hero->DamageSides,Hero->DamageModifier+(Hero->bRaging?2:0)),263,808,.82f,AHUI::Text,true);
    DrawLine(OffsetX+172*Scale,OffsetY+826*Scale,OffsetX+352*Scale,OffsetY+826*Scale,AHUI::Copper,.7f*Scale);
    // Charge orbs
    Label(TEXT("USOS"),263,835,.68f,AHUI::Dim,true);
    DrawChargeOrbs(Hero,263,847);

    // ── Action economy ───────────────────────────────────────────────────────
    Panel(370,738,210,126);
    Label(Hero->bTurnActive?TEXT("SEU TURNO"):TEXT("AGUARDE"),475,742,.88f,Hero->bTurnActive?AHUI::Bright:AHUI::Dim,true,true);
    DrawLine(OffsetX+378*Scale,OffsetY+760*Scale,OffsetX+572*Scale,OffsetY+760*Scale,AHUI::Copper,.7f*Scale);
    // Action pip
    const FLinearColor ActF=Hero->Turn.bAction?AHUI::Green:FLinearColor(.05f,.05f,.05f,1.f);
    DrawFilledCircle(429,774,12,ActF,Hero->Turn.bAction?AHUI::Green:AHUI::Dim,1.5f);
    Label(TEXT("AÇÃO"),429,792,.64f,Hero->Turn.bAction?AHUI::Green:AHUI::Dim,true);
    // Bonus pip
    const FLinearColor BonF=Hero->Turn.bBonus?AHUI::Amber:FLinearColor(.05f,.05f,.05f,1.f);
    DrawFilledCircle(475,774,10,BonF,Hero->Turn.bBonus?AHUI::Amber:AHUI::Dim,1.5f);
    Label(TEXT("BÔNUS"),475,792,.64f,Hero->Turn.bBonus?AHUI::Amber:AHUI::Dim,true);
    // Reaction pip — spent by opportunity attacks, renewed on your own turn
    const FLinearColor ReaF=Hero->Turn.bReaction?AHUI::Teal:FLinearColor(.05f,.05f,.05f,1.f);
    DrawFilledCircle(521,774,10,ReaF,Hero->Turn.bReaction?AHUI::Teal:AHUI::Dim,1.5f);
    Label(TEXT("REAÇÃO"),521,792,.64f,Hero->Turn.bReaction?AHUI::Teal:AHUI::Dim,true);
    DrawLine(OffsetX+378*Scale,OffsetY+806*Scale,OffsetX+572*Scale,OffsetY+806*Scale,AHUI::Copper,.7f*Scale);
    // Movement dots
    Label(TEXT("MOVIMENTO"),475,814,.70f,AHUI::Dim,true);
    const float MovFrac=FMath::Clamp(Hero->Turn.Movement/Hero->BaseMovement,0.f,1.f);
    for(int32 D=0;D<9;++D)
    {
        const bool bFill=(float)D/9.f<MovFrac;
        DrawFilledCircle(430+D*14.f,830,5,bFill?AHUI::Teal:FLinearColor(.04f,.04f,.04f,1.f),bFill?AHUI::Teal:AHUI::Dim,1.f);
    }
    Label(FString::Printf(TEXT("%.0f m"),Hero->Turn.Movement/100.f),475,844,.70f,AHUI::Dim,true);

    // ── Action buttons ───────────────────────────────────────────────────────
    Button(TEXT("Attack"),   TEXT("Q"),  TEXT("ATAQUE"),     588,745,Ready&&Hero->Turn.bAction);
    Button(TEXT("Dodge"),    TEXT("Spc"),TEXT("ESQUIVA"),    678,745,Ready&&Hero->Turn.bAction);
    Button(TEXT("Disengage"),TEXT("X"),  TEXT("DESENGAJAR"), 768,745,Ready&&Hero->Turn.bAction&&!Hero->bDisengaging,.62f);
    Button(TEXT("Heal"),     TEXT("E"),  Hero->MaxSpellSlots(1)>0?TEXT("CONJURAR"):Hero->ClassAbilityName(),858,745,Hero->CanUseClassAbility());
    Button(TEXT("Dash"),     TEXT("R"),  TEXT("DISPARADA"),  948,745,Ready&&Hero->Turn.bAction);

    // ── End Turn ─────────────────────────────────────────────────────────────
    const bool ETHover=HoveredBox==TEXT("EndTurn");
    Panel(1038,745,196,78,ETHover&&Ready);
    if(ETHover) DrawRect(FLinearColor(.22f,.14f,.05f,.25f),OffsetX+1040*Scale,OffsetY+747*Scale,192*Scale,74*Scale);
    if(Ready){ // Subtle pulse on border when it's time
        const float Pulse=.5f+.5f*FMath::Sin(Now*2.f);
        DrawRect(FLinearColor(AHUI::Bright.R,AHUI::Bright.G,AHUI::Bright.B,.06f*Pulse),OffsetX+1038*Scale,OffsetY+745*Scale,196*Scale,78*Scale);
    }
    Label(TEXT("ENCERRAR"),1136,763,1.f,Ready?AHUI::Bright:AHUI::Dim,true,true);
    Label(TEXT("TURNO"),   1136,785,1.f,Ready?AHUI::Bright:AHUI::Dim,true,true);
    Label(TEXT("↵ ENTER"), 1136,814,.78f,AHUI::Dim,true);
    AddHitBox(FVector2D(OffsetX+1038*Scale,OffsetY+745*Scale),FVector2D(196*Scale,78*Scale),TEXT("EndTurn"),true,1);

    // ── Tooltip ──────────────────────────────────────────────────────────────
    FString Tooltip=Hero->Feedback;
    if(HoveredBox==TEXT("Attack"))
    {
        const FAHClassSheet& MySheet=AHRules::Class(Hero->HeroClass);
        Tooltip=Hero->HasRangedAttack()
            ? FString::Printf(TEXT("%s · 1 ação · +%d p/ acertar · 1d%d · alcance %.0f m%s"),
                MySheet.RangedName,Hero->AttackBonus,MySheet.RangedSides,MySheet.RangedRange/100.f,
                Hero->IsThreatenedInMelee()?TEXT(" · DESVANTAGEM: inimigo em corpo a corpo"):TEXT(""))
            : FString::Printf(TEXT("Ataque · 1 ação · +%d p/ acertar · 1d%d+%d de dano"),
                Hero->AttackBonus,Hero->DamageSides,Hero->DamageModifier+(Hero->bRaging?2:0));
    }
    if(HoveredBox==TEXT("Dodge"))   Tooltip=TEXT("Esquiva · 1 ação · desvantagem nos ataques recebidos até seu próximo turno");
    if(HoveredBox==TEXT("Heal"))    Tooltip=Hero->ClassAbilityDescription();
    if(HoveredBox==TEXT("Dash"))    Tooltip=FString::Printf(TEXT("Disparada · 1 acao · +%.1f m neste turno"),Hero->BaseMovement/100.f);
    if(HoveredBox==TEXT("Disengage")) Tooltip=Hero->bDisengaging
        ? TEXT("Você já desengajou: seu movimento não provoca reações até o fim deste turno")
        : TEXT("Desengajar · 1 ação · seu movimento não provoca ataques de oportunidade neste turno");

    if(HoveredBox==TEXT("EndTurn")) Tooltip=TEXT("Encerra seu turno. Ação, bônus e movimento renovam no próximo.");
    Label(Tooltip,800,720,.86f,AHUI::Text,true);
    Label(TEXT("Botão direito: mover / alvo    A / D: girar câmera    Roda: zoom    C: analisar    F5: reiniciar    Sair do alcance provoca ataque de oportunidade"),800,882,.66f,AHUI::Dim,true);

    // ═══════════════════════════════════════════════════════════════════════
    // COMBAT LOG
    // ═══════════════════════════════════════════════════════════════════════
    Panel(1254,738,318,130);
    Label(TEXT("REGISTRO DE COMBATE"),1413,749,.78f,AHUI::Gold,true,true);
    DrawLine(OffsetX+1264*Scale,OffsetY+763*Scale,OffsetX+1562*Scale,OffsetY+763*Scale,AHUI::Copper,.7f*Scale);
    for(int32 I=0;I<Hero->CombatLog.Num();++I)
    {
        const FString& Entry=Hero->CombatLog[I];
        FLinearColor LC=AHUI::Dim;
        if(I==Hero->CombatLog.Num()-1) LC=AHUI::Text;
        if(Entry.Contains(TEXT("CRITICO"))||Entry.Contains(TEXT("CRÍTICO"))||Entry.Contains(TEXT("ACERTO"))) LC=AHUI::Amber;
        if(Entry.Contains(TEXT("cura"))||Entry.Contains(TEXT("Oportunidade"))) LC=Entry.Contains(TEXT("cura"))?AHUI::Green:AHUI::Purple;
        if(Entry.Contains(TEXT("ERRO"))||Entry.Contains(TEXT("falha"))) LC=AHUI::Dim;
        Label(Entry,1264,768+I*18,.67f,LC);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // WORLD-SPACE OVERLAYS
    // ═══════════════════════════════════════════════════════════════════════
    SelectRingAngle=Now*.5f;   // ring slowly rotates
    DrawMovementRing(Hero,PC,Now);

    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
    {
        const AAHCharacter* Actor=*It;
        if(Actor->IsAlive() || Actor->IsDowned() || Actor->bStabilized)
        {
            const FVector Feet=Actor->GetActorLocation()-FVector(0,0,91);
            const FLinearColor TC=Actor->bEnemy?AHUI::Amber:AHUI::Teal;
            const float Thick=Actor->bTurnActive?2.5f*Scale:Scale;
            for(int32 I=0;I<48;++I)
            {
                // Dashed rotating ring
                if(I%3==2) continue;
                const float A=I*2*PI/48+SelectRingAngle;
                const float B=(I+1)*2*PI/48+SelectRingAngle;
                FVector2D PA,PB;
                if(PC->ProjectWorldLocationToScreen(Feet+FVector(FMath::Cos(A)*62,FMath::Sin(A)*62,0),PA)&&
                   PC->ProjectWorldLocationToScreen(Feet+FVector(FMath::Cos(B)*62,FMath::Sin(B)*62,0),PB))
                    if(PA.Y>OffsetY+120*Scale&&PB.Y>OffsetY+120*Scale&&PA.Y<OffsetY+700*Scale&&PB.Y<OffsetY+700*Scale)
                        DrawLine(PA.X,PA.Y,PB.X,PB.Y,TC,Thick);
            }
        }

        // Floating numbers / labels — always continue for downed/stabilized chars
        if(!Actor->bDowned && !Actor->bStabilized)
            if(!It->bEnemy&&Now-It->ImpactTextTime>1.6f&&Now-It->OpportunityFlashTime>1.4f&&!It->bIsAttacking) continue;
        FVector2D Pos; if(!PC->ProjectWorldLocationToScreen(Actor->GetActorLocation()+FVector(0,0,170),Pos)) continue;
        const float SX=(Pos.X-OffsetX)/Scale, SY=(Pos.Y-OffsetY)/Scale;
        if(SY<120||SY>680||SX<30||SX>1220) continue;

        if(Actor->bEnemy&&Actor->IsAlive())
        {
            DrawRect(FLinearColor(0,0,0,.55f),Pos.X-66*Scale,Pos.Y-4*Scale,132*Scale,16*Scale);
            Label(Actor->EnemyName,SX,SY-2,.84f,AHUI::Text,true);
            // Enemy HP bar
            const float EBX=Pos.X-58*Scale, EBY=Pos.Y+16*Scale, EBW=116*Scale, EBH=5*Scale;
            DrawRect(FLinearColor(.06f,.008f,.008f,1.f),EBX,EBY,EBW,EBH);
            const float EFrac=Actor->MaxHealth>0?(float)Actor->Health/Actor->MaxHealth:0.f;
            DrawRect(FLinearColor::LerpUsingHSV(AHUI::Red,AHUI::Amber,EFrac),EBX,EBY,EBW*EFrac,EBH);
            DrawRect(FLinearColor(1,1,1,.07f),EBX,EBY,EBW*EFrac,EBH*.4f);
        }

        // Status icons (skip if downed — death saves shown instead)
        if(!Actor->bDowned && !Actor->bStabilized)
            DrawStatusIcons(SX, SY-46, Actor);
        else
            DrawDeathSaves(SX, SY-30, Actor);

        // Reaction call-out
        const float OppAge=Now-Actor->OpportunityFlashTime;
        if(OppAge>=0.f&&OppAge<1.4f)
        {
            FLinearColor OC=AHUI::Bright; OC.A=1.f-FMath::Clamp((OppAge-.7f)/.7f,0.f,1.f);
            Label(TEXT("REAÇÃO!"),SX,SY-66,1.f,OC,true,true);
        }

        // Floating damage / healing
        const float ImpAge=Now-Actor->ImpactTextTime;
        if(ImpAge>=0.f&&ImpAge<1.6f&&!Actor->ImpactText.IsEmpty())
        {
            const float ScaleAnim=Actor->bLastImpactCritical
                ? FMath::Lerp(2.2f,1.65f,FMath::Clamp(ImpAge/.3f,0.f,1.f))
                : 1.4f;
            FLinearColor IC=Actor->bImpactHealing?AHUI::Teal:Actor->bLastImpactCritical?AHUI::Bright:AHUI::Amber;
            IC.A=1.f-FMath::Clamp((ImpAge-1.f)/.6f,0.f,1.f);
            Label(Actor->ImpactText,SX,SY-30-ImpAge*28,ScaleAnim,IC,true,Actor->bLastImpactCritical);
        }
        else if(Actor->bIsAttacking)
            Label(Actor->MotionLabel,SX,SY-28,.85f,AHUI::Gold,true);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // TARGET INFO
    // ═══════════════════════════════════════════════════════════════════════
    if(PC->HoveredEnemy&&PC->HoveredEnemy->IsAlive())
    {
        DrawTargetBrackets(PC->HoveredEnemy,PC,Now);
        AAHCharacter* Foe=PC->HoveredEnemy.Get();

        // The number on screen has to be the number the dice will use. Cover,
        // high ground and the crowded-shot penalty all move it, and a modifier
        // the player cannot see reads as the game cheating -- which is exactly
        // how advantage and halfling luck read before they were surfaced.
        const EAHCover Shielded=Foe->CoverFrom(Hero);
        const int32 TargetAC=Foe->ArmorClass+AHArena::ArmorBonus(Shielded);
        const bool bHigh=Hero->HasHighGroundOn(Foe);
        const bool bFavoured=bHigh||Hero->bSteadyAim||Hero->bReckless||Foe->bReckless||Foe->HasGuidingMark();
        const bool bHindered=Foe->bDodging||(Hero->HasRangedAttack()&&Hero->IsThreatenedInMelee());
        const float Base=FMath::Clamp(21+Hero->AttackBonus-TargetAC,1,19)*.05f;
        const float Odds=bFavoured==bHindered?Base:bFavoured?1.f-(1.f-Base)*(1.f-Base):Base*Base;
        const int Chance=FMath::RoundToInt(100*Odds);
        Panel(640,130,320,90);
        DrawRect(FLinearColor(AHUI::Red.R,AHUI::Red.G,AHUI::Red.B,.12f),OffsetX+641*Scale,OffsetY+131*Scale,318*Scale,88*Scale);
        Label(PC->HoveredEnemy->EnemyName,800,140,1.f,AHUI::Bright,true,true);
        const float TBX=OffsetX+648*Scale,TBY=OffsetY+160*Scale,TBW=292*Scale,TBH=8*Scale;
        DrawRect(FLinearColor(.06f,.008f,.008f,1.f),TBX,TBY,TBW,TBH);
        const float TF=PC->HoveredEnemy->MaxHealth>0?(float)PC->HoveredEnemy->Health/PC->HoveredEnemy->MaxHealth:0.f;
        DrawRect(FLinearColor::LerpUsingHSV(AHUI::Red,AHUI::Amber,TF),TBX,TBY,TBW*TF,TBH);
        DrawRect(FLinearColor(1,1,1,.07f),TBX,TBY,TBW*TF,TBH*.4f);
        const FLinearColor CC=Chance>=60?AHUI::Green:Chance>=35?AHUI::Amber:AHUI::Red;
        Label(Shielded==EAHCover::None
            ? FString::Printf(TEXT("%d%% de acerto  ·  CA %d"),Chance,TargetAC)
            : FString::Printf(TEXT("%d%% de acerto  ·  CA %d (%d +%d)"),Chance,TargetAC,
                              Foe->ArmorClass,AHArena::ArmorBonus(Shielded)),
            800,176,.86f,CC,true);

        // Why it is that number, in words, under the percentage.
        FString Reading;
        if(Shielded!=EAHCover::None) Reading+=AHArena::CoverName(Shielded);
        if(bHigh) { if(!Reading.IsEmpty()) Reading+=TEXT("  ·  "); Reading+=TEXT("TERRENO ELEVADO: VANTAGEM"); }
        if(bHindered) { if(!Reading.IsEmpty()) Reading+=TEXT("  ·  "); Reading+=TEXT("DESVANTAGEM"); }
        if(!Reading.IsEmpty())
            Label(Reading,800,196,.72f,Shielded!=EAHCover::None?AHUI::Amber:AHUI::Teal,true);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // DICE ROLL PANEL
    // ═══════════════════════════════════════════════════════════════════════
    const float RollAge=Now-Hero->LastRollTime;
    if(Hero->LastRollTime>=0&&RollAge<8.f)
    {
        const auto& Roll=Hero->LastRoll;
        Panel(1262,188,302,298);
        Label(Hero->LastRollLabel,1413,204,.78f,AHUI::Gold,true,true);
        DrawLine(OffsetX+1272*Scale,OffsetY+220*Scale,OffsetX+1554*Scale,OffsetY+220*Scale,AHUI::Copper,.7f*Scale);
        // Advantage, disadvantage and the halfling's luck all resolve inside the
        // d20 roll. Without showing the discarded die the player sees one number
        // and cannot tell any of them ever happened.
        if(Roll.Advantage!=0||Roll.bLuckyReroll)
        {
            FString Tag=Roll.Advantage>0?TEXT("VANTAGEM"):Roll.Advantage<0?TEXT("DESVANTAGEM"):FString();
            if(Roll.bLuckyReroll) Tag=Tag.IsEmpty()?FString(TEXT("SORTE")):Tag+TEXT("  ·  SORTE");
            Label(Tag,1413,226,.80f,
                Roll.Advantage>0?AHUI::Green:Roll.Advantage<0?AHUI::Red:AHUI::Bright,true,true);
        }
        const float T=FMath::Clamp(RollAge/.7f,0.f,1.f);
        DrawDie(1413,318,78,30+300*(1-FMath::Pow(1-T,3)),AHUI::Bright);
        Label(FString::FromInt(Roll.NaturalRoll),1413,297,3.f,AHUI::White,true,true);
        if(Roll.DiscardedRoll>0)
        {
            // The die that was thrown away, struck through.
            DrawFilledCircle(1519,258,21,FLinearColor(.05f,.04f,.03f,1.f),AHUI::Dim,1.2f);
            Label(FString::FromInt(Roll.DiscardedRoll),1519,249,1.05f,AHUI::Dim,true,true);
            DrawLine(OffsetX+1503*Scale,OffsetY+274*Scale,
                     OffsetX+1535*Scale,OffsetY+242*Scale,AHUI::Red,1.8f*Scale);
            Label(TEXT("descartado"),1519,282,.55f,AHUI::Dim,true);
        }
        else if(Roll.bLuckyReroll)
        {
            DrawFilledCircle(1519,258,21,FLinearColor(.05f,.04f,.03f,1.f),AHUI::Dim,1.2f);
            Label(TEXT("1"),1519,249,1.05f,AHUI::Dim,true,true);
            DrawLine(OffsetX+1503*Scale,OffsetY+274*Scale,
                     OffsetX+1535*Scale,OffsetY+242*Scale,AHUI::Red,1.8f*Scale);
            Label(TEXT("rerrolado"),1519,282,.55f,AHUI::Dim,true);
        }
        DrawLine(OffsetX+1272*Scale,OffsetY+403*Scale,OffsetX+1554*Scale,OffsetY+403*Scale,AHUI::Copper,.7f*Scale);
        Label(FString::Printf(TEXT("%d + %d = %d"),Roll.NaturalRoll,Roll.Modifier,Roll.Total),1413,411,1.3f,AHUI::Text,true);
        const FLinearColor RC=Roll.bSuccess?AHUI::Green:AHUI::Red;
        Label(Roll.bCritical?TEXT("C R Í T I C O !"):Roll.bSuccess?TEXT("A C E R T O"):TEXT("F A L H A"),1413,434,1.1f,RC,true,true);
        Label(FString::Printf(TEXT("Alvo %d  ·  %d de dano"),Roll.Target,Roll.Damage),1413,455,.8f,AHUI::Dim,true);
        DrawRect(RC*.28f,OffsetX+1282*Scale,OffsetY+468*Scale,262*Scale,22*Scale);
        DrawLine(OffsetX+1282*Scale,OffsetY+468*Scale,OffsetX+1544*Scale,OffsetY+468*Scale,RC,.9f*Scale);
        Label(Roll.bSuccess?FString::Printf(TEXT("-%d PV"),Roll.Damage):TEXT("Sem dano"),1413,471,.82f,AHUI::White,true);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // OVERLAY FX (always on top)
    // ═══════════════════════════════════════════════════════════════════════
    if(CritFlashTime>0.f) DrawCritFlash(Now-CritFlashTime);
    if(RoundBannerTime>0.f) DrawRoundBanner(Now-RoundBannerTime,Mode?Mode->Round:1);
    if(TurnBannerTime>0.f) DrawTurnBanner(Now-TurnBannerTime,bBannerHeroTurn);

    // ═══════════════════════════════════════════════════════════════════════
    // VICTORY / DEFEAT
    // ═══════════════════════════════════════════════════════════════════════
    if(Mode&&Mode->bFinished)
    {
        bool bEnemyAlive=false;
        for(const auto& C:Mode->Order) if(C.Get()&&C->bEnemy&&C->IsAlive()) bEnemyAlive=true;
        const bool Win=!bEnemyAlive;
        DrawRect(FLinearColor(0,0,0,.65f),OffsetX,OffsetY,1600*Scale,900*Scale);
        Panel(530,210,540,340,true);
        DrawRect((Win?AHUI::Teal:AHUI::Red)*.4f,OffsetX+531*Scale,OffsetY+211*Scale,538*Scale,108*Scale);
        Label(Win?TEXT("V I T Ó R I A"):Hero->bStabilized?TEXT("ESTABILIZADO"):TEXT("D E R R O T A"),800,228,2.f,AHUI::Bright,true,true);
        Label(Win?TEXT("O inimigo foi derrotado."):Hero->bStabilized?TEXT("Voce sobreviveu. Encontro encerrado."):TEXT("Você foi derrotado."),800,270,.95f,AHUI::Text,true);
        Label(FString::Printf(TEXT("Nivel %d | XP %d / 2700 | vitoria +300 XP"),Hero->Level,Hero->Experience),800,300,.85f,AHUI::Gold,true);
        if(Hero->Level==4 && Hero->Feat==0)
        {
            Label(TEXT("Escolha um talento permanente"),800,340,.9f,AHUI::Text,true);
            Button(TEXT("Feat1"),TEXT("+8 PV"),TEXT("ROBUSTO"),645,380,true,.65f);
            Button(TEXT("Feat2"),TEXT("+5 INIT"),TEXT("ALERTA"),755,380,true,.65f);
            Button(TEXT("Feat3"),TEXT("+3 m"),TEXT("MOVEL"),865,380,true,.65f);
        }
        else if(Hero->IsAlive() || Hero->bStabilized)
        {
            Label(TEXT("Descansar e iniciar o proximo combate"),800,350,.9f,AHUI::Text,true);
            Button(TEXT("Next"),TEXT(""),TEXT("SEGUIR"),759,385,true);
        }
        Label(TEXT("F5: nova jornada (reinicia XP)"),800,510,.8f,AHUI::Dim,true);
    }
}
