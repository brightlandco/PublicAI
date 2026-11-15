Game<public> := module:
    Modes<public> := module:
        capture_zone<public> := class(component):
            @editable
            CaptureTime<public> : float = 10.0

            @editable
            ZoneTrigger<public> : trigger_device = trigger_device{}

            RunCaptureLoop<public>()<suspends> : void =
                loop:
                    Agent : player = ZoneTrigger.AgentEntersEvent.Await()
                    race:
                        block:
                            Sleep(CaptureTime)
                            OnZoneCaptured(Agent)
                        block:
                            ZoneTrigger.AgentExitsEvent.Await()
                            OnCaptureCancelled()

            Start<public>() : void =
                spawn { RunCaptureLoop() }
