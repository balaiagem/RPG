class_name CombatEffects
extends Node3D
## Presentation boundary. Replace these procedural effects and tones with assets later.
var sounds: Dictionary = {}

func _exit_tree() -> void:
	for child in get_children():
		if child is AudioStreamPlayer3D:
			child.stop()
			child.stream = null
	sounds.clear()

func _ready() -> void:
	for entry in [["cast", 660.0], ["guard", 330.0], ["impact", 110.0], ["loot", 880.0]]:
		var wav := AudioStreamWAV.new()
		wav.format = AudioStreamWAV.FORMAT_16_BITS
		wav.mix_rate = 22050
		var bytes := PackedByteArray()
		var count := 4400
		bytes.resize(count * 2)
		for i in range(count):
			var envelope := pow(1.0 - float(i) / count, 2) * minf(1, i / 80.0)
			var sample := sin(TAU * float(entry[1]) * i / 22050.0) * envelope * 10000
			bytes.encode_s16(i * 2, int(sample))
		wav.data = bytes
		sounds[entry[0]] = wav

func sound(id: String, point: Vector3) -> void:
	var player := AudioStreamPlayer3D.new()
	player.stream = sounds.get(id, sounds.impact)
	player.position = point
	player.unit_size = 14
	player.volume_db = -13
	add_child(player)
	player.finished.connect(player.queue_free)
	player.play()

func pulse(point: Vector3, radius: float, color: Color, duration: float) -> void:
	var circle := Geometry.ring(self, point + Vector3(0, 0.08, 0), radius, color, 0.045)
	circle.scale = Vector3.ONE * 0.3
	var tween := create_tween().set_parallel()
	tween.tween_property(circle, "scale", Vector3.ONE, duration).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(circle, "transparency", 1.0, duration)
	tween.chain().tween_callback(circle.queue_free)

func number(point: Vector3, text: String, color: Color, critical: bool = false) -> void:
	var label := Label3D.new()
	label.text = text
	label.position = point
	label.font_size = 64 if critical else 48
	label.pixel_size = 0.009
	label.modulate = color
	label.outline_modulate = Color("152025")
	label.outline_size = 12
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	add_child(label)
	var tween := create_tween().set_parallel()
	tween.tween_property(label, "position:y", point.y + 1.5, 0.9)
	tween.tween_property(label, "modulate:a", 0.0, 0.9).set_delay(0.15)
	tween.chain().tween_callback(label.queue_free)
