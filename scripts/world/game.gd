extends Node3D
## Composition root: loads data, assembles systems, and translates input commands.
var characters: Dictionary
var items: Dictionary
var ability_data: Dictionary
var navigation: CourtyardNavigation
var player: Warden
var enemy: Thornbound
var enemies: Array[Thornbound] = []
var abilities: AbilityController
var effects: CombatEffects
var camera_rig: IsometricCamera
var hud: GameHUD
var rng := RandomNumberGenerator.new()
var aim_slot := -1
var aim_ring: MeshInstance3D
var range_ring: MeshInstance3D
var destination: MeshInstance3D
var destination_time := 0.0
var loot: Node3D
var loot_point := Vector3.ZERO
var muted := false
var elapsed := 0.0
var skip_creation := false
var creating := false
var creator: CharacterCreator
var dice: DiceOverlay
var companion: WayfarerCompanion

func read_data(path: String) -> Dictionary:
	return JSON.parse_string(FileAccess.get_file_as_string("res://data/" + path + ".json"))

func _ready() -> void:
	rng.randomize()
	characters = read_data("characters")
	items = read_data("items")
	ability_data = read_data("abilities")
	navigation = CourtyardNavigation.new()
	var courtyard := HollowCourtyard.new()
	add_child(courtyard)
	courtyard.build(navigation)
	setup_lighting()
	effects = CombatEffects.new()
	add_child(effects)
	player = Warden.new()
	player.name = "Warden"
	player.position = Vector3(0, 0, 8)
	add_child(player)
	player.setup(self)
	enemy = Thornbound.new()
	enemy.name = "Thornbound"
	enemy.position = Vector3(0, 0, -3)
	add_child(enemy)
	enemy.setup(self)
	enemies.append(enemy)
	abilities = AbilityController.new()
	add_child(abilities)
	abilities.setup(self)
	camera_rig = IsometricCamera.new()
	add_child(camera_rig)
	camera_rig.target = player
	camera_rig.position = player.position
	camera_rig.camera.position = Vector3(14, 20, 17)
	camera_rig.camera.look_at(player.position)
	var listener := AudioListener3D.new()
	player.add_child(listener)
	listener.make_current()
	var canvas := CanvasLayer.new()
	add_child(canvas)
	hud = GameHUD.new()
	hud.game = self
	canvas.add_child(hud)
	dice = DiceOverlay.new()
	canvas.add_child(dice)
	aim_ring = Geometry.ring(self, Vector3.ZERO, 1, Color("edc387"))
	aim_ring.visible = false
	range_ring = Geometry.ring(self, Vector3.ZERO, 1, Color("75b5ad"), 0.015)
	range_ring.visible = false
	destination = Geometry.ring(self, Vector3.ZERO, 0.45, Color("a8eedc"))
	destination.visible = false
	load_settings()
	player.died.connect(func():
		cancel_aim()
		hud.log_event("The Hollow Warden has fallen."))
	if not skip_creation and not "--legacy" in OS.get_cmdline_user_args():
		if get_tree().has_meta("character_choice"):
			apply_build(get_tree().get_meta("character_choice"))
		else:
			open_creator(canvas)
	if "--capture" in OS.get_cmdline_user_args():
		capture_preview()

func setup_lighting() -> void:
	var world := WorldEnvironment.new()
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color("1c3238")
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("94bdb6")
	environment.ambient_light_energy = 0.28
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.fog_enabled = true
	environment.fog_light_color = Color("314d50")
	environment.fog_density = 0.009
	world.environment = environment
	add_child(world)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-48, -32, 0)
	sun.light_color = Color("ffe0a7")
	sun.light_energy = 0.8
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 70
	add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-30, 145, 0)
	fill.light_color = Color("7daabf")
	fill.light_energy = 0.18
	add_child(fill)

func _process(delta: float) -> void:
	elapsed += delta
	destination_time = maxf(0, destination_time - delta)
	destination.visible = destination_time > 0
	destination.scale = Vector3.ONE * (0.8 + destination_time * 0.5)
	if aim_slot >= 0 and not player.dead:
		var data: Dictionary = ability_data[abilities.slots[aim_slot]]
		var cursor := camera_rig.ground_point(get_viewport().get_mouse_position())
		var offset := (cursor - player.global_position).limit_length(float(data.range))
		aim_ring.position = player.global_position + offset + Vector3(0, 0.12, 0)
		aim_ring.scale = Vector3.ONE * maxf(0.45, float(data.radius))
		range_ring.position = player.global_position + Vector3(0, 0.10, 0)
		range_ring.scale = Vector3.ONE * float(data.range)
		aim_ring.visible = true
		range_ring.visible = true
	var highlight := Input.is_physical_key_pressed(KEY_TAB)
	if not enemy.dead:
		enemy.ring.scale = Vector3.ONE * (1.35 if highlight else 1.0)
	if is_instance_valid(loot):
		loot.position.y = 0.35 + sin(elapsed * 2) * 0.15
		loot.rotation.y += delta

