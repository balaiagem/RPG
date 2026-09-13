class_name CharacterBuild
extends RefCounted
const STATS := ["strength", "dexterity", "constitution", "intelligence", "wisdom", "charisma"]
const ARRAY := [15, 14, 13, 12, 10, 8]
const SKILL_STATS := {"athletics":"strength","acrobatics":"dexterity","sleight_of_hand":"dexterity","stealth":"dexterity","arcana":"intelligence","history":"intelligence","investigation":"intelligence","nature":"intelligence","religion":"intelligence","animal_handling":"wisdom","insight":"wisdom","medicine":"wisdom","perception":"wisdom","survival":"wisdom","deception":"charisma","intimidation":"charisma","performance":"charisma","persuasion":"charisma"}

static func catalog(file: String) -> Dictionary:
	return JSON.parse_string(FileAccess.get_file_as_string("res://data/" + file + ".json"))

static func default_choice(class_id: String = "fighter") -> Dictionary:
	var c: Dictionary = catalog("classes_5e")[class_id]
	var priority: Array = c.primaryAttributes.duplicate()
	for stat in ["constitution", "dexterity"] + STATS:
		if not stat in priority:
			priority.append(stat)
	var scores := {}
	for i in range(6):
		scores[priority[i]] = ARRAY[i]
	return {"name":"Ash","class":class_id,"ancestry":"human","scores":scores,"skills":c.skillChoices.slice(0, c.skillCount),"spells":c.abilities.duplicate(),"color":c.color}

static func validate(choice: Dictionary) -> String:
	if not catalog("classes_5e").has(choice.get("class", "")) or not catalog("ancestries").has(choice.get("ancestry", "")):
		return "Choose a valid class and ancestry."
	var scores: Dictionary = choice.get("scores", {})
	if scores.size() != 6:
		return "Assign all six attributes."
	var values: Array = []
	for stat in STATS:
		values.append(int(scores.get(stat, -100)))
	values.sort()
	if values != [8, 10, 12, 13, 14, 15]:
		return "Use each standard-array value exactly once."
	var c: Dictionary = catalog("classes_5e")[choice["class"]]
	var selected: Array = choice.get("skills", [])
	if selected.size() != int(c.skillCount):
		return "Choose %d class skills." % c.skillCount
	var seen := {}
	for skill in selected:
		if not skill in c.skillChoices or seen.has(skill):
			return "Choose distinct class skills."
		seen[skill] = true
	if not c.spellChoices.is_empty():
		var spells: Array = choice.get("spells", [])
		if spells.size() != 2 or spells[0] == spells[1]:
			return "Prepare a cantrip and a first-level spell."
		var definitions := catalog("abilities_5e")
		for i in range(2):
			if not spells[i] in c.spellChoices or int(definitions[spells[i]].level) != i:
				return "Prepare a supported cantrip and first-level spell."
	return ""

static func derive(choice: Dictionary) -> Dictionary:
	var c: Dictionary = catalog("classes_5e")[choice["class"]]
	var a: Dictionary = catalog("ancestries")[choice.ancestry]
	var scores: Dictionary = choice.scores.duplicate()
	for stat in STATS:
		scores[stat] = int(scores[stat]) + int(a.bonuses.get(stat, 0))
	var dex := CombatRules.modifier(scores.dexterity)
	var con := CombatRules.modifier(scores.constitution)
	var ac := 10 + dex
	match c.armor:
		"chain_shield": ac = 18
		"scale": ac = 14 + mini(dex, 2)
		"leather": ac = 11 + dex
		"leather_shield": ac = 13 + dex
		"unarmored_con": ac = 10 + dex + con
		"unarmored_wis": ac = 10 + dex + CombatRules.modifier(scores.wisdom)
		"draconic": ac = 13 + dex
	if "Fighting Style: Defense" in c.passives:
		ac += 1
	var hp := int(c.hitDie) + con + (1 if "dwarven_toughness" in a.traits else 0) + (1 if "Draconic Resilience" in c.passives else 0)
	var slots: Array = choice.spells.duplicate() if not c.spellChoices.is_empty() else c.abilities.duplicate()
	if choice["class"] == "bard":
		slots.append("bardic_inspiration")
	if "breath_weapon" in a.traits:
		slots.append("breath_weapon")
	for id in ["attack", "dodge", "dash", "hide"]:
		if slots.size() < 4 and not id in slots:
			slots.append(id)
	slots = slots.slice(0, 4)
	var skills: Array = choice.skills.duplicate()
	if "keen_senses" in a.traits and not "perception" in skills:
		skills.append("perception")
	var stats := {"id":"created_hero","name":str(choice.name).strip_edges().left(24),"class":choice["class"],"ancestry":choice.ancestry,"level":1,"xp":0,"attributes":scores,"hp":hp,"mana":100,"armorClass":ac,"speed":6.0 * float(a.speed) / 30.0,"weapon":c.weapon,"abilities":slots,"skills":skills,"traits":a.traits,"passives":c.passives,"castingStat":c.castingStat,"spellSlots":c.spellSlots,"armorType":c.armor,"color":choice.get("color", c.color),"scale":a.scale}
	if stats.name.is_empty():
		stats.name = "Ash"
	return stats

static func weapon(id: String) -> Dictionary:
	var definitions := {"longsword":["Longsword","1d8","strength",2.1,"slashing"],"greataxe":["Greataxe","1d12","strength",2.1,"slashing"],"rapier":["Rapier","1d8","dexterity",2.1,"piercing"],"mace":["Mace","1d6","strength",2.1,"bludgeoning"],"staff":["Quarterstaff","1d6","strength",2.1,"bludgeoning"],"unarmed":["Unarmed strike","1d4","dexterity",2.1,"bludgeoning"],"longbow":["Longbow","1d8","dexterity",30.0,"piercing"]}
	var d: Array = definitions[id]
	return {"id":id,"name":d[0],"damageDice":d[1],"statScaling":d[2],"attackRange":d[3],"damageType":d[4],"attackSpeed":6.0,"damageBonus":0}

static func save_choice(choice: Dictionary, path: String = "user://character.json") -> Error:
	if not validate(choice).is_empty():
		return ERR_INVALID_DATA
	var file := FileAccess.open(path + ".tmp", FileAccess.WRITE)
	if file == null:
		return FileAccess.get_open_error()
	file.store_string(JSON.stringify({"schemaVersion":1,"choice":choice}))
	file.close()
	return DirAccess.rename_absolute(path + ".tmp", path)

static func load_choice(path: String = "user://character.json") -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var parser := JSON.new()
	if parser.parse(FileAccess.get_file_as_string(path)) != OK:
		return {}
	var data: Variant = parser.data
	if not data is Dictionary or data.get("schemaVersion") != 1 or not data.get("choice") is Dictionary:
		return {}
	return data.choice if validate(data.choice).is_empty() else {}
