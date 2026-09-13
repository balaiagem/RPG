class_name CourtyardNavigation
extends RefCounted
## Flat XZ navigation; cell centers and colliders share the same authored footprint.
var grid := AStarGrid2D.new()

func _init() -> void:
	grid.region = Rect2i(-18, -18, 37, 37)
	grid.cell_size = Vector2.ONE
	grid.diagonal_mode = AStarGrid2D.DIAGONAL_MODE_ONLY_IF_NO_OBSTACLES
	grid.update()
	for x in range(-18, 19):
		for y in range(-18, 19):
			if abs(x) >= 17 or abs(y) >= 17:
				grid.set_point_solid(Vector2i(x, y))

func block(center: Vector3, size: Vector3) -> void:
	for x in range(floori(center.x - size.x / 2.0 - 0.4), ceili(center.x + size.x / 2.0 + 0.4) + 1):
		for z in range(floori(center.z - size.z / 2.0 - 0.4), ceili(center.z + size.z / 2.0 + 0.4) + 1):
			if grid.is_in_boundsv(Vector2i(x, z)):
				grid.set_point_solid(Vector2i(x, z))

func cell(p: Vector3) -> Vector2i:
	return Vector2i(clampi(roundi(p.x), -16, 16), clampi(roundi(p.z), -16, 16))

func nearest(p: Vector3) -> Vector2i:
	var c := cell(p)
	if not grid.is_point_solid(c):
		return c
	var best := Vector2i.ZERO
	var distance := INF
	for x in range(-16, 17):
		for y in range(-16, 17):
			var candidate := Vector2i(x, y)
			if not grid.is_point_solid(candidate) and Vector2(candidate - c).length_squared() < distance:
				best = candidate
				distance = Vector2(candidate - c).length_squared()
	return best

func path(from: Vector3, to: Vector3) -> PackedVector3Array:
	var points := grid.get_id_path(nearest(from), nearest(to))
	var output := PackedVector3Array()
	for i in range(1, points.size()):
		output.append(Vector3(points[i].x, 0, points[i].y))
	if output.is_empty() and not grid.is_point_solid(cell(to)):
		output.append(Vector3(to.x, 0, to.z))
	return output

func walkable(p: Vector3) -> bool:
	return absf(p.x) < 16.5 and absf(p.z) < 16.5 and not grid.is_point_solid(cell(p))
