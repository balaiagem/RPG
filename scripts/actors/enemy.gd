class_name Thornbound
extends Combatant
## One reusable melee behavior: idle -> chase -> telegraph -> strike -> recover.
var home := Vector3.ZERO
var state := "idle"
var timer := 0.0
var repath := 0.0
var strike_point := Vector3.ZERO
var telegraph: MeshInstance3D
var definition: Dictionary

func setup(owner_game: Node3D) -> void:
	game = owner_game
	definition = game.characters.enemy
	health = definition.hp
	max_health = health
	armor = definition.armorClass
	speed = definition.speed
	build_model(true)
	home = position
	telegraph = Geometry.ring(game, Vector3.ZERO, 1.9, Color("ff5c42"), 0.08)
	telegraph.visible = false
	hurt.connect(func(_amount: int, _critical: bool):
		if state == "idle":
			state = "chase")
	died.connect(func():
		state = "dead"
		telegraph.visible = false
		game.spawn_loot(global_position))

func _physics_process(delta: float) -> void:
	tick(delta)
	if dead:
		return
	if game.player.dead:
		telegraph.visible = false
		path.clear()
		return
	if is_instance_valid(game.player.rules) and game.player.rules.resources.hidden:
		path.clear()
		move_along(delta, true)
		return
	if stagger_time > 0:
		telegraph.visible = false
		state = "chase"
		move_along(delta, true)
		return
	var distance := global_position.distance_to(game.player.global_position)
	repath -= delta
	match state:
		"idle":
			if distance < float(definition.aggroRange):
				state = "chase"
		"chase":
			if global_position.distance_to(home) > float(definition.leashRange):
				state = "return"
				go_to(home)
			elif distance <= float(definition.attackRange) and attack_timer <= 0:
				state = "windup"
				timer = definition.windup
				path.clear()
				strike_point = game.player.global_position
				telegraph.position = strike_point + Vector3(0, 0.09, 0)
				telegraph.visible = true
				face(strike_point)
			elif repath <= 0 and distance > float(definition.attackRange):
				go_to(game.player.global_position)
				repath = 0.35
		"windup":
			timer -= delta
			telegraph.scale = Vector3.ONE * (0.8 + 0.2 * sin(timer * 20))
			if timer <= 0:
				telegraph.visible = false
				attack_pose = 1
				attack_timer = definition.cooldown
				game.effects.pulse(strike_point, 1.9, Color("ff6a43"), 0.3)
				if game.player.global_position.distance_to(strike_point) < 1.9:
					var disadvantage := -1 if weakened_time > 0 or root_time > 0 or (is_instance_valid(game.player.rules) and game.player.rules.resources.dodge_time > 0) else 0
					var result := CombatRules.attack(game.rng, definition.attackBonus, game.player.armor, definition.damageDice, definition.damageBonus, disadvantage)
					weakened_time = 0
					game.resolve_attack(self, game.player, result, "Thorn cleave")
				state = "chase"
		"return":
			if path.is_empty():
				health = max_health
				state = "idle"
	move_along(delta, state == "windup")
