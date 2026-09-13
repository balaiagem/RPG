class_name CharacterCreator
extends Control
signal completed(choice: Dictionary)

var choice := CharacterBuild.default_choice()
var classes := CharacterBuild.catalog("classes_5e")
var ancestries := CharacterBuild.catalog("ancestries")
var spells := CharacterBuild.catalog("abilities_5e")
var step := 0
var content: Control
var summary: Control
var error_label: Label
var preview_actor: Combatant
var preview_root: Node3D
var font := ThemeDB.fallback_font
var ink := Color("e1e5d9")
var gold := Color("d9bb83")
var muted := Color("93aba8")
var saved := CharacterBuild.load_choice()

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	var theme_resource := Theme.new()
	theme_resource.default_font_size = 18
	for kind in ["normal", "hover", "pressed", "focus", "disabled"]:
		var style := StyleBoxFlat.new()
		style.bg_color = Color("1b3339") if kind == "normal" else Color("375351")
		style.border_color = Color("6c8276") if kind == "normal" else gold
		style.set_border_width_all(1)
		style.set_corner_radius_all(5)
		style.content_margin_left = 14
		style.content_margin_right = 14
		theme_resource.set_stylebox(kind, "Button", style)
		theme_resource.set_stylebox(kind, "OptionButton", style)
	theme = theme_resource
	redraw_page()

func _draw() -> void:
	draw_rect(Rect2(0, 0, 1600, 900), Color("0b191f"))
	draw_rect(Rect2(0, 0, 1600, 5), gold)
	draw_rect(Rect2(1080, 108, 480, 660), Color("14282e"))
	draw_line(Vector2(270, 115), Vector2(270, 766), Color("3e5556"), 1)
	draw_line(Vector2(40, 789), Vector2(1560, 789), Color("3e5556"), 1)
	draw_string(font, Vector2(42, 55), "A S H E N   H O L L O W", 0, -1, 24, gold)
	draw_string(font, Vector2(42, 82), "CHARACTER CREATION  /  A NEW SOUL AT THE BROKEN WATCH", 0, -1, 13, muted)
	draw_string(font, Vector2(1140, 62), "LEVEL 1   •   FIFTH EDITION", 0, -1, 17, ink)
	var names := ["Origin", "Class", "Attributes", "Skills", "Spells", "Review"]
	for i in range(names.size()):
		var color := gold if i == step else muted
		draw_circle(Vector2(57, 158 + i * 74), 16, Color("345250") if i <= step else Color("14282e"))
		draw_string(font, Vector2(52, 164 + i * 74), str(i + 1), 0, -1, 15, color)
		draw_string(font, Vector2(88, 165 + i * 74), names[i], 0, -1, 20, color)
	draw_string(font, Vector2(43, 693), "YOUR CHOICES MATTER", 0, -1, 13, gold)
	draw_string(font, Vector2(43, 723), "One character. Twelve paths.", 0, -1, 14, muted)
	draw_string(font, Vector2(43, 748), "The dice decide the rest.", 0, -1, 14, muted)

func label_at(parent: Control, rect: Rect2, value: String, size: int = 18, color: Color = ink) -> Label:
	var label := Label.new()
	label.position = rect.position
	label.size = rect.size
	label.text = value
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)
	return label

func button_at(parent: Control, rect: Rect2, caption: String, action: Callable, selected: bool = false) -> Button:
	var button := Button.new()
	button.position = rect.position
	button.size = rect.size
	button.text = caption
	button.add_theme_color_override("font_color", gold if selected else ink)
	button.pressed.connect(action)
	parent.add_child(button)
	return button

func redraw_page() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	content = Control.new()
	content.position = Vector2(310, 118)
	add_child(content)
	summary = Control.new()
	summary.position = Vector2(1105, 125)
	add_child(summary)
	var headings := ["Where your story begins", "Choose your calling", "Shape your strengths", "What you know", "Prepare your magic", "Your story awaits"]
	label_at(content, Rect2(0, 0, 735, 45), headings[step], 33, gold)
	match step:
		0: origin_page()
		1: class_page()
		2: attributes_page()
		3: skills_page()
		4: spells_page()
		5: review_page()
	build_summary()
	if step > 0:
		button_at(self, Rect2(310, 812, 165, 53), "BACK", func(): step -= 1; redraw_page())
	if step < 5:
		button_at(self, Rect2(1330, 812, 230, 53), "CONTINUE  →", next_step)
	else:
		button_at(self, Rect2(1260, 812, 300, 53), "ENTER THE HOLLOW", finish)
	if step == 0 and not saved.is_empty():
		button_at(self, Rect2(310, 812, 290, 53), "USE SAVED CHARACTER", func(): choice = saved.duplicate(true); step = 5; redraw_page())
	error_label = label_at(self, Rect2(620, 812, 615, 55), "", 16, Color("df9e80"))
	queue_redraw()

