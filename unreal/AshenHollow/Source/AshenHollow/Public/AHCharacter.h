#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AHDiceRules.h"
#include "AHAbilities.h"
#include "AHItems.h"
#include "AHTurnBudget.h"
#include "AHClassData.h"
#include "AHSpellData.h"
#include "AHArena.h"
#include "AHCharacter.generated.h"

class UAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UAnimationAsset;
class UAnimInstance;
class UNiagaraSystem;
class AAHMagicVisual;
class UAHEquipmentComponent;
class UAHLifeAudioComponent;

/** Shared character for the turn-based SRD combat encounter. */
UCLASS(Blueprintable)
class ASHENHOLLOW_API AAHCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AAHCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;

    // ── Core stats ────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly) bool bEnemy = false;
    UPROPERTY(BlueprintReadOnly) int32 Health = 12;
    UPROPERTY(BlueprintReadOnly) int32 MaxHealth = 12;
    UPROPERTY(BlueprintReadOnly) int32 ArmorClass = 16;
    UPROPERTY(BlueprintReadOnly) FAHDiceOutcome LastRoll;
    /** Shown by the HUD for a foe; set from its rolled archetype in BecomeEnemy. */
    FString EnemyName = TEXT("THORNBOUND");
    int32 Gold=0;
    bool bLootDropped=false;
    bool DropBackpackOnSlot(int32 Index, EAHSlot Slot);
    UPROPERTY(BlueprintReadOnly) FString LastRollLabel;
    float LastRollTime = -100.f;
    FAHTurnBudget Turn;
    bool bTurnActive = false;
    /**
     * Wandering the world rather than taking a turn. Set by the game mode each
     * frame while exploring; it buys a brisker walk, because a bigger world that
     * takes longer to cross is just a slower world.
     */
    bool bRoaming = false;
    bool bDodging = false;
    bool bDashing = false;
    /** True after the Disengage action: this turn's movement provokes no reactions. */
    bool bDisengaging = false;
    /** World-time of the last opportunity attack this character made (HUD flash). */
    float OpportunityFlashTime = -100.f;
    int32 Initiative = 0;
    int32 LastDamage = 0;
    float LastDamageTime = -100.f;
    FString ImpactText;
    float ImpactTextTime = -100.f;
    bool bImpactHealing = false;
    bool bLastImpactCritical = false;
    FString Feedback;
    FString MotionLabel=TEXT("ATACANDO");
    bool bCharacterReady = false;
    bool bAncestrySelected=false;
    EAHAncestry Ancestry=EAHAncestry::Human;
    float BaseMovement=900.f;
    void ChooseAncestry(EAHAncestry Choice);
    static FString AncestryName(EAHAncestry Choice);
    static FString AncestryTrait(EAHAncestry Choice);
    EAHHeroClass HeroClass = EAHHeroClass::Fighter;
    int32 AttackBonus = 5, DamageSides = 8, DamageModifier = 3, InitiativeBonus = 1;
    int32 ClassCharges = 2;
    bool bRaging = false;
    int32 Level=1, Experience=0, Feat=0;
    int32 SpellSlots2=0, SelectedSpellLevel=1;
    bool bActionSurgeUsed=false, bReckless=false;
    int32 GuardTurns=0;
    void GainExperience(int32 Amount);
    bool ChooseFeat(int32 Choice);
    int32 MaxSpellSlots(int32 Rank) const;
    bool HasSpellSlot() const;
    void SpendSpellSlot();
    void CycleSpellLevel();
    void UseProgressionAbility();
    FString ProgressionAbilityName() const;
    void Rest();
    /**
     * Between camps. The once-per-encounter abilities come back; hit points and
     * spell slots do not. Roughly a short rest, and it is what keeps a string of
     * fights costly without making the walk across the valley unwinnable.
     */
    void ShortRest();
    int32 SorceryPoints=0, LayOnHands=5, Goodberries=0;
    bool bEmpowerNext=false, bSmiteArmed=false, bSneakUsed=false, bSteadyAim=false, bAimMovementLocked=false;

    // ── Features that were on the sheet and nowhere in the code ──────────────
    /**
     * Rogue, hidden. Advantage on the next attack, which for a rogue is the only
     * road to Sneak Attack there is.
     *
     * Sneak Attack needs advantage or an ally beside the target, and this game
     * has a party of one -- so before Hide existed the rogue's signature feature
     * essentially never fired, which is exactly what Lucas reported.
     */
    bool bHidden=false;
    /**
     * Moving carefully, out of combat.
     *
     * Halves the walking pace and shrinks every camp's notice range, and a fight
     * begun from here is an ambush. It is the piece that was missing between
     * "walking around" and "suddenly in initiative": without it the only way to
     * meet a camp is to blunder into it.
     */
    bool bSneaking=false;
    void ToggleSneak();
    /** True while the arcane Shield is standing (+5 CA), until your next turn. */
    int32 ShieldTurns=0;

    /** Ranger, Colossus Slayer: the extra die is once per turn, not per swing. */
    bool bColossusUsed=false;
    /** Cleric and paladin: Channel Divinity is once per short rest. */
    bool bChannelUsed=false;
    /** Paladin, Sacred Weapon: turns of +2 to hit still running. */
    int32 SacredTurns=0;

    // ── Subclass ─────────────────────────────────────────────────────────────
    /** True once the domain, origin or archetype has actually arrived. */
    bool HasSubclass() const
    { return AHRules::Subclass(HeroClass).Level > 0 && Level >= AHRules::Subclass(HeroClass).Level; }
    /** The round number of the fight in progress, or 0 outside one. */
    int32 CombatRound() const;
    /** True during the first round of an ambush this character is caught in. */
    bool  IsSurpriseRound() const;
    /**
     * Turns a hit that already rolled its damage into a critical.
     *
     * The doubling happens inside RollAttack, so a feature that widens the crit
     * range -- the champion's 19, the assassin's surprise -- has to add the
     * extra weapon die itself rather than re-rolling the attack.
     */
    void UpgradeToCritical(FAHDiceOutcome& Roll, int32 Sides, const TCHAR* Why);
    /** Applies whichever subclass widens the critical range, if any. */
    void ApplySubclassCrit(FAHDiceOutcome& Roll, int32 Sides);
    /** Attack bonus including Sacred Weapon. */
    int32 AttackRollBonus() const { return AttackBonus + (SacredTurns>0?2:0); }
    /** Paladin, Channel Divinity: +2 to hit for ten turns, once per short rest. */
    void SacredWeapon();
    /** Barbarian, Frenzy: a second swing on the bonus action while raging. */
    void Frenzy(AAHCharacter* Target);

    /** Hide. An action at level 1, a Cunning Action at level 2. Needs cover. */
    void Hide();
    /** Why the rogue cannot hide right now, or empty when they can. */
    FString HideRefusal() const;
    /**
     * SRD Shield: five more armour class against the attack that triggered it and
     * everything until your next turn. Spends the reaction and a first-circle
     * slot, and ONLY fires when those five points change the answer -- a slot
     * burnt on an attack that was going to miss anyway is a rule nobody wants.
     * Rewrites Roll in place; returns true when the hit became a miss.
     */
    /**
     * Why the Shield cannot answer this attack, or empty when it can.
     *
     * One function rather than a bool and a separate explanation: the two would
     * drift, and a silent gate is exactly what made this spell look broken for
     * three builds. The caller logs the reason and puts it in the combat log, so
     * a Shield that correctly does nothing still says so.
     */
    FString ShieldRefusal() const;
    /** Spends the reaction and the slot, puts the Shield up and turns Roll into a miss. */
    void RaiseShield(FAHDiceOutcome& Roll);
    /**
     * An attack held in the air while its target decides about the Shield.
     *
     * The prompt pauses the game, so the swing cannot finish resolving inside
     * the call that asked. It is parked here on the ATTACKER and picked up again
     * by ResumeShieldedImpact once the answer comes back.
     */
    UPROPERTY() TObjectPtr<AAHCharacter> HeldTarget;
    FAHDiceOutcome HeldRoll;
    float HeldShotRange = 0.f;
    /** Stops the resumed impact from asking the same question a second time. */
    bool  bShieldAsked = false;
    void  ResumeShieldedImpact(bool bShield);
    /** Attack bonus for a shot, including the Archery fighting style. */
    int32 RangedAttackBonus() const;
    /** Damage bonus for a melee swing, including the Duelling fighting style. */
    int32 MeleeDamageBonus() const;
    float MovementSpentThisTurn=0;
    TWeakObjectPtr<AAHCharacter> MarkedTarget;
    int32 MarkTurns=0;
    void UseClassUtility();
    void ToggleEmpower();
    void SteadyAim();
    void EatGoodberry();
    int32 AddWeaponRiders(AAHCharacter* Target, FAHDiceOutcome& Roll, bool bRanged);
    int32 SpellDamageDie(int32 Sides, int32& Rerolls);
    bool bPreparingSpells=false;
    TArray<EAHSpell> PreparedSpells;
    EAHSpell SelectedSpell=EAHSpell::CureWounds;
    bool IsSpellAvailable(EAHSpell Id) const;
    bool TogglePreparedSpell(EAHSpell Id);
    bool SelectSpell(EAHSpell Id);
    bool CastSpell(EAHSpell Id, AAHCharacter* Target=nullptr);
    void InitializeSpellbook();
    int32 PreparedLimit() const { return HeroClass==EAHHeroClass::Sorcerer?Level+1:HeroClass==EAHHeroClass::Ranger?FMath::Min(Level,3):HeroClass==EAHHeroClass::Paladin?2+Level/2:Level+3; }
    int32 AidBonus=0, MageArmorBonus=0, FrostTurns=0, BlessTurns=0;
    TWeakObjectPtr<AAHCharacter> GuidingSource;
    int32 GuidingExpiresTurn=0, TurnsStarted=0;
    bool HasGuidingMark() const;
    bool bBonusSpellCast=false, bLeveledActionSpellCast=false;


    // ── Death saves (D&D 5e downed state) ────────────────────────────────────
    /** True when HP == 0 but death saves haven't been exhausted yet. */
    bool bDowned = false;
    /** True after accumulating 3 successful death saves (stable). */
    bool bStabilized = false;
    int32 DeathSuccesses = 0;
    int32 DeathFailures  = 0;
    /** World-time when a death-save roll last happened (for HUD flash). */
    float DeathSaveRollTime = -100.f;
    bool  bLastDeathSaveSuccess = false;

    // ── Temporary HP ─────────────────────────────────────────────────────────
    /** Absorbed before real HP; shown as a white arc overlay on the HP orb. */
    int32 TempHP = 0;

    // ── Turn timer ────────────────────────────────────────────────────────────
    /** World-time when the current turn began (set in StartTurn, used by HUD). */
    float TurnStartTime = -1.f;

    void ChooseClass(EAHHeroClass Choice);
    void UseClassAbility();
    bool CanUseClassAbility() const;
    static FString ClassName(EAHHeroClass Choice);
    FString ClassAbilityName() const;
    FString ClassAbilityDescription() const;
    TArray<FString> CombatLog;

    // ── Animation state (read by UAHAnimInstance) ─────────────────────────────
    /**
     * True during the attack slot animation window.
     * UAHAnimInstance polls this every frame to drive upper-body blend.
     * Set in PlayAttack(), cleared when the slot finishes.
     */
    UPROPERTY(BlueprintReadOnly, Category="AH|Animation")
    bool bIsAttacking = false;

    // ── Visual FX ─────────────────────────────────────────────────────────────
    /**
     * Optional Niagara system spawned at the character's torso when it receives
     * a hit. Leave null to use the built-in instanced spark effect.
     * Assign any NS_* asset in the Blueprint Details panel to upgrade to a real
     * particle effect with zero code changes.
     */
    UPROPERTY(EditDefaultsOnly, Category="AH|FX")
    TObjectPtr<UNiagaraSystem> HitFX;

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool IsBusy() const    { return AnimationEnds > 0.f; }
    bool IsAlive() const   { return Health > 0; }
    /** True while downed but still rolling saves (not yet truly dead). */
    bool IsDowned() const  { return bDowned && DeathFailures < 3 && !bStabilized; }
    /** Cannot act (attack, move) — downed or dead. */
    bool CanAct() const    { return IsAlive() && bTurnActive && !IsBusy(); }

    // ── Combat actions ────────────────────────────────────────────────────────
    void StartTurn();
    void FinishTurn();
    void AddLog(const FString& Message);
    void Dash();
    void Dodge();
    /** Action: this turn's movement no longer provokes opportunity attacks. */
    void Disengage();
    void SecondWind();

    // ── Reactions (opportunity attacks) ───────────────────────────────────────
    /**
     * Runs every frame. Tracks which hostiles' melee reach this character is
     * currently standing inside, and fires one opportunity attack each time it
     * leaves one under its own power during its own turn.
     * The state must be a latch, not a per-frame distance comparison: a walking
     * character crosses the reach boundary a few centimetres at a time, so no
     * single frame ever spans the whole hysteresis band.
     * Public so automation tests can step the state machine by hand.
     */
    void UpdateThreatState();

    /**
     * Spends this character's reaction on a free attack against a hostile that
     * just left its melee reach.  Returns false when the reaction is unavailable,
     * the line is blocked, or either side cannot fight.
     */
    bool TryOpportunityAttack(AAHCharacter* Mover, bool bConfirmed=false);
    void CheckArcana();
    /** bBonusAction spends the bonus instead of the action: barbarian Frenzy. */
    bool TryAttack(AAHCharacter* Target, bool bBonusAction=false);

    // ── Ranged attacks ────────────────────────────────────────────────────────
    // ── Camera ────────────────────────────────────────────────────────────────
    /**
     * Turns the view by Degrees. A fixed camera hides whatever a house happens to
     * stand in front of, and in a game where cover and line of sight decide the
     * roll, not being able to look round a corner is a rules problem and not only
     * a comfort one.
     *
     * Eight steps of 45 degrees rather than free rotation: the isometric read is
     * what makes distance judgeable at a glance, and a camera that can sit at any
     * angle quietly takes that away.
     */
    void RotateCamera(float Degrees);
    /** Pulls the view in or out one notch. */
    void ZoomCamera(float Steps);

    // ── Cover and high ground ────────────────────────────────────────────────
    /** Ground level under this character, not the capsule centre. */
    FVector FootLocation() const;
    /** How much the arena's obstacles shield THIS character from Shooter. */
    EAHCover CoverFrom(const AAHCharacter* Shooter) const;
    /**
     * True when this character stands high enough above Target to shoot down on
     * it. Advantage from high ground is a Baldur's Gate rule, not an SRD one; it
     * is here because parity with that game is the goal.
     */
    bool HasHighGroundOn(const AAHCharacter* Target) const;

    /** True when this archetype can shoot; see FAHClassSheet::RangedRange. */
    /**
     * The bow is now a thing you are HOLDING, not a thing your class has.
     *
     * RangedRange is filled by RecomputeSheet from the weapon in the main
     * hand, and falls back to the class table for anybody with no kit -- which
     * is every foe. So a ranger who put his longbow away fights in melee, and
     * a wizard who picked up a crossbow can shoot, and neither of those was
     * possible while the answer came from the class.
     */
    bool HasRangedAttack() const { return RangedRange > 0; }
    /** Range in centimetres, 0 for a melee-only archetype. */
    float RangedReach() const { return static_cast<float>(RangedRange); }
    /** A hostile within melee reach: shooting from here is at disadvantage (SRD 5.1). */
    bool IsThreatenedInMelee() const;
    /** Spends the action on a shot. Resolves on the animation notify like a melee swing. */
    bool TryRangedAttack(AAHCharacter* Target);
    void ReceiveHit(int32 Damage, EAHDamageType Type = EAHDamageType::Physical, int32 RadiantBonus=0,
                    bool bCriticalHit=false);

    // ── Saves and healing ─────────────────────────────────────────────────────
    /** Constitution modifier used for concentration saves. */
    int32 ConcentrationModifier() const;
    /** 8 + proficiency + casting modifier (PHB spellcasting). */
    int32 SpellSaveDC() const;
    /** Proficiency + casting modifier. */
    int32 SpellAttackBonus() const;
    /**
     * Single entry point for every source of healing. Any healing above 0 HP
     * ends the downed state and clears death saves, as the rules require.
     */
    int32 ApplyHealing(int32 Amount);
    void BecomeEnemy(int32 AppearanceSeed = 0);
    /**
     * Gives this body a colour.
     *
     * Every character in the game is the same UE5 mannequin, which ships as
     * untextured grey plastic -- and with a hero, up to eighteen foes and ten
     * villagers on screen, that is most of what Lucas means by "muitas
     * texturas ainda continuam cinza". The scenery has had twenty-four colours
     * and per-face shading for a while; the people never did.
     *
     * A dynamic instance of M_AH_Corpo per material slot. Does nothing at all
     * when the material is missing, so a project whose kit has not been
     * imported yet simply keeps the grey mannequin rather than failing.
     */
    void PaintBody(const FLinearColor& Tone);
    /** Searches reachable, unoccupied attack positions instead of chasing a capsule centre. */
    bool FindCombatPosition(AAHCharacter* Target, bool bKeepDistance, FVector& Out) const;

    /**
     * Called by UAHNotify_MeleeImpact when the attack animation reaches the
     * impact frame.  Resolves the pending d20 roll immediately instead of
     * waiting for the fallback timer.
     */
    void OnMeleeImpactNotify();

    /**
     * Resolves the pending d20 roll against the target.
     * Public so automation tests can trigger impact directly.
     * Called internally by OnMeleeImpactNotify() (frame-perfect path)
     * and by the fallback timer in Tick().
     */
    void ResolveImpact();

    bool bSecondWindUsed = false;
    /** Half-orc: the one refusal to fall has been spent since the last rest. */
    bool bRelentlessUsed = false;
    /** Dragonborn: the breath has been spent since the last rest. */
    bool bBreathUsed = false;
    /** True once this character has fled a fight since the last rest. */
    bool bHasRetreated = false;

    // ── Ancestry ability ──────────────────────────────────────────────────────
    /** Empty when this ancestry has no active ability. */
    FString RacialAbilityName() const;
    bool CanUseRacialAbility() const;
    void UseRacialAbility();

    /** Rerolls a natural 1. Foes do not benefit, as before the data tables. */
    bool IsLucky() const { return !bEnemy && AHRules::Ancestry(Ancestry).bLucky; }

