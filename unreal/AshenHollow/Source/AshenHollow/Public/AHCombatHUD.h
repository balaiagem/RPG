#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AHCombatHUD.generated.h"
UCLASS()
class ASHENHOLLOW_API AAHCombatHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
    virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;
    bool IsPointerOverInterface() const;
private:
    FName HoveredBox;
    float Scale=1.f;
    float SmoothedFrameMs=16.67f;
    float OffsetX=0.f, OffsetY=0.f;
    void Panel(float X,float Y,float W,float H,bool Active=false);
    void Label(const FString& Text,float X,float Y,float Size,FLinearColor Color,bool Center=false);
    void Icon(FName Type,float X,float Y,FLinearColor Color);
    void Button(FName Name,const FString& Key,const FString& Title,float X,float Y,bool Enabled);
    void DrawDie(float X, float Y, float Radius, float Angle, FLinearColor Color);
};