func next_step() -> void:
	if step == 3 and choice.skills.size() != int(classes[choice["class"]].skillCount):
		error_label.text = "Choose exactly %d skill proficiencies." % classes[choice["class"]].skillCount
		return
	step += 1
	redraw_page()

func finish() -> void:
	var error := CharacterBuild.validate(choice)
	if not error.is_empty():
		error_label.text = error
		return
	completed.emit(choice.duplicate(true))

func origin_page() -> void:
	label_at(content, Rect2(0, 59, 700, 30), "NAME", 13, muted)
	var name_edit := LineEdit.new()
	name_edit.position = Vector2(0, 96)
	name_edit.size = Vector2(470, 43)
	name_edit.max_length = 24
	name_edit.text = choice.name
	name_edit.text_changed.connect(func(value: String): choice.name = value)
	content.add_child(name_edit)
	var index := 0
	for id in ancestries:
		button_at(content, Rect2((index % 2) * 362, 169 + (index / 2) * 69, 346, 55), ancestries[id].name, func(): choice.ancestry = id; redraw_page(), choice.ancestry == id)
		index += 1
	label_at(content, Rect2(0, 465, 718, 90), ancestries[choice.ancestry].description, 20)
	label_at(content, Rect2(0, 568, 150, 32), "CLOAK TONE", 13, muted)
	for i in range(5):
		var colors := ["#82b8c3", "#b76c52", "#95b881", "#b895c7", "#d8c79c"]
		var button := button_at(content, Rect2(150 + i * 106, 563, 92, 43), "◆", func(): choice.color = colors[i]; redraw_page())
		button.add_theme_color_override("font_color", Color(colors[i]))

func class_page() -> void:
	label_at(content, Rect2(0, 54, 730, 44), "Each class changes your defenses, weapon, features and magic.", 17, muted)
	var index := 0
	for id in classes:
		var c: Dictionary = classes[id]
		button_at(content, Rect2((index % 3) * 241, 115 + (index / 3) * 84, 228, 71), c.name + "\nD" + str(int(c.hitDie)), func():
			var previous := choice.duplicate(true)
			choice = CharacterBuild.default_choice(id)
			choice.name = previous.name
			choice.ancestry = previous.ancestry
			redraw_page(), choice["class"] == id)
		index += 1
	var selected: Dictionary = classes[choice["class"]]
	label_at(content, Rect2(0, 465, 725, 76), selected.description, 22)
	label_at(content, Rect2(0, 555, 725, 70), "FEATURES  /  " + ", ".join(selected.passives), 17, gold)

func attributes_page() -> void:
	label_at(content, Rect2(0, 58, 723, 60), "Standard array: 15, 14, 13, 12, 10, 8. Selecting a used value swaps the two attributes. Ancestry bonuses apply afterward.", 18, muted)
	var stats := CharacterBuild.derive(choice)
	for i in range(6):
		var stat: String = CharacterBuild.STATS[i]
		var x := (i % 3) * 241
		var y := 158 + (i / 3) * 190
		label_at(content, Rect2(x, y, 220, 30), stat.to_upper(), 15, gold)
		var option := OptionButton.new()
		option.position = Vector2(x, y + 43)
		option.size = Vector2(215, 55)
		for number in CharacterBuild.ARRAY:
			option.add_item(str(number))
		option.select(CharacterBuild.ARRAY.find(int(choice.scores[stat])))
		option.item_selected.connect(func(index: int):
			var next_value: int = CharacterBuild.ARRAY[index]
			for other in CharacterBuild.STATS:
				if int(choice.scores[other]) == next_value:
					choice.scores[other] = choice.scores[stat]
					break
			choice.scores[stat] = next_value
			redraw_page())
		content.add_child(option)
		label_at(content, Rect2(x, y + 111, 228, 35), "Final %d   /   %+d modifier" % [stats.attributes[stat], CombatRules.modifier(stats.attributes[stat])], 17)
	label_at(content, Rect2(0, 569, 720, 55), "Spell DC = 8 + proficiency + casting modifier. Weapon attacks use the weapon's attribute.", 17, muted)

func skills_page() -> void:
	var c: Dictionary = classes[choice["class"]]
	label_at(content, Rect2(0, 56, 720, 60), "Choose %d proficiencies. Selected: %d. Rogue expertise applies to the first two selected skills." % [c.skillCount, choice.skills.size()], 18, muted)
	for i in range(c.skillChoices.size()):
		var skill: String = c.skillChoices[i]
		button_at(content, Rect2((i % 3) * 241, 146 + (i / 3) * 70, 229, 56), ("✓ " if skill in choice.skills else "") + skill.capitalize(), func():
			if skill in choice.skills:
				choice.skills.erase(skill)
			elif choice.skills.size() < int(c.skillCount):
				choice.skills.append(skill)
			redraw_page(), skill in choice.skills)

