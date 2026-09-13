class_name Warden
extends Combatant

var mana := 100.0
var target: Combatant
var repath := 0.0
var potion_count := 2
var dodge_cooldown := 0.0
var inventory: Array[String] = []
var weapon_data: Dictionary
var rules: HeroRules

func setup(owner_game: Node3D) -> void:
	game = owner_game
	var data: Dictionary = game.characters.player
	health = data.hp
	max_health = health
	armor = data.armorClass
	speed = data.speed
	weapon_data = game.items[data.weapon]
	build_model(false)

func _physics_process(delta: float) -> void:
	tick(delta)
	if dead:
		return
	mana = minf(100, mana + delta * 4)
	dodge_cooldown = maxf(0, dodge_cooldown - delta)
	repath -= delta
	var hold := Input.is_physical_key_pressed(KEY_SHIFT)
	if is_instance_valid(target) and not target.dead:
		var distance := global_position.distance_to(target.global_position)
		if distance <= float(weapon_data.attackRange):
			path.clear()
			face(target.global_position, delta * 18)
			if attack_timer <= 0 and stagger_time <= 0:
				basic_attack()
		elif not hold and repath <= 0:
			go_to(target.global_position)
			repath = 0.2
	else:
		target = null
	move_along(delta, hold)

func command_move(point: Vector3) -> void:
	target = null
	go_to(point)

func basic_attack() -> void:
	if is_instance_valid(rules):
		rules.weapon_attack(target)
		return
	attack_timer = weapon_data.attackSpeed
	attack_pose = 1.0
	var attributes: Dictionary = game.characters.player.attributes
	var bonus := CombatRules.modifier(attributes[weapon_data.statScaling]) + CombatRules.proficiency(game.characters.player.level)
	var result := CombatRules.attack(game.rng, bonus, target.armor, weapon_data.damageDice, weapon_data.damageBonus)
	game.resolve_attack(self, target, result, "Blade")
	game.effects.pulse(global_position + Vector3(0, 0.4, 0), 1.7, Color("b4e0d0"), 0.23)

func drink_potion() -> void:
	if dead or potion_count <= 0 or health == max_health:
		return
	if is_instance_valid(rules):
		if not rules.resources.available("action"):
			return
		rules.resources.spend("action")
		potion_count -= 1
		var healing := CombatRules.dice("2d4", game.rng) + 2
		health = mini(max_health, health + healing)
		game.effects.number(global_position + Vector3(0, 2.2, 0), "+%d HP" % healing, Color("8de1a4"))
		return
	potion_count -= 1
	health = mini(max_health, health + 45)
	game.effects.number(global_position + Vector3(0, 2.2, 0), "+45 HP", Color("8de1a4"))
	game.effects.pulse(global_position, 1.4, Color("8de1a4"), 0.5)

func dodge(point: Vector3) -> void:
	if is_instance_valid(rules):
		if not dead and rules.resources.available("action"):
			rules.resources.spend("action")
			rules.resources.dodge_time = 6
			game.hud.notify("Dodge — incoming attacks have disadvantage for 6 seconds")
		return
	if dead or dodge_cooldown > 0 or root_time > 0 or stagger_time > 0:
		return
	var direction := (point - global_position).normalized()
	var destination := global_position
	for i in range(1, 13):
		var candidate := global_position + direction * i * 0.25
		if not game.navigation.walkable(candidate):
			break
		destination = candidate
	game.effects.pulse(global_position, 0.8, Color("80d7d0"), 0.3)
	global_position = destination
	path.clear()
	dodge_cooldown = 3

func receive_damage(amount: int, critical: bool = false, damage_type: String = "physical", attacker: Combatant = null) -> void:
	if dead:
		return
	if is_instance_valid(rules):
		amount = rules.incoming(amount, critical, damage_type, attacker)
	var massive := amount - temporary_hp - health >= max_health
	super.receive_damage(amount, critical, damage_type, attacker)
	if dead and is_instance_valid(rules):
		rules.on_downed()
		if massive:
			rules.downed = false
			rules.death_failures = 3

func apply_character(stats: Dictionary) -> void:
	if is_instance_valid(rules):
		rules.queue_free()
	health = int(stats.hp)
	max_health = health
	armor = int(stats.armorClass)
	speed = float(stats.speed)
	weapon_data = CharacterBuild.weapon(stats.weapon)
	dead = false
	collision_layer = 2
	visual.rotation.z = 0
	visual.scale = Vector3.ONE * float(stats.scale)
	rules = HeroRules.new()
	add_child(rules)
	rules.configure(game, stats)
	apply_appearance(stats)
