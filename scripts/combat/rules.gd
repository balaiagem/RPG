class_name CombatRules
extends RefCounted
## Pure mechanics: no actors, scene tree, animation, or wall-clock dependencies.

static func modifier(score: int) -> int:
	return floori((score - 10) / 2.0)

static func proficiency(level: int) -> int:
	return 2 + floori((maxi(level, 1) - 1) / 4.0)

static func d20(rng: RandomNumberGenerator, advantage: int = 0) -> int:
	var first := rng.randi_range(1, 20)
	if advantage == 0:
		return first
	var second := rng.randi_range(1, 20)
	return maxi(first, second) if advantage > 0 else mini(first, second)

static func dice(expression: String, rng: RandomNumberGenerator, critical: bool = false) -> int:
	var parts := expression.split("d")
	var total := 0
	for i in range(int(parts[0]) * (2 if critical else 1)):
		total += rng.randi_range(1, maxi(1, int(parts[1])))
	return total

static func attack(rng: RandomNumberGenerator, bonus: int, armor: int, expression: String, damage_bonus: int, advantage: int = 0) -> Dictionary:
	var natural := d20(rng, advantage)
	var critical := natural == 20
	var hit := critical or (natural != 1 and natural + bonus >= armor)
	return {"roll":natural,"total":natural + bonus,"hit":hit,"critical":critical,"damage":dice(expression, rng, critical) + damage_bonus if hit else 0}

static func saving_throw(rng: RandomNumberGenerator, score: int, dc: int) -> Dictionary:
	var natural := d20(rng)
	return {"roll":natural,"total":natural + modifier(score),"success":natural + modifier(score) >= dc}

