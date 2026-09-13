class_name RoundResources
extends Node
## Independent real-time action economy and level-one resource state.
var actions := {"action":0.0,"bonus":0.0,"reaction":0.0}
var pools := {}
var max_slots := 0
var slots := 0
var class_id := ""
var dodge_time := 0.0
var rage_time := 0.0
var combat_activity := 0.0
var hidden := false
var concentration := ""
var concentration_time := 0.0
var sneak_time := 0.0
var martial_window := 0.0
var last_bonus_spell := 0.0
var last_leveled_action := 0.0
var agathys := false
var rebuke_armed := false
var mage_armor := false
var endurance_used := false
var hit_die := true
var arcane_recovered := false

func configure(data: Dictionary) -> void:
	class_id = data["class"]
	max_slots = int(data.spellSlots)
	slots = max_slots
	pools = {"second_wind":1,"rage":2,"lay_hands":5,"divine_sense":maxi(1, 1 + CombatRules.modifier(data.attributes.charisma)),"breath":1}
	pools.inspiration = maxi(1, CombatRules.modifier(data.attributes.charisma))

func _process(delta: float) -> void:
	for key in actions:
		actions[key] = maxf(0, actions[key] - delta)
	dodge_time = maxf(0, dodge_time - delta)
	combat_activity = maxf(0, combat_activity - delta)
	rage_time = maxf(0, rage_time - delta) if combat_activity > 0 else 0.0
	sneak_time = maxf(0, sneak_time - delta)
	martial_window = maxf(0, martial_window - delta)
	last_bonus_spell = maxf(0, last_bonus_spell - delta)
	last_leveled_action = maxf(0, last_leveled_action - delta)
	concentration_time = maxf(0, concentration_time - delta)
	if concentration_time == 0:
		concentration = ""

func available(kind: String) -> bool:
	return kind == "free" or actions.get(kind, 0.0) <= 0

func spend(kind: String) -> void:
	if kind != "free":
		actions[kind] = 6.0

func short_rest() -> void:
	pools.second_wind = 1
	pools.breath = 1
	if class_id == "warlock":
		slots = max_slots
	if class_id == "wizard" and not arcane_recovered and slots < max_slots:
		slots = mini(max_slots, slots + 1)
		arcane_recovered = true

func long_rest(data: Dictionary) -> void:
	configure(data)
	for key in actions:
		actions[key] = 0.0
	dodge_time = 0
	rage_time = 0
	hidden = false
	concentration = ""
	concentration_time = 0
	agathys = false
	mage_armor = false
	endurance_used = false
	hit_die = true
	arcane_recovered = false
