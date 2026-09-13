class_name FifthEditionAbilities
extends AbilityController

func setup(owner_game: Node3D) -> void:
	game = owner_game
	slots = game.characters.player.abilities.duplicate()
	for id in slots:
		cooldowns[id] = 0.0

func _process(_delta: float) -> void:
	if not is_instance_valid(game.player.rules):
		return
	for id in slots:
		var kind: String = game.ability_data[id].castType
		cooldowns[id] = game.player.rules.resources.actions.get(kind, 0.0)

func target_at(point: Vector3) -> Combatant:
	var best: Combatant
	var distance := 3.0
	for enemy in game.enemies:
		if not enemy.dead and enemy.position.distance_to(point) < distance:
			best = enemy
			distance = enemy.position.distance_to(point)
	if best == null and is_instance_valid(game.player.target) and not game.player.target.dead:
		best = game.player.target
	return best

func cast(slot: int, point: Vector3) -> bool:
	if slot < 0 or slot >= slots.size():
		return false
	var hero: Warden = game.player
	var rules: HeroRules = hero.rules
	var res: RoundResources = rules.resources
	var data: Dictionary = game.ability_data[slots[slot]]
	if hero.dead or hero.stagger_time > 0:
		return false
	if not res.available(data.castType):
		game.hud.notify(data.castType.capitalize() + " recovers in %.1fs" % res.actions[data.castType])
		return false
	if data.school != "Martial" and data.school != "Exploration" and res.rage_time > 0:
		game.hud.notify("You cannot cast spells while raging")
		return false
	if int(data.spellSlotCost) > res.slots:
		game.hud.notify("No first-level spell slots remain. Rest to recover.")
		return false
	if int(data.level) > 0 and ((data.castType == "bonus" and res.last_leveled_action > 0) or (data.castType == "action" and res.last_bonus_spell > 0)):
		game.hud.notify("A bonus-action spell allows only an action cantrip this round")
		return false
	if data.has("resource") and int(res.pools.get(data.resource, 0)) <= 0:
		game.hud.notify("No uses remain — rest to recover " + data.name)
		return false
	var target := target_at(point)
	if data.handler == "inspiration" and (not is_instance_valid(game.companion) or game.companion.dead or hero.position.distance_to(game.companion.position) > 18.3):
		game.hud.notify("Bardic Inspiration needs a living companion within 60 feet")
		return false
	if data.targetingType == "TARGET_ENEMY":
		if target == null:
			game.hud.notify("Aim at an enemy or select one with right click")
			return false
		if hero.position.distance_to(target.position) > float(data.range):
			game.hud.notify("Target is out of range")
			return false
		var query := PhysicsRayQueryParameters3D.create(hero.position + Vector3.UP, target.position + Vector3.UP, 1)
		if not hero.get_world_3d().direct_space_state.intersect_ray(query).is_empty():
			game.hud.notify("Your target is behind cover")
			return false
	if data.handler == "weapon":
		return rules.weapon_attack(target)
	if data.handler == "martial":
		if res.martial_window <= 0:
			game.hud.notify("Use an Attack action before Martial Arts")
		return rules.weapon_attack(target, true)
	if data.handler == "hide" and hero.position.distance_to(game.enemy.position) < 6 and not game.enemy.dead:
		game.hud.notify("Too exposed to hide. Move at least 6 metres away.")
		return false
	if data.targetingType == "GROUND_POINT":
		if hero.position.distance_to(point) > float(data.range) or not game.navigation.walkable(point):
			game.hud.notify("Choose clear ground within range")
			return false
	res.spend(data.castType)
	if data.handler in ["spell_attack", "save", "missile"]:
		res.hidden = false
		res.combat_activity = 6
	res.slots -= int(data.spellSlotCost)
	if int(data.level) > 0:
		if data.castType == "bonus":
			res.last_bonus_spell = 6
		elif data.castType == "action":
			res.last_leveled_action = 6
	if data.has("resource") and data.resource != "lay_hands":
		res.pools[data.resource] -= 1
	if data.concentration:
		rules.break_concentration()
		res.concentration = data.id
		res.concentration_time = float(data.duration)
	hero.attack_pose = 1
	hero.face(point)
	game.effects.sound(data.audioEffect, hero.position)
	match data.handler:
		"inspiration":
			game.companion.inspiration = 600
			game.effects.number(game.companion.position + Vector3.UP * 2, "INSPIRED · d6", Color("dbb9ed"))
		"heal":
			var bonus: int = rules.casting_modifier() if data.get("healCasting", false) else int(data.get("healBonus", 0))
			if "Disciple of Life" in rules.data.passives and int(data.level) > 0:
				bonus += 2 + int(data.level)
			var healing := maxi(0, CombatRules.dice(data.damageDice, game.rng) + bonus)
			hero.health = mini(hero.max_health, hero.health + healing)
			game.effects.number(hero.position + Vector3.UP * 2, "+%d HP" % healing, Color("9ad5ac"))
		"lay_hands":
			var healing := mini(int(res.pools.lay_hands), hero.max_health - hero.health)
			res.pools.lay_hands -= healing
			hero.health += healing
			game.effects.number(hero.position + Vector3.UP * 2, "+%d HP" % healing, Color("9ad5ac"))
		"rage":
			res.rage_time = 60
			res.combat_activity = 6
			rules.break_concentration()
		"dodge": res.dodge_time = 6
		"dash": rules.dash_time = 6
		"hide": res.hidden = rules.check_skill("stealth", 12).success
		"sense": game.hud.notify("Divine Sense — no celestials, fiends or undead. The Thornbound is a plant.")
		"study": rules.check_skill("survival", 12)
		"mage_armor": res.mage_armor = true
		"agathys":
			hero.temporary_hp = maxi(hero.temporary_hp, 5)
			hero.barrier_time = 3600
			res.agathys = true
		"rebuke":
			res.rebuke_armed = true
			game.hud.notify("Hellish Rebuke armed — triggers when you take damage")
		"spell_attack":
			var result := rules.attack_roll(target, data.damageDice, rules.data.castingStat, 0, 1 if target.root_time > 0 or target.marked_time > 0 else 0)
			result.damageType = data.damageType
			target.marked_time = 0
			game.resolve_attack(hero, target, result, data.name)
			if result.hit:
				apply_status(target, data)
			game.effects.pulse(target.position, 0.9, Color(rules.data.color), 0.4)
		"missile":
			target.receive_damage(CombatRules.dice("3d4", game.rng) + 3, false, "force", hero)
			game.hud.log_event("Magic Missile — three darts hit automatically")
			game.effects.pulse(target.position, 1, Color("a6c9ed"), 0.5)
		"save":
			var origin: Vector3 = point if data.targetingType == "GROUND_POINT" else hero.position
			game.effects.pulse(origin, maxf(1, float(data.radius)), Color(rules.data.color), 0.6)
			for enemy in game.enemies:
				if enemy.dead:
					continue
				if data.targetingType == "TARGET_ENEMY" and enemy != target:
					continue
				if data.targetingType != "TARGET_ENEMY" and enemy.position.distance_to(origin) > float(data.radius):
					continue
				if data.targetingType == "CONE" and (point - hero.position).normalized().dot((enemy.position - hero.position).normalized()) < cos(PI / 4):
					continue
				var target_dc := 10 + rules.mod("constitution") if data.id == "breath_weapon" else rules.dc()
				var save := CombatRules.saving_throw(game.rng, int(enemy.definition.attributes.get(data.savingThrow, 10)), target_dc)
				var damage := CombatRules.dice(data.damageDice, game.rng)
				if save.success:
					damage = floori(damage / 2.0) if data.get("halfSave", false) else 0
				else:
					apply_status(enemy, data)
				enemy.receive_damage(damage, false, data.damageType, hero)
				game.dice.present(data.name + " • " + str(data.savingThrow).left(3).to_upper(), save.roll, save.total, target_dc, save.success, "%d damage%s" % [damage, " • save negated effect" if save.success else ""])
				game.hud.log_event("%s: save %d / DC %d; %d damage" % [data.name, save.total, target_dc, damage])
	game.effects.pulse(hero.position, 0.7, Color(rules.data.color), 0.3)
	return true

func apply_status(target: Combatant, data: Dictionary) -> void:
	for effect in data.statusEffects:
		match effect:
			"restrained": target.root_time = data.duration
			"marked": target.marked_time = data.duration
			"weaken": target.weakened_time = data.duration
			"slow": target.slow_time = data.duration
			"push":
				var direction: Vector3 = (target.position - game.player.position).normalized()
				for i in range(10):
					if game.navigation.walkable(target.position + direction * 0.3):
						target.position += direction * 0.3