func spells_page() -> void:
	var c: Dictionary = classes[choice["class"]]
	if c.spellChoices.is_empty():
		label_at(content, Rect2(0, 79, 720, 94), "Your class does not cast spells at level 1. Your class features and universal combat actions are ready.", 23)
		label_at(content, Rect2(0, 220, 720, 250), "\n\n".join(c.abilities.map(func(id): return spells[id].name + " — " + spells[id].description)), 19, gold)
		return
	label_at(content, Rect2(0, 56, 720, 64), "Prepare two combat spells from the implemented library: one cantrip and one level-one spell. Cantrips cost no spell slots.", 18, muted)
	for level in range(2):
		var y := 147 + level * 232
		label_at(content, Rect2(0, y, 700, 27), "CANTRIP  /  AT WILL" if level == 0 else "LEVEL 1  /  SPELL SLOT", 14, gold)
		var choices: Array = c.spellChoices.filter(func(id): return int(spells[id].level) == level)
		var option := OptionButton.new()
		option.position = Vector2(0, y + 40)
		option.size = Vector2(710, 49)
		for id in choices:
			option.add_item(spells[id].name)
		option.select(choices.find(choice.spells[level]))
		option.item_selected.connect(func(index: int): choice.spells[level] = choices[index]; redraw_page())
		content.add_child(option)
		var spell: Dictionary = spells[choice.spells[level]]
		label_at(content, Rect2(0, y + 101, 710, 66), spell.description, 19)
		label_at(content, Rect2(0, y + 172, 710, 44), "%s  •  %s  •  %.1f m  •  %s" % [spell.school, spell.castType.capitalize(), spell.range, "Concentration" if spell.concentration else "No concentration"], 14, muted)

func review_page() -> void:
	var stats := CharacterBuild.derive(choice)
	label_at(content, Rect2(0, 73, 720, 63), "%s, %s %s" % [stats.name, ancestries[choice.ancestry].name, classes[choice["class"]].name], 29)
	label_at(content, Rect2(0, 159, 720, 63), "SKILLS  /  " + ", ".join(choice.skills).capitalize(), 18, muted)
	for i in range(4):
		var ability: Dictionary = spells[stats.abilities[i]]
		label_at(content, Rect2(0, 248 + i * 83, 720, 28), ["Q", "W", "E", "R"][i] + "  /  " + ability.name, 21, gold)
		label_at(content, Rect2(44, 278 + i * 83, 670, 49), ability.description, 15)
	label_at(content, Rect2(0, 593, 720, 45), "Character choices are saved locally. F5 resets the encounter with this build.", 16, muted)

func build_summary() -> void:
	var stats := CharacterBuild.derive(choice)
	label_at(summary, Rect2(0, 0, 430, 32), classes[choice["class"]].name.to_upper(), 25, gold)
	label_at(summary, Rect2(0, 37, 430, 28), ancestries[choice.ancestry].name + "  •  LEVEL 1", 15, muted)
	var viewport_container := SubViewportContainer.new()
	viewport_container.position = Vector2(20, 75)
	viewport_container.size = Vector2(380, 280)
	viewport_container.stretch = true
	viewport_container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	summary.add_child(viewport_container)
	var viewport := SubViewport.new()
	viewport.size = Vector2i(380, 280)
	viewport.own_world_3d = true
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport_container.add_child(viewport)
	preview_root = Node3D.new()
	viewport.add_child(preview_root)
	preview_actor = Combatant.new()
	preview_root.add_child(preview_actor)
	preview_actor.build_model(false)
	preview_actor.apply_appearance(stats)
	preview_actor.rotation.y = PI - 0.35
	var camera := Camera3D.new()
	preview_root.add_child(camera)
	camera.position = Vector3(2.2, 2.2, 4.4)
	camera.look_at(Vector3(0, 0.9, 0))
	camera.fov = 34
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-40, -30, 0)
	light.light_color = Color("fff0d2")
	preview_root.add_child(light)
	var rim := DirectionalLight3D.new()
	rim.rotation_degrees = Vector3(-25, 145, 0)
	rim.light_color = Color("88bed2")
	rim.light_energy = 0.7
	preview_root.add_child(rim)
	label_at(summary, Rect2(0, 363, 430, 37), "%d HP     %d AC     +2 PROFICIENCY" % [stats.hp, stats.armorClass], 22, gold)
	label_at(summary, Rect2(0, 413, 430, 35), "WEAPON  /  " + CharacterBuild.weapon(stats.weapon).name, 17)
	label_at(summary, Rect2(0, 454, 430, 83), "PASSIVES\n" + "\n".join(stats.passives), 16, muted)
	label_at(summary, Rect2(0, 554, 430, 75), "REAL-TIME ROUNDS\nAction + bonus action • 6-second recovery\nMove freely. Spell slots recover on rest.", 16, gold)
