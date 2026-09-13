class_name Geometry
extends RefCounted
## Original metre-scale placeholder meshes; all actor origins are at their feet.

static func material(color: Color, emission: float = 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.85
	if emission > 0:
		m.emission_enabled = true
		m.emission = color
		m.emission_energy_multiplier = emission
	return m

static func mesh(parent: Node3D, resource: Mesh, pos: Vector3, color: Color, emission: float = 0) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = resource
	instance.position = pos
	instance.material_override = material(color, emission)
	parent.add_child(instance)
	return instance

static func box(parent: Node3D, pos: Vector3, size: Vector3, color: Color) -> MeshInstance3D:
	var b := BoxMesh.new()
	b.size = size
	return mesh(parent, b, pos, color)

static func cylinder(parent: Node3D, pos: Vector3, radius: float, height: float, color: Color, top: float = -1) -> MeshInstance3D:
	var c := CylinderMesh.new()
	c.top_radius = radius if top < 0 else top
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = 8
	return mesh(parent, c, pos, color)

static func ring(parent: Node3D, pos: Vector3, radius: float, color: Color, width: float = 0.06) -> MeshInstance3D:
	var t := TorusMesh.new()
	t.inner_radius = maxf(0.01, radius - width)
	t.outer_radius = radius + width
	t.rings = 40
	t.ring_segments = 6
	var result := mesh(parent, t, pos, color, 1.2)
	result.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return result

static func solid(parent: Node3D, pos: Vector3, size: Vector3, color: Color) -> void:
	box(parent, pos, size, color)
	var body := StaticBody3D.new()
	body.position = pos
	body.collision_layer = 1
	var collider := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	collider.shape = shape
	body.add_child(collider)
	parent.add_child(body)
