class_name EmberProjectile
extends Node3D
var game: Node3D
var data: Dictionary
var direction: Vector3
var remaining := 0.0
var trail_timer := 0.0

func setup(owner_game: Node3D, origin: Vector3, target: Vector3, definition: Dictionary) -> void:
	game = owner_game
	data = definition
	position = origin
	direction = (target - origin).normalized()
	if direction.length_squared() < 0.01:
		direction = Vector3.FORWARD
	remaining = data.range
	var ball := SphereMesh.new()
	ball.radius = 0.19
	ball.height = 0.38
	Geometry.mesh(self, ball, Vector3.ZERO, Color("ffa565"), 3)
	var light := OmniLight3D.new()
	light.light_color = Color("ff944e")
	light.light_energy = 2
	light.omni_range = 3
	add_child(light)

func _physics_process(delta: float) -> void:
	var step := direction * minf(float(data.projectileSpeed) * delta, remaining)
	var query := PhysicsRayQueryParameters3D.create(global_position, global_position + step, 5)
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if not hit.is_empty():
		if hit.collider is Combatant and not hit.collider.dead:
			var result := CombatRules.attack(game.rng, 5, hit.collider.armor, data.damageDice, 0)
			game.resolve_attack(game.player, hit.collider, result, data.name)
		game.effects.pulse(hit.position, 0.85, Color("ffc07c"), 0.3)
		queue_free()
		return
	position += step
	remaining -= step.length()
	trail_timer -= delta
	if trail_timer <= 0:
		game.effects.pulse(position, 0.22, Color("d47642"), 0.2)
		trail_timer = 0.05
	if remaining <= 0:
		queue_free()