private:
    // Where the view is now and where it is heading. Easing between the two is
    // what keeps a 45 degree step from losing the player's bearings.
    float CameraYaw = -45.f, CameraYawTarget = -45.f;
    float CameraReach = 2200.f, CameraReachTarget = 2200.f;
    static constexpr float CameraPitch = -48.f;

    FRandomStream Dice;
    float NextThink = 0.f;
    void TickEnemyAI(float Now);
    bool bPatrolling=false;
    /**
     * This foe stands in a dungeon room rather than in the open.
     *
     * Set by the game mode from the camp that woke him, and read by exactly one
     * line: how far he may patrol. A four-and-a-half-metre patrol is right for
     * a camp in a clearing and wrong for a room four and a half metres across,
     * where it would walk him into the corridor and bring his fight with him.
     */
    // Public, because the game mode is what knows: it woke the camp, and the
    // camp is what knows whether it is a clearing or a room.
public:
    bool bIndoorFoe=false;
private:
    FVector EnemyHome=FVector::ZeroVector, ProgressLocation=FVector::ZeroVector;
    FVector LastPursuedLocation=FVector::ZeroVector;
    float RepathAt=0.f, ProgressAt=0.f, PatrolAt=0.f;
    int32 FailedPaths=0;

    /** Absolute time when the attack slot finishes.  0 = not animating. */
    float AnimationEnds = 0.f;
    float ReactionEnds = 0.f;
    int32 RageTurns = 0;
    bool bAttackedSinceTurnEnd = false, bDamagedSinceTurnEnd = false;
    int32 PendingSpellDamage = 0;
    int32 PendingSpellId=-1, PendingSpellRank=1, PendingEmpowerRerolls=0;
    void ResolveSpellImpact();
    /** >0 while a shot is in flight; also the distance the target may drift to. */
    float PendingRange = 0.f;

    /**
     * Absolute time for the impact fallback timer.
     * Computed as animation-start-time + (sequence-length * ImpactFraction).
     * Set to -1 once the notify has already fired to prevent double-resolution.
     */
    float ImpactAt = 0.f;

    /** Fraction of the attack animation length at which the fallback fires. */
    static constexpr float ImpactFraction = 0.35f;

    /**
     * Melee threat radius in centimeters.  A hostile that starts a movement step
     * inside this radius and ends it outside provokes an opportunity attack.
     * Matches the 190 cm reach used by TryAttack, with a small tolerance.
     */
    static constexpr float ThreatReach = 200.f;

    /** Reach of the draconic breath, in centimetres. */
    static constexpr float BreathReach = 450.f;

    /** Hostiles whose melee reach this character is currently standing inside. */
    UPROPERTY() TArray<TObjectPtr<AAHCharacter>> InReachOf;

    /** Applies a level-1 archetype sheet plus ancestry traits. Shared by hero and foe. */
    void ApplySheet(EAHHeroClass Choice);

