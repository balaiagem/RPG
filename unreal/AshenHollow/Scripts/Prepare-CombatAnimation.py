"""Create project-owned attack copies with contact notifies; preserve source assets."""
import unreal

library = unreal.AnimationLibrary
notify_class = unreal.load_class(None, '/Script/AshenHollow.AHNotify_MeleeImpact')
assert notify_class, 'Compile the editor target before preparing animations.'
for name, contact in [('MM_Attack_01', 12 / 30), ('MM_Attack_02', 14 / 30)]:
    source = '/Game/Characters/Mannequins/Anims/Unarmed/Attack/' + name
    destination = '/Game/AshenHollow/Animation/' + name
    exists = unreal.EditorAssetLibrary.does_asset_exist(destination)
    asset = unreal.load_asset(destination) if exists else unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    assert asset, destination
    if exists:
        # Never silently replace manually authored timing on a subsequent run.
        events = library.get_animation_notify_events_for_track(asset, 'AH_Contact')
        assert len(events) == 1, 'Existing contact track needs manual review: ' + destination
        unreal.log('AH_ANIMATION_PRESERVED ' + destination)
        continue
    library.add_animation_notify_track(asset, 'AH_Contact')
    notify = library.add_animation_notify_event(asset, 'AH_Contact', contact, notify_class)
    assert notify, 'Could not create contact notify'
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log('AH_ANIMATION_AUTHORED {} contact={:.6f}'.format(destination, contact))
