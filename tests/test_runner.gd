extends SceneTree
var failures := 0
var checks := 0

func _initialize() -> void:
	call_deferred("run")

func check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error("FAIL: " + label)
	else:
		print("PASS: " + label)

func run() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 912
	check(CombatRules.modifier(9) == -1 and CombatRules.modifier(16) == 3, "attribute modifiers round down")
	check(CombatRules.proficiency(1) == 2 and CombatRules.proficiency(5) == 3 and CombatRules.proficiency(17) == 6, "proficiency progression")
	var critical_seen := false
	var one_seen := false
	for i in range(500):
		var result := CombatRules.attack(rng, 100, 999, "1d1", 3)
		if result.roll == 20:
			critical_seen = result.hit and result.damage == 5
		if result.roll == 1:
			one_seen = not result.hit
	check(critical_seen and one_seen, "natural 20 auto-hits and doubles dice; natural 1 misses")
	var high := RandomNumberGenerator.new()
	var low := RandomNumberGenerator.new()
	high.seed = 10
	low.seed = 10
	check(CombatRules.d20(high, 1) >= CombatRules.d20(low, -1), "advantage and disadvantage select opposing dice")
	var nav := CourtyardNavigation.new()
	nav.block(Vector3.ZERO, Vector3(2, 2, 6))
	var route := nav.path(Vector3(-5, 0, 0), Vector3(5, 0, 0))
	var clear := not route.is_empty()
	for point in route:
		clear = clear and nav.walkable(point)
	check(clear and route.size() > 10, "navigation routes around blocked footprints")
	var blocked_route := nav.path(Vector3(-5, 0, 0), Vector3.ZERO)
	check(not blocked_route.is_empty() and nav.walkable(blocked_route[-1]), "blocked destination resolves to reachable ground")
	var game: Node3D = load("res://scenes/main.tscn").instantiate()
	game.skip_creation = true
	root.add_child(game)
	current_scene = game
	await process_frame
	game.rng.seed = 66
	check(game.abilities.slots.size() == 4, "four data-driven abilities load")
	game.enemy.set_physics_process(false)
	game.player.command_move(Vector3(0, 0, 11))
	await create_timer(0.8).timeout
	check(game.player.position.distance_to(Vector3(0, 0, 11)) < 0.5, "character reaches commanded destination")
	game.player.path.clear()
	game.player.velocity = Vector3.ZERO
	check(game.abilities.cast(1, Vector3.ZERO), "barrier casts")
	game.player.receive_damage(10)
	check(game.player.health == 100 and game.player.temporary_hp == 14, "temporary HP absorbs damage before health")
	check(not game.abilities.cast(1, Vector3.ZERO), "cooldown prevents repeated cast")
	game.player.mana = 0
	check(not game.abilities.cast(3, Vector3.ZERO), "insufficient resource prevents cast")
	game.player.mana = 100
	game.player.position = Vector3(0, 0, 1)
	game.enemy.position = Vector3(0, 0, -2)
	check(game.abilities.cast(2, game.enemy.position), "ground ability casts in range")
	check(game.enemy.health < game.enemy.max_health, "ground ability resolves saving throw damage")
	game.enemy.health = game.enemy.max_health
	check(game.abilities.cast(0, game.enemy.position), "projectile casts")
	await create_timer(0.4).timeout
	check(game.get_children().filter(func(n: Node): return n is EmberProjectile).is_empty(), "projectile terminates on enemy collision")
	game.abilities.cooldowns.dawnbreak = 0.0
	check(game.abilities.cast(3, game.player.position), "radial burst casts")
	var health_before: int = game.enemy.health
	game.player.target = game.enemy
	game.player.attack_timer = 0
	await create_timer(1.5).timeout
	check(game.player.position.distance_to(game.enemy.position) <= 2.3 and game.enemy.health < health_before, "auto-attack approaches then damages target")
	game.player.target = null
	game.player.path.clear()
	game.enemy.health = 120
	game.enemy.state = "chase"
	game.enemy.stagger_time = 0
	game.enemy.root_time = 0
	game.enemy.set_physics_process(true)
	game.player.position = game.enemy.position + Vector3(0, 0, 1.3)
	await create_timer(0.1).timeout
	check(game.enemy.state == "windup" and game.enemy.telegraph.visible, "melee AI telegraphs before striking")
	game.player.position += Vector3(5, 0, 0)
	var hp_before: int = game.player.health
	await create_timer(0.75).timeout
	check(game.player.health == hp_before, "leaving telegraph avoids the attack")
	game.enemy.set_physics_process(false)
	game.toggle_pause()
	var cd_before: float = game.abilities.cooldowns.cinder_guard
	await create_timer(0.15, true).timeout
	check(is_equal_approx(game.abilities.cooldowns.cinder_guard, cd_before), "pause freezes cooldowns")
	game.toggle_pause()
	game.enemy.receive_damage(999)
	check(game.enemy.dead and is_instance_valid(game.loot), "enemy death spawns one loot item")
	game.player.position = game.loot_point + Vector3(0, 0, 1)
	var screen: Vector2 = game.camera_rig.camera.unproject_position(game.loot.global_position + Vector3(0, 0.5, 0))
	check(game.try_collect(screen) and "ember_shard" in game.player.inventory, "nearby loot collection updates inventory")
	check(not game.try_collect(screen) and game.player.inventory.size() == 1, "same-frame repeat click cannot duplicate loot")
	game.player.temporary_hp = 0
	game.player.receive_damage(999)
	check(game.player.dead and game.player.health == 0, "player death enters zero-health state")
	game.restart()
	await process_frame
	await process_frame
	check(current_scene != game and current_scene.player.health == 100 and current_scene.player.inventory.is_empty(), "restart creates a fresh playable encounter")
	print("RESULT: %d/%d checks passed" % [checks - failures, checks])
	current_scene.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