public:
    // ── The character sheet ──────────────────────────────────────────────
    /**
     * The six ability scores, after the ancestry's bonus.
     *
     * Ten across the board for anybody who never went through creation, which
     * means every modifier is zero and every derived number falls back to the
     * class table -- so a foe is exactly the foe it was before this existed.
     */
    FAHAbilities Abilities;
    /** What is in each slot, by item id. Empty means the slot is bare. */
    FString Equipped[static_cast<int32>(EAHSlot::Count)];
    /** What is in the pack: ids and how many of each. */
    struct FAHCarried { FString Id; int32 Many = 1; };
    TArray<FAHCarried> Backpack;
    /** Which of the class's two starting kits was taken. -1 for none. */
    int32 StartingKit = -1;
    /**
     * True once the player has touched the point-buy screen.
     *
     * Until then, picking a class loads that class's suggested spread -- so
     * somebody who clicks straight past creation still gets a character that
     * works, and somebody who does spend the points does not have them wiped
     * when he changes his mind about the class afterwards... which he will,
     * because the sheet is right there showing him what each class does with
     * what he bought.
     */
    bool bAbilitiesChosen = false;
    /** Step two of creation is done: a class is chosen but play has not begun. */
    bool bClassPicked = false;
    /**
     * Step three is done: the player has left the point-buy screen.
     *
     * A separate flag from bAbilitiesChosen, and the difference matters:
     * bAbilitiesChosen goes true the moment he moves ONE point, which is what
     * stops the suggestion overwriting his work -- and if the screen were
     * gated on that, the screen would close itself on his first click.
     */
    bool bPointsDone = false;

    /**
     * Creation, in four steps instead of two.
     *
     * It used to be ancestry then class, and clicking the class card started
     * the game. Now the class card only CHOOSES -- the points and the kit come
     * after it, and only the kit card starts play. Splitting them is what lets
     * the ability screen show what each point does to this class's own armour
     * class and hit points while you spend it, which is the entire reason to
     * have point buy rather than a fixed array.
     */
    void PickClass(EAHHeroClass Choice);
    /** Moves one score by one, if the budget and the 8..15 range allow it. */
    bool BuyAbility(EAHAbility Which, int32 Delta);
    /** Takes that kit and starts the game. The last click of creation. */
    void BeginWith(int32 Kit);
    /** Reach and die of the weapon in hand, or the class's own if bare-handed. */
    int32 RangedRange = 0;
    int32 RangedSides = 0;
    /** Flat damage on a shot: the class's own for a foe, the sheet's for a hero. */
    int32 RangedBonus = 0;
    /** Spell attack and DC, derived. The HUD shows both on the sheet. */
    int32 SpellAttack = 0;
    int32 SpellDC     = 10;
    /** Armour heavier than this Strength: three metres slower, and it shows. */
    bool  bOverloaded = false;

    /**
     * Works the whole sheet out from the abilities and what is equipped.
     *
     * Everything derived lands in the fields the rest of the game already
     * reads -- ArmorClass, AttackBonus, DamageSides, MaxHealth -- so a hundred
     * call sites never learn that ability scores exist. bFull is true at
     * creation and on a level up, where the new maximum should be handed over
     * whole rather than added to a wounded character.
     */
    void RecomputeSheet(bool bFull = false);
    /** Puts an item in its slot, returning whatever came out of it. */
    FString Equip(const FString& Id);
    /** Empties a slot into the pack. */
    void Unequip(EAHSlot Slot);
    /** Adds to the pack, stacking. */
    void Carry(const FString& Id, int32 Many = 1);
    /** Hands out one of the class's two kits and wears the wearable half. */
    void TakeKit(int32 Which);
    bool UseBackpackItem(int32 Index);
    int32 HitDice = 1;
    bool bArcaneRecoveryUsed = false;
    int32 WeaponDice() const;
