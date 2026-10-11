extends SceneTree
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var probe = ClassDB.instantiate("MeowodyServicePerfProbe")
    var passed = probe.run("", "")
    probe = null
    print("[HistoryBaseline] PASS" if passed else "[HistoryBaseline] FAILED")
    quit(0 if passed else 1)
