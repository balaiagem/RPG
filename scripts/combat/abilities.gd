class_name AbilityController
extends Node

var game: Node3D
var slots: Array = []
var cooldowns: Dictionary = {}
var handlers: Dictionary = {}

func setup(owner_game: Node3D) -> void:
	game = owner_game
	slots = game.characters.player.abilities.duplicate()
	handlers = {"projectile":_projectile,"barrier":_barrier,"ground":_ground,"burst":_burst}
	for id in slots:
		cooldowns[id] = 0.0

func _process(delta: float) -> void:
	for id in cooldowns:
		cooldowns[id] = maxf(0, cooldowns[id] - delta)

func cast(slot: int, point: Vector3) -> bool:
	var player: Warden = game.player
	var id: String = slots[slot]
	var data: Dictionary = game.ability_data[id]
	if player.dead or player.stagger_time > 0:
		return false
	if cooldowns[id] > 0:
		game.hud.notify(data.name + " is recovering")
		return false
	if player.mana < float(data.manaCost):
		game.hud.notify("Not enough focus")
		return false
	var offset := point - player.global_position
	if float(data.range) > 0:
		point = player.global_position + offset.limit_length(float(data.range))
	if data.handler == "ground" and not game.navigation.walkable(point):
		game.hud.notify("Choose clear ground")
		return false
	player.mana -= data.manaCost
	cooldowns[id] = float(data.cooldown)
	player.face(point)
	player.attack_pose = 1
	handlers[data.handler].call(data, point)
	game.effects.sound(data.audioEffect, player.global_position)
	return true

func _projectile(data: Dictionary, point: Vector3) -> void:
	var projectile := EmberProjectile.new()
	game.add_child(projectile)
	projectile.setup(game, game.player.global_position + Vector3(0, 0.85, 0), point + Vector3(0, 0.85, 0), data)

func _barrier(data: Dictionary, _point: Vector3) -> void:
	game.player.temporary_hp = 24
	game.player.barrier_time = data.duration
	game.effects.pulse(game.player.global_position, 1.5, Color("79dfe9"), 0.5)

func _ground(data: Dictionary, point: Vector3) -> void:
	game.effects.pulse(point, data.radius, Color("efa865"), 0.65)
	area_damage(data, point)

func _burst(data: Dictionary, _point: Vector3) -> void:
	game.effects.pulse(game.player.global_position, data.radius, Color("ffe2a1"), 0.7)
	game.camera_rig.shake = 0.22
	area_damage(data, game.player.global_position)

func area_damage(data: Dictionary, point: Vector3) -> void:
	for enemy in game.enemies:
		if enemy.dead or enemy.global_position.distance_to(point) > float(data.radius):
			continue
		var score: int = enemy.definition.attributes.get(data.savingThrow, 10)
		var save := CombatRules.saving_throw(game.rng, score, 13)
		var damage := CombatRules.dice(data.damageDice, game.rng)
		if save.success:
			damage = floori(damage / 2.0)
		else:
			if "root" in data.statusEffects:
				enemy.root_time = data.duration
			if "stagger" in data.statusEffects:
				enemy.stagger_time = data.duration
		enemy.receive_damage(damage)
		game.hud.log_event("%s • save %d vs 13 • %d damage" % [data.name, save.total, damage])
