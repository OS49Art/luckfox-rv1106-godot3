extends Control
# Original geometric demo, no product assets or bundled font files.
signal return_requested
var selected_slot = -1
var selected_tab = 0
var info
var overlay
var smoke_frames = 0

func flat(color):
    var style = StyleBoxFlat.new()
    style.bg_color = color
    return style

func button(text, box, method, args = []):
    var node = Button.new()
    node.text = text
    node.rect_position = box.position
    node.rect_size = box.size
    node.add_stylebox_override("normal", flat(Color(0.18, 0.27, 0.39)))
    node.add_stylebox_override("hover", flat(Color(0.24, 0.36, 0.50)))
    node.add_stylebox_override("pressed", flat(Color(0.12, 0.56, 0.55)))
    node.add_stylebox_override("focus", StyleBoxEmpty.new())
    node.connect("pressed", self, method, args)
    add_child(node)
    return node

func _ready():
    rect_size = Vector2(360, 360)
    var bg = ColorRect.new()
    bg.color = Color(0.07, 0.1, 0.16)
    bg.rect_size = Vector2(360, 360)
    bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(bg)
    button("Back", Rect2(8, 8, 64, 32), "go_back")
    info = Label.new()
    info.text = "CPU Canvas / 360 x 360"
    info.rect_position = Vector2(84, 14)
    add_child(info)
    for i in range(8):
        button("Slot " + str(i + 1), Rect2(12 + (i % 4) * 86, 62 + int(i / 4) * 64, 78, 54), "select_slot", [i])
    for i in range(3):
        button("Tab " + str(i + 1), Rect2(12 + i * 116, 204, 104, 36), "select_tab", [i])
    button("Details", Rect2(12, 256, 162, 42), "show_details")
    button("Action", Rect2(186, 256, 162, 42), "action")
    overlay = button("Details - tap to close", Rect2(28, 82, 304, 112), "hide_details")
    overlay.hide()
    for arg in OS.get_cmdline_args():
        if arg == "--smoke": smoke_frames = 5
    print("DEMO_READY slots=8 tabs=3 logical=360")

func select_slot(index):
    selected_slot = index
    info.text = "Slot " + str(index + 1)
    print("SLOT ", index)

func select_tab(index):
    selected_tab = index
    info.text = "Tab " + str(index + 1)
    print("TAB ", index)

func show_details():
    overlay.show()

func hide_details():
    overlay.hide()

func action():
    info.text = "Action: slot " + str(selected_slot + 1)
    print("ACTION")

func go_back():
    emit_signal("return_requested")
    print("DEMO_RETURN_REQUESTED")
    get_tree().quit()

func _process(_delta):
    if smoke_frames > 0:
        smoke_frames -= 1
        if smoke_frames == 0:
            print("DEMO_SMOKE_PASS")
            get_tree().quit()
