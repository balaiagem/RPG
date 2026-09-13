extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func shot(path: String) -> void:
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(path)

func run() -> void:
	var game: Node3D = load("res://scenes/main.tscn").instantiate()
	root.add_child(game)
	current_scene = game
	await process_frame
	DirAccess.make_dir_recursive_absolute("res://artifacts")
	for step in range(6):
		game.creator.choice = CharacterBuild.default_choice("wizard")
		game.creator.step = step
		game.creator.redraw_page()
		await shot("res://artifacts/creation_%d.png" % step)
	game.apply_build(CharacterBuild.default_choice("wizard"))
	game.creator.queue_free()
	game.creating = false
	game.hud.visible = true
	game.dice.visible = true
	game.rng.seed = 173
	game.player.rules.check_skill("arcana", 12)
	game.dice.age = 1
	await shot("res://artifacts/dice_hud.png")
	game.queue_free()
	await process_frame
	quit()
