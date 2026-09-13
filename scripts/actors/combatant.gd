class_name Combatant
extends CharacterBody3D

signal hurt(amount: int, critical: bool)
signal died

var game: Node3D
var health := 100
var max_health := 100
var armor := 12
var speed := 6.0
var dead := false
var temporary_hp := 0
var barrier_time := 0.0
var root_time := 0.0
var stagger_time := 0.0
var attack_timer := 0.0
var path := PackedVector3Array()
var visual: Node3D
var weapon: Node3D
var ring: MeshInstance3D
var shield: MeshInstance3D
var time := 0.0
var attack_pose := 0.0
var is_enemy := false
var marked_time := 0.0
var weakened_time := 0.0
var slow_time := 0.0
var legs: Array[Node3D] = []

func build_model(enemy: bool) -> void:
	is_enemy = enemy
	collision_layer = 4 if enemy else 2
	collision_mask = 1
	var shape := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.42
	capsule.height = 1.8
	shape.shape = capsule
	shape.position.y = 0.9
	add_child(shape)
	visual = Node3D.new()
	add_child(visual)
	var cloth := Color("41585b") if enemy else Color("244e60")
	var steel := Color("695a48") if enemy else Color("a9b8b0")
	for side in [-1, 1]:
		var leg := Node3D.new()
		leg.position = Vector3(side * 0.22, 0.6, 0)
		visual.add_child(leg)
		Geometry.box(leg, Vector3(0, -0.28, 0), Vector3(0.25, 0.58, 0.33), Color("252e31"))
		Geometry.box(leg, Vector3(0, -0.49, -0.09), Vector3(0.28, 0.18, 0.46), steel)
		legs.append(leg)
	Geometry.cylinder(visual, Vector3(0, 0.96, 0), 0.44, 0.78, cloth, 0.33)
	Geometry.box(visual, Vector3(0, 1.13, -0.23), Vector3(0.55, 0.56, 0.16), steel)
	Geometry.box(visual, Vector3(0, 1.04, 0.26), Vector3(0.76, 1.04, 0.12), Color("302e28") if enemy else Color("923c31"))
	Geometry.cylinder(visual, Vector3(0, 1.65, 0), 0.27, 0.43, steel, 0.23)
	Geometry.box(visual, Vector3(0, 1.67, -0.244), Vector3(0.34, 0.075, 0.05), Color("ff7149") if enemy else Color("85eeef"))
	for side in [-1, 1]:
		Geometry.box(visual, Vector3(side * 0.47, 1.28, 0), Vector3(0.32, 0.3, 0.42), steel)
		if enemy:
			var thorn := Geometry.cylinder(visual, Vector3(side * 0.43, 1.73, 0), 0.13, 0.95, Color("a39168"), 0)
			thorn.rotation.z = side * -0.6
	weapon = Node3D.new()
	weapon.position = Vector3(0.56, 1.1, -0.2)
	visual.add_child(weapon)
	Geometry.box(weapon, Vector3(0, 0, -0.56), Vector3(0.14, 0.12, 1.4), steel)
	Geometry.box(weapon, Vector3(0, 0, -0.04), Vector3(0.48, 0.14, 0.12), Color("d6ae65"))
	ring = Geometry.ring(self, Vector3(0, 0.06, 0), 0.65, Color("bc553e") if enemy else Color("74cfc7"), 0.035)
	shield = Geometry.ring(self, Vector3(0, 0.8, 0), 0.95, Color("7bdfed"))
	shield.visible = false

func tick(delta: float) -> void:
	time += delta
	marked_time = maxf(0, marked_time - delta)
	weakened_time = maxf(0, weakened_time - delta)
	slow_time = maxf(0, slow_time - delta)
	attack_timer = maxf(0, attack_timer - delta)
	root_time = maxf(0, root_time - delta)
	stagger_time = maxf(0, stagger_time - delta)
	barrier_time = maxf(0, barrier_time - delta)
	if barrier_time <= 0:
		temporary_hp = 0
	shield.visible = temporary_hp > 0 and not dead
	shield.rotation.z = sin(time * 2) * 0.15
	attack_pose = maxf(0, attack_pose - delta * 3.0)
	weapon.rotation.y = sin(attack_pose * PI) * -2.4
	if dead:
		return
	var moving := Vector2(velocity.x, velocity.z).length() / speed
	visual.position.y = absf(sin(time * 11)) * 0.07 * moving
	legs[0].rotation.x = sin(time * 11) * 0.65 * moving
	legs[1].rotation.x = -sin(time * 11) * 0.65 * moving

