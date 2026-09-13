class_name GameHUD
extends Control
## Canvas-drawn HUD in a fixed logical 1600x900 canvas, scaled by Godot.
var game: Node3D
var font: Font = ThemeDB.fallback_font
var message := "Follow the broken path. Something waits among the stones."
var message_time := 7.0
var log_lines: Array[String] = []
var show_log := false
var show_help := false
var ink := Color("dce5d9")
var muted := Color("94aaa4")
var gold := Color("dcba7f")
var panel := Color(0.045, 0.077, 0.082, 0.94)

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

func _process(delta: float) -> void:
	message_time = maxf(0, message_time - delta)
	queue_redraw()

func text_at(pos: Vector2, text: String, size: int = 18, color: Color = ink) -> void:
	draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, color)

func centered(y: float, text: String, size: int, color: Color) -> void:
	var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x
	text_at(Vector2((1600 - width) / 2, y), text, size, color)

func card(rect: Rect2, color: Color = panel, border: Color = Color("3b514e")) -> void:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(5)
	draw_style_box(style, rect)

func bar(rect: Rect2, ratio: float, color: Color) -> void:
	draw_rect(rect, Color("17282b"))
	draw_rect(Rect2(rect.position, Vector2(rect.size.x * clampf(ratio, 0, 1), rect.size.y)), color)

func slot_rect(index: int) -> Rect2:
	return Rect2(572 + index * 116, 786, 106, 88)

