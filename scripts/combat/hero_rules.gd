class_name HeroRules
extends Node
## Level-one character mechanics, isolated from input and presentation geometry.
var game: Node3D
var hero: Warden
var data: Dictionary
var resources: RoundResources
var base_ac := 10
var dash_time := 0.0
var heroism_tick := 0.0
var death_timer := 6.0
var death_successes := 0
var death_failures := 0
var downed := false
var stable := false

func configure(owner_game: Node3D, stats: Dictionary) -> void:
	game = owner_game
	hero = game.player
	data = stats
	base_ac = stats.armorClass
	resources = RoundResources.new()
	add_child(resources)
	resources.configure(stats)

func _process(delta: float) -> void:
	dash_time = maxf(0, dash_time - delta)
	hero.speed = float(data.speed) * (2 if dash_time > 0 else 1)
	hero.armor = base_ac
	if resources.mage_armor:
		hero.armor = maxi(hero.armor, 13 + mod("dexterity"))
	if resources.concentration == "shield_of_faith":
		hero.armor += 2
	if resources.concentration == "heroism":
		heroism_tick -= delta
		if heroism_tick <= 0:
			hero.temporary_hp = maxi(hero.temporary_hp, maxi(1, casting_modifier()))
			hero.barrier_time = 7
			heroism_tick = 6
	if downed and not stable:
		death_timer -= delta
		if death_timer <= 0:
			death_timer = 6
			var roll := CombatRules.d20(game.rng)
			game.dice.present("Death saving throw", roll, roll, 10, roll >= 10, "%d successes / %d failures" % [death_successes, death_failures])
			if roll == 20:
				revive()
			elif roll >= 10:
				death_successes += 1
			else:
				death_failures += 2 if roll == 1 else 1
			if death_successes >= 3:
				stable = true
			if death_failures >= 3:
				downed = false

func mod(stat: String) -> int:
	return CombatRules.modifier(int(data.attributes.get(stat, 10)))

func casting_modifier() -> int:
	return mod(data.castingStat) if not str(data.castingStat).is_empty() else mod("wisdom")

func dc() -> int:
	return 10 + casting_modifier()

func d20(advantage: int = 0) -> int:
	var roll := CombatRules.d20(game.rng, advantage)
	if roll == 1 and "lucky" in data.traits:
		roll = CombatRules.d20(game.rng)
	return roll

func attack_roll(target: Combatant, expression: String, stat: String, damage_bonus: int, advantage: int = 0) -> Dictionary:
	var roll := d20(advantage)
	var total := roll + mod(stat) + 2
	var hit := roll == 20 or (roll != 1 and total >= target.armor)
	var damage := CombatRules.dice(expression, game.rng, roll == 20) + damage_bonus if hit else 0
	return {"roll":roll,"total":total,"hit":hit,"critical":roll == 20,"damage":maxi(0, damage)}

func weapon_attack(target: Combatant, martial: bool = false) -> bool:
	var channel := "bonus" if martial else "action"
	if not resources.available(channel) or (martial and resources.martial_window <= 0):
		return false
	if target.dead or hero.position.distance_to(target.position) > float(hero.weapon_data.attackRange if not martial else 2.1):
		return false
	resources.spend(channel)
	resources.combat_activity = 6
	hero.attack_pose = 1
	hero.attack_timer = 0.1
	var stat: String = "dexterity" if martial else hero.weapon_data.statScaling
	var expression: String = "1d4" if martial else hero.weapon_data.damageDice
	var advantage := 1 if resources.hidden or target.root_time > 0 or target.marked_time > 0 else 0
	if hero.weapon_data.id == "longbow" and hero.position.distance_to(target.position) < 2.1:
		advantage = 0 if advantage > 0 else -1
	var bonus := mod(stat) + (2 if resources.rage_time > 0 and stat == "strength" else 0)
	var result := attack_roll(target, expression, stat, bonus, advantage)
	if result.hit and "Sneak Attack" in data.passives and advantage > 0 and resources.sneak_time <= 0:
		result.damage += CombatRules.dice("1d6", game.rng, result.critical)
		resources.sneak_time = 6
	if result.critical and "savage_attacks" in data.traits and float(hero.weapon_data.attackRange) < 3:
		result.damage += CombatRules.dice(expression, game.rng)
	resources.hidden = false
	target.marked_time = 0
	if not martial:
		resources.martial_window = 6
	game.resolve_attack(hero, target, result, "Martial Arts" if martial else hero.weapon_data.name)
	game.effects.pulse(target.global_position, 0.8, Color(data.color), 0.3)
	return true

