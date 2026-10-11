extends SceneTree

func check(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
        return false
    return true

func _initialize():
    call_deferred("run_tests")

func run_tests():
    var view = load("res://yatzy.tscn").instantiate()
    root.add_child(view)
    await process_frame
    await process_frame
    var roll = view.find_child("Roll", true, false)
    var score = view.find_child("Score", true, false)
    var die = view.find_child("Die0", true, false)
    if not check(score.disabled and die.disabled, "Initial UI must require a roll"): return
    roll.emit_signal("pressed")
    die.emit_signal("pressed")
    var held_text = die.text
    roll.emit_signal("pressed")
    if not check(die.text == held_text and "已保留" in held_text, "Held die changed"): return
    roll.emit_signal("pressed")
    if not check(roll.disabled, "Fourth roll should be disabled"): return
    view.find_child("Category12", true, false).emit_signal("pressed")
    if not check(not score.disabled and "SCORE  +" in score.text, "Score selection failed"): return
    await process_frame
    if "--visual" in OS.get_cmdline_user_args():
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://ui.png")
    score.emit_signal("pressed")
    if not check(score.disabled and not roll.disabled and die.disabled, "Next round did not reset"): return
    for round_index in range(12):
        roll.emit_signal("pressed")
        var selected = false
        for category in range(13):
            var row = view.find_child("Category%d" % category, true, false)
            if not row.disabled:
                row.emit_signal("pressed")
                selected = true
                break
        if not check(selected and not score.disabled, "No legal scoring option"): return
        score.emit_signal("pressed")
    if not check(roll.disabled and score.disabled, "Game should finish after 13 rounds"): return
    view.on_restart()
    if not check(not roll.disabled and score.disabled and die.disabled, "Restart failed"): return
    view.on_settings()
    view.on_restart()
    view.on_back()
    view.queue_free()
    await process_frame
    print("Yahtzee UI: PASS")
    quit(0)