private:

    FVector PreviousLocation;
    bool bImpactResolved = false;     // prevents double-resolution (notify + timer)

    UPROPERTY() TObjectPtr<AAHCharacter> PendingTarget;
    UPROPERTY() TObjectPtr<AAHMagicVisual> MagicVisual;
    FAHDiceOutcome PendingRoll;
    UPROPERTY() TObjectPtr<UAnimationAsset> AttackAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> AlternateAttackAnimation;
    uint32 AttackAnimationIndex = 0;
    UPROPERTY() TObjectPtr<UAnimationAsset> DeathAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> HitAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> CastAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> HealAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> RageAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> GuardAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> EvadeAnimation;
    UPROPERTY() TArray<TObjectPtr<UAnimationAsset>> WeaponAnimations;
    UPROPERTY() TObjectPtr<UAHEquipmentComponent> Equipment;
    UPROPERTY() TObjectPtr<UAHLifeAudioComponent> LifeAudio;
    UPROPERTY() TSubclassOf<UAnimInstance> LocomotionClass;

    /** The swing clip for this archetype's weapon, or the generic attack as fallback. */
    UAnimationAsset* WeaponClip() const;
    void PlayAttack();
    void PlayGesture(UAnimationAsset* Asset);
    /** Roll one d20 death save; updates counts; may revive or transition to true death. */
    void RollDeathSave();
    void CompleteDeathSaveTurn();
    FTimerHandle DeathSaveTimer;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;
};
