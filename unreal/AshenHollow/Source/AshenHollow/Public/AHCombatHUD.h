#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AHCombatHUD.generated.h"

UCLASS()
class ASHENHOLLOW_API AAHCombatHUD : public AHUD
{
    GENERATED_BODY()
public:
    AAHCombatHUD();
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
    virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;
    bool IsPointerOverInterface() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Font")
    TObjectPtr<UFont> CinzelFont;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Font")
    TObjectPtr<UFont> CinzelBoldFont;

private:
    // ── Layout ───────────────────────────────────────────────────────────────
    FName  HoveredBox;
    float  Scale           = 1.f;
    float  SmoothedFrameMs = 16.67f;
    float  OffsetX         = 0.f;
    float  OffsetY         = 0.f;

    // ── Animation state ──────────────────────────────────────────────────────
    float  TurnBannerTime   = -100.f;
    bool   bBannerHeroTurn  = true;
    bool   bPrevTurnActive  = false;
    float  CritFlashTime    = -100.f;
    int32  PrevRound        = -1;
    float  RoundBannerTime  = -100.f;
    float  SelectRingAngle  = 0.f;   // rotates over time

    // ── Font ─────────────────────────────────────────────────────────────────
    UFont* F(bool bBold = false) const;

    // ── Primitives ───────────────────────────────────────────────────────────
    void Panel(float X, float Y, float W, float H, bool Active = false);
    void Label(const FString& Text, float X, float Y, float Size,
               FLinearColor Color, bool Center = false, bool bBold = false);
    void Icon(FName Type, float X, float Y, FLinearColor Color);
    void Button(FName Name, const FString& Key, const FString& Title,
                float X, float Y, bool Enabled, float TitleSize = .78f);
    void DrawDie(float X, float Y, float Radius, float Angle, FLinearColor Color);
    void DrawFilledCircle(float X, float Y, float R,
                          FLinearColor Fill, FLinearColor Border, float Thickness = 1.5f);

    // ── BG3 features ─────────────────────────────────────────────────────────
    /** Circular portrait HP orb with arc drain + optional TempHP white ring */
    void DrawHPOrb(float CX, float CY, float R, int32 HP, int32 MaxHP, bool bEnemy, int32 TempHP = 0);
    /** Three-heart / three-skull death save tracker above a downed character */
    void DrawDeathSaves(float SX, float SY, const class AAHCharacter* Actor);
    /** Row of ability-charge pips (spell slots / rage / second-wind) */
    void DrawChargeOrbs(const class AAHCharacter* Hero, float X, float Y);
    /** Small status icons above a character's head (in world-projected space) */
    void DrawStatusIcons(float ScreenX, float ScreenY, const class AAHCharacter* Actor);
    /** Corner bracket reticle around hovered enemy */
    void DrawTargetBrackets(const class AAHCharacter* Target,
                            const class AAHPlayerController* PC, float Now);
    /** Ground dashed ring showing remaining movement radius */
    void DrawMovementRing(const class AAHCharacter* Hero,
                          const class AAHPlayerController* PC, float Now);
    /** Red vignette flash on taking damage */
    void DrawDamageVignette(float HitAge);
    /** Gold/white full-screen flash on critical hit */
    void DrawCritFlash(float FlashAge);
    /** "SEU TURNO" / "TURNO INIMIGO" animated slide-in banner */
    void DrawTurnBanner(float BannerAge, bool bHero);
    /** "RODADA X" centre banner when the round number increases */
    void DrawRoundBanner(float BannerAge, int32 Round);
};
