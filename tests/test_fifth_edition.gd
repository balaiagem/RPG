extends SceneTree
var failures := 0
var checks := 0
var game: Node3D

func _initialize() -> void:
	call_deferred("run")

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + label)
	else:
		print("PASS: " + label)

func build(id: String, ancestry: String = "human") -> void:
	var choice := CharacterBuild.default_choice(id)
	choice.ancestry = ancestry
	game.apply_build(choice)
	game.player.position = Vector3(0, 0, 0)
	game.player.target = null
	game.player.path.clear()
	game.player.temporary_hp = 0
	game.enemy.position = Vector3(0, 0, -1.5)
	game.enemy.dead = false
	game.enemy.health = 24
	game.enemy.collision_layer = 4
	game.enemy.root_time = 0
	game.enemy.marked_time = 0
	game.enemy.state = "idle"
	game.enemy.set_physics_process(false)
	game.rng.seed = 719
	await process_frame

func cast(id: String) -> bool:
	return game.abilities.cast(game.abilities.slots.find(id), game.enemy.position)

func run() -> void:
	var classes := CharacterBuild.catalog("classes_5e")
	check(classes.size() == 12, "all twelve classes exist")
	for id in classes:
		var choice := CharacterBuild.default_choice(id)
		check(CharacterBuild.validate(choice).is_empty(), "%s default selection validates" % id)
		var data := CharacterBuild.derive(choice)
		check(data.abilities.size() == 4 and data.hp > 0 and data.armorClass >= 10, "%s derives a complete playable loadout" % id)
	var bad := CharacterBuild.default_choice()
	bad.scores.strength = 20
	check(not CharacterBuild.validate(bad).is_empty(), "illegal attribute assignment rejected")
	bad = CharacterBuild.default_choice()
	bad.skills[1] = bad.skills[0]
	check(not CharacterBuild.validate(bad).is_empty(), "duplicate skill proficiency rejected")
	var save_path := "user://test_character_roundtrip.json"
	var choice := CharacterBuild.default_choice("wizard")
	check(CharacterBuild.save_choice(choice, save_path) == OK, "versioned character save writes")
	choice.name = "Test wizard"
	check(CharacterBuild.save_choice(choice, save_path) == OK and CharacterBuild.load_choice(save_path).name == "Test wizard", "existing character save is atomically replaced and loaded")
	var file := FileAccess.open(save_path, FileAccess.WRITE)
	file.store_string("{broken")
	file.close()
	check(CharacterBuild.load_choice(save_path).is_empty(), "corrupt character save safely falls back")
	DirAccess.remove_absolute(save_path)
	game = load("res://scenes/main.tscn").instantiate()
	game.skip_creation = true
	root.add_child(game)
	current_scene = game
	await process_frame
	await build("fighter")
	game.player.health = 1
	check(cast("second_wind") and game.player.health > 1, "Second Wind heals with a bonus action")
	check(game.player.rules.resources.actions.action == 0 and game.player.rules.resources.actions.bonus > 0, "bonus action does not spend action")
	check(game.player.rules.weapon_attack(game.enemy), "action remains available after Second Wind")
	check(not game.player.rules.weapon_attack(game.enemy), "six-second action gate prevents extra basic attack")
	game.player.rules.resources.actions.bonus = 0
	check(not cast("second_wind"), "Second Wind cannot exceed its rest pool")
	game.enemy.state = "idle"
	game.player.rules.rest(false)
	check(game.player.rules.resources.pools.second_wind == 1, "short rest restores Second Wind")
	await build("barbarian")
	check(cast("rage"), "Rage activates")
	var hp: int = game.player.health
	game.player.receive_damage(7)
	check(game.player.health == hp - 3, "Rage halves physical damage, rounding down")
	await build("rogue")
	game.player.position = Vector3(0, 0, 9)
	check(cast("hide"), "Rogue can attempt Hide while distant")
	game.player.rules.resources.hidden = true
	game.player.rules.resources.actions.action = 0
	game.player.position = Vector3.ZERO
	game.enemy.armor = 1
	check(game.player.rules.weapon_attack(game.enemy) and game.player.rules.resources.sneak_time > 0, "advantage enables once-per-round Sneak Attack")
	game.enemy.armor = 12
	await build("monk")
	check(not cast("martial_strike"), "Martial Arts requires an Attack action")
	game.player.rules.weapon_attack(game.enemy)
	check(cast("martial_strike"), "Martial Arts follows an Attack using the bonus action")
	await build("paladin")
	game.player.health = 1
	check(cast("lay_on_hands") and game.player.health == 6 and game.player.rules.resources.pools.lay_hands == 0, "Lay on Hands spends exactly five healing points")
	await build("wizard")
	var slots: int = game.player.rules.resources.slots
	check(cast("fire_bolt") and game.player.rules.resources.slots == slots, "cantrip spends no spell slot")
	game.player.rules.resources.actions.action = 0
	check(cast("magic_missile") and game.player.rules.resources.slots == slots - 1, "leveled spell consumes a slot")
	game.player.rules.resources.actions.action = 0
	game.player.rules.resources.slots = 0
	check(not cast("magic_missile"), "empty slot pool blocks leveled spell")
	game.enemy.state = "idle"
	game.player.rules.rest(false)
	check(game.player.rules.resources.slots == 1, "Arcane Recovery restores one slot on short rest")
	game.player.rules.resources.slots = 0
	game.player.rules.rest(false)
	check(game.player.rules.resources.slots == 0, "Arcane Recovery is limited to once per long rest")
	await build("warlock")
	check(cast("armor_of_agathys") and game.player.temporary_hp == 5, "Armor of Agathys grants five temporary HP")
	var enemy_hp: int = game.enemy.health
	game.player.receive_damage(1, false, "physical", game.enemy)
	check(game.enemy.health == enemy_hp - 5, "Agathys retaliates against melee damage")
	game.enemy.state = "idle"
	game.player.rules.rest(false)
	check(game.player.rules.resources.slots == 1, "Pact Magic slot recovers on short rest")
	await build("bard")
	check(is_instance_valid(game.companion) and cast("bardic_inspiration") and game.companion.inspiration > 0, "Bardic Inspiration affects a real ally")
	await build("cleric")
	game.player.health = 1
	check(cast("cure_wounds") and game.player.health >= 8, "Life cleric adds casting modifier and Disciple of Life healing")
	await build("fighter", "half_orc")
	game.player.receive_damage(game.player.health)
	check(not game.player.dead and game.player.health == 1, "Relentless Endurance prevents one non-massive knockout")
	game.player.receive_damage(1)
	check(game.player.dead and game.player.rules.downed, "second knockout starts death saves")
	game.player.rules.revive()
	check(not game.player.dead and game.player.health == 1, "natural-20 revival restores playable state")
	await build("sorcerer", "tiefling")
	hp = game.player.health
	game.player.receive_damage(7, false, "fire")
	check(game.player.health == hp - 3, "Tiefling fire resistance halves fire damage")
	check(not game.dice.history.is_empty(), "actual combat outcomes are delivered to the on-screen d20")
	var outcome: Dictionary = game.dice.history[-1]
	check(outcome.has("roll") and outcome.has("total") and outcome.has("target"), "dice presentation includes natural roll, total and target")
	game.enemy.state = "chase"
	check(not game.player.rules.rest(true), "rest is blocked while an enemy is alert")
	# Exercise each offered spell through the same loadout/cast route as the UI.
	for class_id in classes:
		for spell_id in classes[class_id].spellChoices:
			await build(class_id)
			var selection := CharacterBuild.default_choice(class_id)
			var level: int = CharacterBuild.catalog("abilities_5e")[spell_id].level
			selection.spells[level] = spell_id
			game.apply_build(selection)
			game.player.health = maxi(1, game.player.max_health - 5)
			await process_frame
			check(cast(spell_id), "%s can cast offered %s" % [class_id, spell_id])
	await build("cleric")
	var faith := CharacterBuild.default_choice("cleric")
	faith.spells[1] = "shield_of_faith"
	game.apply_build(faith)
	var base_ac: int = game.player.armor
	check(cast("shield_of_faith"), "concentration spell can begin")
	await process_frame
	check(game.player.armor == base_ac + 2, "Shield of Faith adds two AC while concentrating")
	game.player.rules.break_concentration()
	await process_frame
	check(game.player.armor == base_ac, "ending concentration removes the AC bonus")
	await build("druid")
	var entangle := CharacterBuild.default_choice("druid")
	entangle.spells[1] = "entangle"
	game.apply_build(entangle)
	cast("entangle")
	check(game.player.rules.resources.concentration == "entangle", "Entangle owns the concentration channel")
	game.enemy.root_time = 60
	game.player.rules.break_concentration()
	check(game.enemy.root_time == 0, "breaking Entangle concentration releases the restrained target")
	# Verify actual creator controls, rather than only the build generator.
	var creator := CharacterCreator.new()
	root.add_child(creator)
	creator.choice = CharacterBuild.default_choice("wizard")
	creator.step = 2
	creator.redraw_page()
	var options := creator.content.get_children().filter(func(n: Node): return n is OptionButton)
	options[0].item_selected.emit(0)
	check(int(creator.choice.scores.strength) == 15 and CharacterBuild.validate(creator.choice).is_empty(), "attribute UI swaps a selected value without duplicating the array")
	creator.step = 3
	creator.choice.skills.clear()
	creator.redraw_page()
	creator.next_step()
	check(creator.step == 3 and not creator.error_label.text.is_empty(), "creator prevents advancing with missing skill choices")
	creator.queue_free()
	print("RESULT: %d/%d fifth-edition checks passed" % [checks - failures, checks])
	game.queue_free()
	await process_frame
	await create_timer(0.3).timeout
	quit(1 if failures else 0)
