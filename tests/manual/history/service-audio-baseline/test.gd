extends SceneTree
func _initialize():
    call_deferred("run_tests")
func run_tests():
    var probe = ClassDB.instantiate("MeowodyServicePerfProbe")
    var passed = probe.run(OS.get_environment("MEOWODY_PERF_HTTP"), OS.get_environment("MEOWODY_PERF_SHA"))
    probe = null
    # Give the dummy audio mixer time to retire the benchmark's playbacks.
    await create_timer(0.25).timeout
    print("[ServicePerf] PASS" if passed else "[ServicePerf] FAILED")
    quit(0 if passed else 1)