func _draw() -> void:
	if not is_instance_valid(game) or not is_instance_valid(game.player):
		return
	var p: Warden = game.player
	var fifth := is_instance_valid(p.rules)
	# Quiet top band and chapter marker.
	draw_rect(Rect2(0, 0, 1600, 7), Color("a38152"))
	text_at(Vector2(36, 46), "A S H E N   H O L L O W", 23, ink)
	text_at(Vector2(38, 71), "THE BROKEN WATCH  /  COMBAT PROTOTYPE", 12, gold)
	card(Rect2(36, 107, 310, 119))
	text_at(Vector2(55, 134), "I   /   A FIRE IN THE ROOTS", 14, gold)
	var objective := "Defeat the Thornbound"
	var detail := "Right-click an enemy to approach and attack."
	if game.enemy.dead:
		objective = "Collect the Ember Shard"
		detail = "Approach the glow. Left-click to collect."
	if "ember_shard" in p.inventory:
		objective = "The watch is quiet again"
		detail = "Prototype complete. F5 starts a new encounter."
	text_at(Vector2(55, 167), objective, 20, ink)
	text_at(Vector2(55, 198), detail, 12, muted)
	text_at(Vector2(38, 255), "H  FIELD GUIDE     L  COMBAT LOG", 12, muted)
	# Region minimap, populated by the same navigation grid as gameplay.
	card(Rect2(1375, 30, 191, 206))
	for x in range(-16, 17):
		for z in range(-16, 17):
			var c := Color("314743") if not game.navigation.grid.is_point_solid(Vector2i(x, z)) else Color("152b2c")
			draw_rect(Rect2(1388 + (x + 16) * 5, 43 + (z + 16) * 5, 4, 4), c)
	var dot := Vector2(1470, 125) + Vector2(p.position.x, p.position.z) * 5
	draw_circle(dot, 4, Color("b8f4e2"))
	if not game.enemy.dead:
		draw_circle(Vector2(1470, 125) + Vector2(game.enemy.position.x, game.enemy.position.z) * 5, 4, Color("ec7755"))
	text_at(Vector2(1418, 223), "BROKEN WATCH", 12, gold)
	# Floating enemy information follows the actor, including conditions.
	if not game.enemy.dead:
		var at: Vector2 = game.camera_rig.camera.unproject_position(game.enemy.global_position + Vector3(0, 2.65, 0))
		text_at(at + Vector2(-58, -19), "THORNBOUND", 12, ink)
		bar(Rect2(at + Vector2(-62, -9), Vector2(124, 6)), float(game.enemy.health) / game.enemy.max_health, Color("c36d50"))
		if game.enemy.root_time > 0 or game.enemy.stagger_time > 0:
			text_at(at + Vector2(-32, 18), "STAGGER" if game.enemy.stagger_time > 0 else "ROOTED", 12, gold)
	# Bottom bar separates vitality, actions and focus without obscuring the world.
	card(Rect2(30, 775, 502, 106))
	card(Rect2(1068, 775, 502, 106))
	draw_circle(Vector2(80, 827), 30, Color("29464a"))
	draw_arc(Vector2(80, 827), 31, 0, TAU, 32, gold, 1.5)
	text_at(Vector2(68, 837), "I" if fifth else "III", 25, gold)
	text_at(Vector2(130, 805), (game.characters.player.name + " / " + game.characters.player["class"]).to_upper() if fifth else "HOLLOW WARDEN", 14, gold)
	text_at(Vector2(130, 831), "%d / %d" % [p.health, p.max_health], 21)
	bar(Rect2(130, 848, 370, 10), float(p.health) / p.max_health, Color("b95e50"))
	if p.temporary_hp > 0:
		text_at(Vector2(347, 829), "+%d BARRIER" % p.temporary_hp, 13, Color("85dce4"))
	if fifth:
		var res: RoundResources = p.rules.resources
		text_at(Vector2(1090, 805), "SPELL SLOTS  %d / %d   •   AC %d" % [res.slots, res.max_slots, p.armor], 14, gold)
		text_at(Vector2(1090, 832), "ACTION %s   BONUS %s" % ["READY" if res.actions.action <= 0 else "%.1fs" % res.actions.action, "READY" if res.actions.bonus <= 0 else "%.1fs" % res.actions.bonus], 13)
		text_at(Vector2(1090, 858), "5  SHORT REST    6  LONG REST", 12, muted)
		var status: String = res.concentration.capitalize() if not res.concentration.is_empty() else ("RAGING" if res.rage_time > 0 else ("HIDDEN" if res.hidden else ""))
		if not status.is_empty():
			centered(740, status, 16, gold)
	else:
		text_at(Vector2(1090, 805), "EMBER FOCUS", 14, gold)
		text_at(Vector2(1090, 831), "%d / 100" % int(p.mana), 21)
		bar(Rect2(1090, 848, 295, 10), p.mana / 100.0, Color("548e9c"))
	for i in range(4):
		var id: String = game.abilities.slots[i]
		var data: Dictionary = game.ability_data[id]
		var rect := slot_rect(i)
		var selected: bool = game.aim_slot == i
		card(rect, Color("273d3c") if selected else panel, gold if selected else Color("52625a"))
		var color := [Color("f3ae6c"), Color("8dd3df"), Color("d0bd8c"), Color("ffe0a0")][i] as Color
		var center := rect.position + Vector2(53, 35)
		if i == 0:
			draw_colored_polygon(PackedVector2Array([center + Vector2(0, -21), center + Vector2(10, 0), center + Vector2(0, 15), center + Vector2(-10, 0)]), color)
		elif i == 1:
			draw_polyline(PackedVector2Array([center + Vector2(-15, -16), center + Vector2(15, -16), center + Vector2(14, 6), center + Vector2(0, 19), center + Vector2(-14, 6), center + Vector2(-15, -16)]), color, 2.5)
		else:
			draw_arc(center, 17, 0, TAU, 32, color, 2)
			draw_circle(center, 5 if i == 2 else 10, color)
		var cooldown: float = game.abilities.cooldowns[id]
		if cooldown > 0:
			draw_rect(Rect2(rect.position, Vector2(rect.size.x, rect.size.y * cooldown / float(data.cooldown))), Color(0.025, 0.045, 0.05, 0.8))
			text_at(rect.position + Vector2(34, 44), "%.1f" % cooldown, 24, ink)
		text_at(rect.position + Vector2(10, 75), data.key, 16, gold)
		text_at(rect.position + Vector2(67, 75), ("S1" if int(data.spellSlotCost) > 0 else str(data.castType).left(1).to_upper()) if fifth else str(data.manaCost), 12, muted)
		if rect.has_point(get_local_mouse_position()):
			card(Rect2(510, 600, 800, 135))
			text_at(Vector2(531, 630), data.name.to_upper(), 19, gold)
			draw_multiline_string(font, Vector2(531, 658), data.description, HORIZONTAL_ALIGNMENT_LEFT, 753, 14, 2, ink)
			var cost := "%s • %d spell slot • %s" % [data.castType.capitalize(), data.spellSlotCost, data.targetingType] if fifth else "%ss recovery / %s focus" % [data.cooldown, data.manaCost]
			if fifth and data.has("resource"):
				cost += " • %d uses" % p.rules.resources.pools.get(data.resource, 0)
			text_at(Vector2(531, 717), cost, 12, muted)
	text_at(Vector2(1410, 811), "1  /  TONIC × %d" % p.potion_count, 13, ink)
	text_at(Vector2(1410, 838), "SPACE / DODGE" if fifth else "SPACE / STEP", 12, muted)
	text_at(Vector2(1410, 859), "READY" if p.dodge_cooldown <= 0 else "%.1fs" % p.dodge_cooldown, 12, gold)
	centered(768, "TARGETED ABILITIES: HOLD KEY TO AIM, RELEASE TO CAST  •  C: SKILL CHECK  •  F6: NEW HERO" if fifth else "Q / E  HOLD TO AIM, RELEASE TO CAST     W / R  INSTANT", 12, muted)
	if message_time > 0:
		centered(713, message, 17, gold)
	if game.aim_slot >= 0:
		centered(650, "RELEASE TO CAST  •  RIGHT CLICK TO CANCEL", 13, gold)
	if show_log:
		card(Rect2(36, 470, 450, 225))
		text_at(Vector2(53, 498), "COMBAT RECORD / D20", 14, gold)
		for i in range(log_lines.size()):
			text_at(Vector2(53, 524 + i * 23), log_lines[i], 12, muted)
	if show_help:
		card(Rect2(450, 190, 700, 440))
		text_at(Vector2(482, 238), "THE WARDEN'S FIELD GUIDE", 27, gold)
		var lines := ["Right click ground — move; right click enemy — pursue and attack", "Hold Q / E — preview aim; release — cast; right click — cancel", "W — temporary barrier     R — sweeping burst and interrupt", "Space — step toward cursor (3s recovery; no invulnerability)", "1 — drink a healing tonic     Shift — hold position", "Tab — highlight enemy and loot     Wheel — zoom     A / D — rotate", "Left click shard nearby — collect     L — dice log", "Esc — pause and audio settings     F5 — restart encounter", "Avoid the red circle before the Thornbound's cleave lands."]
		if fifth:
			lines = ["Right click ground — move; right click enemy — pursue and attack", "Targeted QWER: hold to aim, release to cast; right click cancels", "Self abilities cast immediately. Hover a slot for its exact rules.", "Actions and bonus actions recover independently over 6 seconds.", "Space — Dodge action    1 — healing potion (action)", "5 — short rest    6 — long rest (safe areas only)", "C — Arcana check    L — log    F6 — new character", "Esc — pause    F5 — fresh encounter, same character", "Spell slots and limited features require rest. Movement stays free."]
		for i in range(lines.size()):
			text_at(Vector2(482, 283 + i * 34), lines[i], 16, ink)
	if p.dead:
		draw_rect(Rect2(0, 0, 1600, 900), Color(0.025, 0.035, 0.04, 0.73))
		centered(375, "BETWEEN LIFE AND DEATH" if fifth and p.rules.downed else "THE EMBER FADES", 40, gold)
		centered(425, ("STABLE — the encounter has ended" if p.rules.stable else "Death saves: %d successes / %d failures" % [p.rules.death_successes, p.rules.death_failures]) if fifth else "A fallen warden. Another chance.", 20, ink)
		centered(485, "Press F5 to try again", 18, muted)
	if get_tree().paused:
		draw_rect(Rect2(0, 0, 1600, 900), Color(0.025, 0.035, 0.04, 0.85))
		centered(290, "A MOMENT OF STILLNESS", 32, gold)
		for i in range(3):
			var rect := Rect2(610, 350 + i * 75, 380, 56)
			card(rect, Color("29403e") if rect.has_point(get_local_mouse_position()) else panel)
			var captions := ["RESUME  /  ESC", "AUDIO: " + ("OFF" if game.muted else "ON"), "RESTART ENCOUNTER  /  F5"]
			centered(385 + i * 75, captions[i], 16, ink)

func handle_click(point: Vector2) -> bool:
	if get_tree().paused:
		for i in range(3):
			if Rect2(610, 350 + i * 75, 380, 56).has_point(point):
				if i == 0:
					game.toggle_pause()
				elif i == 1:
					game.toggle_audio()
				else:
					game.restart()
		return true
	return point.y >= 775 or (show_help and Rect2(450, 190, 700, 440).has_point(point))

func notify(value: String) -> void:
	message = value
	message_time = 4.0

func log_event(value: String) -> void:
	log_lines.append(value)
	if log_lines.size() > 7:
		log_lines.pop_front()
