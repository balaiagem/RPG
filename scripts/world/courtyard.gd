class_name HollowCourtyard
extends Node3D
var navigation: CourtyardNavigation
var rng := RandomNumberGenerator.new()

func build(nav: CourtyardNavigation) -> void:
	navigation = nav
	rng.seed = 82731
	var ground := Color("354845")
	Geometry.solid(self, Vector3(0, -0.35, 0), Vector3(38, 0.65, 38), ground)
	# Stone paths connect the arrival stair, dueling court and sealed shrine.
	for x in range(-5, 6):
		for z in range(-7, 12):
			if abs(x) < 3 or z < 2:
				var shade := rng.randf_range(0.30, 0.41)
				var tile := Geometry.box(self, Vector3(x * 1.1, rng.randf_range(0.003, 0.025), z * 1.1), Vector3(1.02, 0.10, 1.01), Color(shade, shade * 1.1, shade * 1.05))
				tile.rotation.y = rng.randf_range(-0.025, 0.025)
	Geometry.cylinder(self, Vector3(0, 0.05, -2), 4.7, 0.13, Color("485652"))
	Geometry.ring(self, Vector3(0, 0.13, -2), 4.2, Color("8c8970"), 0.03)
	Geometry.ring(self, Vector3(0, 0.14, -2), 3.8, Color("727d70"), 0.02)
	# Broken walls define navigable gaps, rather than decorative invisible barriers.
	for spec in [[Vector3(-7, 1, -3), Vector3(1, 2, 7)], [Vector3(7, 0.75, -5), Vector3(1, 1.5, 4)], [Vector3(-5, 0.65, 7), Vector3(4, 1.3, 1)], [Vector3(5, 0.65, 7), Vector3(4, 1.3, 1)], [Vector3(0, 1.0, -12), Vector3(7, 2, 1)]]:
		wall(spec[0], spec[1])
	for p in [Vector3(-7, 0, -7), Vector3(-7, 0, 1), Vector3(7, 0, -7), Vector3(7, 0, 1), Vector3(-3, 0, -12), Vector3(3, 0, -12)]:
		pillar(p)
	# Shrine silhouette beyond the arena.
	Geometry.cylinder(self, Vector3(0, 0.32, -10), 1.5, 0.55, Color("63716b"))
	var relic := Geometry.cylinder(self, Vector3(0, 2.0, -10), 0.47, 2.8, Color("85b5a6"), 0)
	relic.rotation.z = 0.15
	Geometry.ring(self, Vector3(0, 0.63, -10), 1.1, Color("88ddd0"))
	navigation.block(Vector3(0, 0, -10), Vector3(2, 1, 2))
	# Dark water and a short plank crossing occupy the eastern edge.
	Geometry.box(self, Vector3(12.5, -0.005, 0), Vector3(2.7, 0.1, 32), Color("20434a"))
	for z in range(-16, 17):
		if abs(z - 5) > 1:
			navigation.block(Vector3(12.5, 0, z), Vector3(1.8, 1, 0.1))
	for z in range(8):
		Geometry.box(self, Vector3(12.5, 0.13, 3.8 + z * 0.31), Vector3(4, 0.20, 0.26), Color("665a44"))
	for z in [3.8, 6.0]:
		Geometry.box(self, Vector3(12.5, 0.5, z), Vector3(4, 0.16, 0.13), Color("514838"))
	# Tree masses frame the combat space; seeded placement is authored and repeatable.
	for i in range(70):
		var p := Vector3(rng.randf_range(-19, 19), 0, rng.randf_range(-19, 19))
		if absf(p.x) < 9 and absf(p.z) < 14:
			continue
		if p.x > 10 and p.x < 15:
			continue
		tree(p, rng.randf_range(0.8, 1.4))
	for i in range(140):
		var p := Vector3(rng.randf_range(-16, 16), 0, rng.randf_range(-16, 16))
		if absf(p.x) < 6 and p.z < 12:
			continue
		Geometry.cylinder(self, p + Vector3(0, 0.2, 0), rng.randf_range(0.16, 0.4), rng.randf_range(0.3, 0.7), Color("536e50"), 0)
	for p in [Vector3(-3, 0, 7), Vector3(3, 0, 7), Vector3(-6, 0, -6), Vector3(6, 0, -6)]:
		Geometry.cylinder(self, p + Vector3(0, 0.6, 0), 0.16, 1.2, Color("665b46"))
		Geometry.cylinder(self, p + Vector3(0, 1.38, 0), 0.22, 0.5, Color("ffb66b"), 0)
		var light := OmniLight3D.new()
		light.position = p + Vector3(0, 1.6, 0)
		light.light_color = Color("ffb274")
		light.light_energy = 2.4
		light.omni_range = 6
		add_child(light)

func wall(p: Vector3, size: Vector3) -> void:
	Geometry.solid(self, p, size, Color("59655d"))
	navigation.block(p, size)
	Geometry.box(self, p + Vector3(0, size.y / 2, 0), Vector3(size.x + 0.14, 0.18, size.z + 0.14), Color("798171"))

func pillar(p: Vector3) -> void:
	Geometry.solid(self, p + Vector3(0, 1.3, 0), Vector3(0.85, 2.6, 0.85), Color("778073"))
	Geometry.box(self, p + Vector3(0, 2.6, 0), Vector3(1.15, 0.3, 1.15), Color("929582"))
	Geometry.box(self, p + Vector3(0, 0.15, 0), Vector3(1.2, 0.3, 1.2), Color("727e70"))
	navigation.block(p, Vector3(1, 1, 1))

func tree(p: Vector3, size: float) -> void:
	Geometry.cylinder(self, p + Vector3(0, 1.3 * size, 0), 0.28 * size, 2.6 * size, Color("413f33"), 0.16 * size)
	for i in range(3):
		var color := Color("284c45").lightened(i * 0.055)
		Geometry.cylinder(self, p + Vector3(0, (2.8 + i * 0.85) * size, 0), (1.8 - i * 0.35) * size, 2.5 * size, color, 0)
	navigation.block(p, Vector3(0.7, 1, 0.7))
