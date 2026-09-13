class_name WayfarerCompanion
extends Combatant
## Allied sparring partner: Bardic Inspiration never targets the bard themselves.
var inspiration := 0.0
var repath := 0.0

func setup(owner_game: Node3D) -> void:
	game = owner_game
	health = 10
	max_health = 10
	speed = 5
	build_model(false)
	ring.material_override = Geometry.material(Color("c7a7df"), 1)
	var label := Label3D.new()
	label.text = "WAYFARER · ALLY"
	label.position.y = 2.4
	label.font_size = 28
	label.pixel_size = 0.008
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	add_child(label)

func _physics_process(delta: float) -> void:
	tick(delta)
	inspiration = maxf(0, inspiration - delta)
	repath -= delta
	if dead or game.player.dead:
		return
	var foe: Thornbound = game.enemy
	if not foe.dead and foe.state != "idle":
		if position.distance_to(foe.position) <= 2.1:
			path.clear()
			face(foe.position)
			if attack_timer <= 0:
				attack_timer = 6
				attack_pose = 1
				var result := CombatRules.attack(game.rng, 3, foe.armor, "1d6", 1)
				if inspiration > 0 and not result.hit and result.roll != 1:
					var bonus: int = game.rng.randi_range(1, 6)
					result.total += bonus
					result.hit = result.total >= foe.armor
					result.damage = CombatRules.dice("1d6", game.rng) + 1 if result.hit else 0
					inspiration = 0
					game.hud.log_event("Bardic Inspiration added +%d to ally's attack" % bonus)
				game.resolve_attack(self, foe, result, "Wayfarer attack")
		elif repath <= 0:
			go_to(foe.position)
			repath = 0.4
	elif position.distance_to(game.player.position) > 3 and repath <= 0:
		go_to(game.player.position + Vector3(1.5, 0, 1.5))
		repath = 0.5
	move_along(delta)