func apply_appearance(stats: Dictionary) -> void:
	# Shared by the creator preview and the playable actor.
	visual.scale = Vector3.ONE * float(stats.scale)
	for child in weapon.get_children():
		child.queue_free()
	var color := Color(stats.color)
	for child in visual.get_children():
		if child is MeshInstance3D and (is_equal_approx(child.position.y, 0.96) or is_equal_approx(child.position.y, 1.04)):
			child.material_override = Geometry.material(color)
	match stats.weapon:
		"staff":
			Geometry.cylinder(weapon, Vector3(0, 0, -0.3), 0.07, 1.9, Color("8f7455"))
			Geometry.cylinder(weapon, Vector3(0, 0.95, -0.3), 0.16, 0.4, color, 0)
		"longbow":
			var bow := Geometry.ring(weapon, Vector3.ZERO, 0.6, Color("bd9865"), 0.035)
			bow.rotation.x = PI / 2
			bow.scale.x = 0.4
		"unarmed":
			Geometry.box(weapon, Vector3.ZERO, Vector3(0.2, 0.2, 0.2), Color("cfb891"))
		"greataxe":
			Geometry.box(weapon, Vector3(0, 0, -0.5), Vector3(0.12, 0.12, 1.4), Color("8f7455"))
			Geometry.box(weapon, Vector3(0, 0, -1.05), Vector3(0.7, 0.12, 0.4), Color("c1c7bd"))
		"mace":
			Geometry.box(weapon, Vector3(0, 0, -0.4), Vector3(0.12, 0.12, 1.0), Color("8f7455"))
			Geometry.box(weapon, Vector3(0, 0, -0.8), Vector3(0.32, 0.32, 0.32), Color("c1c7bd"))
		_:
			Geometry.box(weapon, Vector3(0, 0, -0.56), Vector3(0.07 if stats.weapon == "rapier" else 0.16, 0.10, 1.4), Color("c1c7bd"))
			Geometry.box(weapon, Vector3(0, 0, -0.04), Vector3(0.48, 0.14, 0.12), Color("d6ae65"))

func go_to(destination: Vector3) -> void:
	path = game.navigation.path(global_position, destination)

func move_along(delta: float, hold: bool = false) -> void:
	var desired := Vector3.ZERO
	if not path.is_empty() and not hold and root_time <= 0 and stagger_time <= 0:
		var offset := path[0] - global_position
		offset.y = 0
		if offset.length() < 0.22:
			path.remove_at(0)
		else:
			desired = offset.normalized() * minf(speed * (0.667 if slow_time > 0 else 1.0), offset.length() / delta)
	velocity = velocity.move_toward(desired, 45 * delta)
	move_and_slide()
	global_position.y = 0
	if desired.length_squared() > 0.1:
		face(global_position + desired, delta * 16)

func face(point: Vector3, weight: float = 1) -> void:
	var direction := point - global_position
	if direction.length_squared() > 0.001:
		visual.rotation.y = lerp_angle(visual.rotation.y, atan2(-direction.x, -direction.z), minf(1, weight))

func receive_damage(amount: int, critical: bool = false, _damage_type: String = "physical", _attacker: Combatant = null) -> void:
	if dead:
		return
	var absorbed := mini(temporary_hp, amount)
	temporary_hp -= absorbed
	amount -= absorbed
	health = maxi(0, health - amount)
	hurt.emit(amount, critical)
	game.effects.number(global_position + Vector3(0, 2.2, 0), str(amount) + ("!" if critical else ""), Color("ffc26d") if is_enemy else Color("ff7266"), critical)
	game.effects.pulse(global_position + Vector3(0, 0.7, 0), 0.6, Color("ffc880"), 0.2)
	if absorbed > 0:
		game.effects.number(global_position + Vector3(0, 2.7, 0), "BLOCK " + str(absorbed), Color("8adfea"))
	if health == 0:
		dead = true
		path.clear()
		velocity = Vector3.ZERO
		collision_layer = 0
		ring.visible = false
		shield.visible = false
		var tween := create_tween()
		tween.tween_property(visual, "rotation:z", -1.4, 0.4).set_trans(Tween.TRANS_QUAD)
		died.emit()
