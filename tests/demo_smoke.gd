extends SceneTree
func _init():
    call_deferred("run")
func run():
    var scene = load("res://main.tscn").instance()
    get_root().add_child(scene)
    scene.select_slot(7)
    scene.select_tab(2)
    scene.show_details()
    if scene.selected_slot != 7 or scene.selected_tab != 2 or not scene.overlay.visible:
        print("DEMO_TEST_FAIL")
        quit(1)
        return
    scene.hide_details()
    scene.action()
    if scene.overlay.visible:
        quit(1)
        return
    print("DEMO_BEHAVIOR_PASS")
    scene.go_back()
