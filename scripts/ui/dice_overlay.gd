class_name DiceOverlay
extends Control
## Original faceted die visualization. Cosmetic animation never touches rules RNG.
var pending: Array[Dictionary] = []
var current: Dictionary = {}
var age := 0.0
var history: Array[Dictionary] = []
var font := ThemeDB.fallback_font

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func present(label: String, roll: int, total: int, target: int, success: bool, detail: String = "") -> void:
	var outcome := {"label":label,"roll":roll,"total":total,"target":target,"success":success,"detail":detail}
	history.append(outcome)
	if history.size() > 50:
		history.pop_front()
	if current.is_empty() or age > 2.3:
		current = outcome
		age = 0
	else:
		pending.append(outcome)
		if pending.size() > 6:
			pending.pop_front()

func _process(delta: float) -> void:
	age += delta
	if age > 3 and not pending.is_empty():
		current = pending.pop_front()
		age = 0
	queue_redraw()

func _draw() -> void:
	if current.is_empty() or age > 6:
		return
	var center := Vector2(90, 640)
	var color := Color("ddb979") if current.success else Color("c87965")
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.025, 0.045, 0.055, 0.97)
	style.border_color = Color("576a64")
	style.set_border_width_all(1)
	style.set_corner_radius_all(8)
	draw_style_box(style, Rect2(30, 562, 470, 162))
	var spin := pow(maxf(0, 1.0 - age / 0.45), 2) * 4
	var phi := (1.0 + sqrt(5.0)) / 2.0
	var vertices := [Vector3(-1,phi,0),Vector3(1,phi,0),Vector3(-1,-phi,0),Vector3(1,-phi,0),Vector3(0,-1,phi),Vector3(0,1,phi),Vector3(0,-1,-phi),Vector3(0,1,-phi),Vector3(phi,0,-1),Vector3(phi,0,1),Vector3(-phi,0,-1),Vector3(-phi,0,1)]
	var faces := [[0,11,5],[0,5,1],[0,1,7],[0,7,10],[0,10,11],[1,5,9],[5,11,4],[11,10,2],[10,7,6],[7,1,8],[3,9,4],[3,4,2],[3,2,6],[3,6,8],[3,8,9],[4,9,5],[2,4,11],[6,2,10],[8,6,7],[9,8,1]]
	for i in range(vertices.size()):
		vertices[i] = (vertices[i] as Vector3).normalized().rotated(Vector3.UP, 0.25 + spin).rotated(Vector3.RIGHT, 0.15 + spin * 0.7)
	faces.sort_custom(func(a: Array, b: Array): return (vertices[a[0]].z + vertices[a[1]].z + vertices[a[2]].z) < (vertices[b[0]].z + vertices[b[1]].z + vertices[b[2]].z))
	for face in faces:
		var points := PackedVector2Array()
		for index in face:
			var v: Vector3 = vertices[index]
			points.append(center + Vector2(v.x, -v.y) * 49)
		var depth: float = (vertices[face[0]].z + vertices[face[1]].z + vertices[face[2]].z) / 3.0
		draw_colored_polygon(points, Color("1a3039").lightened((depth + 1) * 0.055))
		points.append(points[0])
		draw_polyline(points, color.darkened(0.35), 1.1, true)
	draw_circle(center, 19, Color(0.06, 0.13, 0.16, 0.94))
	var number := str(current.roll) if age >= 0.45 else str(1 + int(age * 91) % 20)
	draw_string(font, center + Vector2(-font.get_string_size(number, 0, -1, 29).x / 2, 8), number, 0, -1, 29, Color("fff0cf"))
	draw_string(font, Vector2(154, 594), str(current.label).left(30).to_upper(), 0, -1, 16, color)
	if age >= 0.45:
		var modifier := int(current.total) - int(current.roll)
		draw_string(font, Vector2(154, 631), "%d %+d = %d  /  DC·AC %d" % [current.roll, modifier, current.total, current.target], 0, -1, 20, Color("e4e9dd"))
		draw_string(font, Vector2(154, 662), "SUCCESS" if current.success else "FAILED", 0, -1, 19, color)
		draw_string(font, Vector2(154, 693), str(current.detail).left(40), 0, -1, 13, Color("a9b7ae"))
	else:
		draw_string(font, Vector2(154, 643), "ROLLING…", 0, -1, 20, Color("b9c9c1"))