func check_skill(skill: String, target: int, advantage: int = 0) -> Dictionary:
	var bonus := mod(CharacterBuild.SKILL_STATS.get(skill, "wisdom"))
	if resources.rage_time > 0 and CharacterBuild.SKILL_STATS.get(skill) == "strength":
		advantage = 1
	if skill in data.skills:
		var expertise: bool = "Expertise" in data.passives and skill in data.skills.slice(0, 2)
		var forest_expertise: bool = data["class"] == "ranger" and CharacterBuild.SKILL_STATS.get(skill) in ["wisdom", "intelligence"]
		bonus += 4 if expertise or forest_expertise else 2
	if data["class"] == "ranger" and skill in ["survival", "nature"]:
		advantage = 1
	var roll := d20(advantage)
	var result := {"roll":roll,"total":roll + bonus,"success":roll + bonus >= target}
	game.dice.present(skill.capitalize(), roll, result.total, target, result.success, "Advantage" if advantage > 0 else "Ability check")
	return result

func incoming(amount: int, critical: bool, damage_type: String, attacker: Combatant) -> int:
	resources.combat_activity = 6
	if resources.rage_time > 0 and damage_type in ["physical", "slashing", "bludgeoning", "piercing"]:
		amount = floori(amount / 2.0)
	if damage_type == "fire" and "fire_resistance" in data.traits:
		amount = floori(amount / 2.0)
	if damage_type == "poison" and "poison_resilience" in data.traits:
		amount = floori(amount / 2.0)
	if resources.agathys and hero.temporary_hp > 0 and is_instance_valid(attacker) and hero.position.distance_to(attacker.position) <= 2.1:
		attacker.receive_damage(5, false, "cold")
	if resources.rebuke_armed and resources.available("reaction") and resources.slots > 0 and is_instance_valid(attacker):
		resources.rebuke_armed = false
		resources.spend("reaction")
		resources.slots -= 1
		var save := CombatRules.saving_throw(game.rng, 10, dc())
		var damage := CombatRules.dice("2d10", game.rng)
		attacker.receive_damage(floori(damage / 2.0) if save.success else damage, false, "fire")
		game.dice.present("Hellish Rebuke • DEX", save.roll, save.total, dc(), save.success, "Half damage" if save.success else "Full fire damage")
	if amount > 0 and not resources.concentration.is_empty():
		var roll := d20()
		var total := roll + mod("constitution") + (2 if "constitution" in CharacterBuild.catalog("classes_5e")[data["class"]].savingThrowProficiencies else 0)
		var target := maxi(10, floori(amount / 2.0))
		game.dice.present("Concentration • CON", roll, total, target, total >= target, resources.concentration.capitalize())
		if total < target:
			break_concentration()
	if "relentless_endurance" in data.traits and not resources.endurance_used and amount >= hero.health + hero.temporary_hp and amount < hero.health + hero.temporary_hp + hero.max_health:
		resources.endurance_used = true
		amount = hero.health + hero.temporary_hp - 1
		game.hud.notify("Relentless Endurance — remain at 1 HP")
	return amount

func break_concentration() -> void:
	if resources.concentration == "entangle":
		for enemy in game.enemies:
			enemy.root_time = 0
	resources.concentration = ""
	resources.concentration_time = 0

func on_kill() -> void:
	if "Dark One's Blessing" in data.passives:
		hero.temporary_hp = maxi(hero.temporary_hp, maxi(1, mod("charisma") + 1))
		hero.barrier_time = 3600
		resources.agathys = false

func on_downed() -> void:
	downed = true
	stable = false
	death_timer = 6
	death_successes = 0
	death_failures = 0
	break_concentration()

func revive() -> void:
	hero.dead = false
	hero.health = 1
	hero.collision_layer = 2
	hero.visual.rotation.z = 0
	hero.ring.visible = true
	downed = false
	stable = false

func rest(long: bool) -> bool:
	if not game.enemy.dead and game.enemy.state != "idle":
		game.hud.notify("Rest is unavailable while enemies are alert")
		return false
	if hero.dead:
		return false
	if long:
		resources.long_rest(data)
		hero.health = hero.max_health
		hero.temporary_hp = 0
		game.hud.notify("Long rest — eight hours pass. HP, slots and features restored.")
	else:
		resources.short_rest()
		if resources.hit_die and hero.health < hero.max_health:
			resources.hit_die = false
			var die: int = CharacterBuild.catalog("classes_5e")[data["class"]].hitDie
			hero.health = mini(hero.max_health, hero.health + maxi(0, game.rng.randi_range(1, die) + mod("constitution")))
		game.hud.notify("Short rest — one hour passes. Available rest features restored.")
	break_concentration()
	return true
