class_name IsometricCamera
extends Node3D
var camera: Camera3D
var target: Node3D
var zoom := 1.0
var angle := 0.0
var shake := 0.0

func _ready() -> void:
	camera = Camera3D.new()
	camera.fov = 43
	camera.near = 0.1
	camera.far = 160
	add_child(camera)
	camera.current = true

func _process(delta: float) -> void:
	if not is_instance_valid(target):
		return
	global_position = global_position.lerp(target.global_position, 1.0 - exp(-6 * delta))
	var offset := Vector3(14, 20, 17).rotated(Vector3.UP, angle) * zoom
	camera.position = camera.position.lerp(offset, 1.0 - exp(-10 * delta))
	camera.look_at(global_position + Vector3(0, 0.5, 0))
	shake = maxf(0, shake - delta)
	if shake > 0:
		camera.h_offset = sin(Time.get_ticks_msec() * 0.11) * shake * 0.16
		camera.v_offset = cos(Time.get_ticks_msec() * 0.09) * shake * 0.12
	else:
		camera.h_offset = 0
		camera.v_offset = 0

func ground_point(screen: Vector2) -> Vector3:
	var origin := camera.project_ray_origin(screen)
	var direction := camera.project_ray_normal(screen)
	var hit: Variant = Plane(Vector3.UP, 0).intersects_ray(origin, direction)
	return hit if hit != null else target.global_position

func pick(screen: Vector2) -> Dictionary:
	var origin := camera.project_ray_origin(screen)
	var query := PhysicsRayQueryParameters3D.create(origin, origin + camera.project_ray_normal(screen) * 200, 4)
	return get_world_3d().direct_space_state.intersect_ray(query)