func _input(event: InputEvent) -> void:
	if creating:
		return
	if event is InputEventKey and event.pressed and not event.echo:
		if event.physical_keycode == KEY_ESCAPE:
			if aim_slot >= 0:
				cancel_aim()
			else:
				toggle_pause()
			return
		if event.physical_keycode == KEY_F5:
			restart()
			return
		if event.physical_keycode == KEY_F6:
			get_tree().remove_meta("character_choice")
			restart()
			return
	# This node processes input while paused, but never advances gameplay then.
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if hud.handle_click(event.position):
			return
	if get_tree().paused or player.dead:
		return
	var mouse := get_viewport().get_mouse_position()
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			camera_rig.zoom = clampf(camera_rig.zoom - 0.08, 0.65, 1.35)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			camera_rig.zoom = clampf(camera_rig.zoom + 0.08, 0.65, 1.35)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			if aim_slot >= 0:
				cancel_aim()
				return
			if mouse.y >= 775:
				return
			var hit := camera_rig.pick(mouse)
			if not hit.is_empty() and hit.collider is Thornbound and not hit.collider.dead:
				player.target = hit.collider
				player.repath = 0
			else:
				var point := camera_rig.ground_point(mouse)
				player.command_move(point)
				var cell := navigation.nearest(point)
				destination.position = Vector3(cell.x, 0.14, cell.y)
				destination_time = 0.8
		elif event.button_index == MOUSE_BUTTON_LEFT:
			try_collect(mouse)
	if event is InputEventKey and not event.echo:
		var index := [KEY_Q, KEY_W, KEY_E, KEY_R].find(event.physical_keycode)
		if index >= 0:
			if event.pressed:
				var targeting: String = ability_data[abilities.slots[index]].targetingType
				if targeting in ["GROUND_POINT", "PROJECTILE", "TARGET_ENEMY", "CONE"]:
					aim_slot = index
				else:
					abilities.cast(index, camera_rig.ground_point(mouse))
			elif aim_slot == index:
				abilities.cast(index, camera_rig.ground_point(mouse))
				cancel_aim()
		elif event.pressed:
			match event.physical_keycode:
				KEY_1: player.drink_potion()
				KEY_SPACE: player.dodge(camera_rig.ground_point(mouse))
				KEY_H: hud.show_help = not hud.show_help
				KEY_L: hud.show_log = not hud.show_log
				KEY_A: camera_rig.angle -= PI / 8
				KEY_D: camera_rig.angle += PI / 8
				KEY_5:
					if is_instance_valid(player.rules): player.rules.rest(false)
				KEY_6:
					if is_instance_valid(player.rules): player.rules.rest(true)
				KEY_C:
					if is_instance_valid(player.rules): player.rules.check_skill("arcana", 12)

func cancel_aim() -> void:
	aim_slot = -1
	aim_ring.visible = false
	range_ring.visible = false

func resolve_attack(attacker: Combatant, victim: Combatant, result: Dictionary, label: String) -> void:
	if result.hit:
		victim.receive_damage(result.damage, result.critical, result.get("damageType", "physical"), attacker)
		effects.sound("impact", victim.global_position)
		if attacker == player:
			camera_rig.shake = 0.2 if result.critical else 0.06
	else:
		effects.number(victim.global_position + Vector3(0, 2.2, 0), "MISS", Color("bdc3b7"))
	dice.present(label, result.roll, result.total, victim.armor, result.hit, "CRITICAL HIT • %d damage" % result.damage if result.critical else "%d damage" % result.damage)
	hud.log_event("%s • d20 %d → %d vs AC %d • %s" % [label, result.roll, result.total, victim.armor, str(result.damage) + " damage" if result.hit else "miss"])

