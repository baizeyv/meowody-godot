extends SceneTree
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var probe = ClassDB.instantiate("MeowodyServicePerfProbe")
    var passed = probe.run()
    probe = null
    await create_timer(0.25).timeout
    print("[ServicePerf] PASS" if passed else "[ServicePerf] FAILED")
    quit(0 if passed else 1)