func spawn_loot(point: Vector3) -> void:
	if is_instance_valid(player.rules):
		player.rules.on_kill()
	loot_point = point
	loot = Node3D.new()
	add_child(loot)
	loot.position = point + Vector3(0, 0.35, 0)
	Geometry.cylinder(loot, Vector3(0, 0.35, 0), 0.2, 0.7, Color("edc589"), 0)
	Geometry.ring(loot, Vector3.ZERO, 0.65, Color("e2be7e"))
	var label := Label3D.new()
	label.text = "EMBER SHARD\nLeft click nearby to collect"
	label.font_size = 28
	label.pixel_size = 0.009
	label.position.y = 1.5
	label.modulate = Color("f2d29a")
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	loot.add_child(label)
	hud.notify("Thornbound defeated. Collect the Ember Shard.")

func try_collect(screen: Vector2) -> bool:
	if not is_instance_valid(loot):
		return false
	var projected := camera_rig.camera.unproject_position(loot.global_position + Vector3(0, 0.5, 0))
	if projected.distance_to(screen) > 85:
		return false
	if player.global_position.distance_to(loot_point) > 2.8:
		hud.notify("Move closer to the Ember Shard")
		return false
	player.inventory.append("ember_shard")
	loot.queue_free()
	loot = null
	effects.sound("loot", player.global_position)
	hud.notify("Ember Shard collected. First encounter complete.")
	return true

func toggle_pause() -> void:
	get_tree().paused = not get_tree().paused
	# Only this coordinator and the HUD receive paused input; children stay paused.
	process_mode = Node.PROCESS_MODE_ALWAYS if get_tree().paused else Node.PROCESS_MODE_PAUSABLE
	set_process(not get_tree().paused)
	for child in get_children():
		if child != hud:
			child.process_mode = Node.PROCESS_MODE_PAUSABLE
	hud.process_mode = Node.PROCESS_MODE_ALWAYS

func restart() -> void:
	get_tree().paused = false
	get_tree().reload_current_scene()

func toggle_audio() -> void:
	muted = not muted
	AudioServer.set_bus_mute(0, muted)
	var config := ConfigFile.new()
	config.set_value("audio", "muted", muted)
	config.save("user://settings.cfg")

func load_settings() -> void:
	var config := ConfigFile.new()
	if config.load("user://settings.cfg") == OK:
		muted = config.get_value("audio", "muted", false)
	AudioServer.set_bus_mute(0, muted)

func capture_preview() -> void:
	await get_tree().create_timer(2).timeout
	await RenderingServer.frame_post_draw
	DirAccess.make_dir_recursive_absolute("res://artifacts")
	get_viewport().get_texture().get_image().save_png("res://artifacts/preview.png")

func open_creator(canvas: CanvasLayer) -> void:
	creating = true
	hud.visible = false
	dice.visible = false
	player.set_physics_process(false)
	enemy.set_physics_process(false)
	creator = CharacterCreator.new()
	canvas.add_child(creator)
	creator.completed.connect(func(choice: Dictionary):
		var result := CharacterBuild.save_choice(choice)
		apply_build(choice)
		creator.queue_free()
		creating = false
		hud.visible = true
		dice.visible = true
		player.set_physics_process(true)
		enemy.set_physics_process(true)
		if result != OK:
			hud.notify("Character ready, but saving failed. This session remains playable."))

func apply_build(choice: Dictionary) -> void:
	get_tree().set_meta("character_choice", choice.duplicate(true))
	characters.player = CharacterBuild.derive(choice)
	ability_data = CharacterBuild.catalog("abilities_5e")
	for i in range(4):
		ability_data[characters.player.abilities[i]].key = ["Q", "W", "E", "R"][i]
	player.apply_character(characters.player)
	if is_instance_valid(companion):
		remove_child(companion)
		companion.queue_free()
		companion = null
	if choice["class"] == "bard":
		companion = WayfarerCompanion.new()
		companion.position = Vector3(1.5, 0, 9)
		add_child(companion)
		companion.setup(self)
	remove_child(abilities)
	abilities.queue_free()
	abilities = FifthEditionAbilities.new()
	add_child(abilities)
	abilities.setup(self)
	enemy.definition = enemy.definition.duplicate(true)
	enemy.definition.hp = 24
	enemy.definition.attackBonus = 3
	enemy.definition.damageDice = "1d6"
	enemy.definition.damageBonus = 1
	enemy.definition.cooldown = 6
	enemy.definition.aggroRange = 8
	enemy.health = 24
	enemy.max_health = 24
	hud.notify("%s arrives. C: Arcana check  •  5: short rest  •  6: long rest  •  F6: create a new hero" % characters.player.name)
