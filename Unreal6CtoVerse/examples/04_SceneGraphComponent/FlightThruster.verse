Game<public> := module:
    Flight<public> := module:
        thruster_telemetry<public> := struct:
            ThrustNewtons<public> : float
            FuelRemaining<public> : float

        thruster_component<public> := class(component):
            @editable
            MaxThrust<public> : float = 45000.0

            @editable
            BurnRate<public> : float = 2.5

            var<private> Fuel : float = 100.0
            var<private> LastTelemetry : ?thruster_telemetry = false

            ApplyThrottle<public>(Throttle : float, Dt : float)<decides><transacts> : thruster_telemetry =
                Throttle > 0.0 and Fuel > 0.0
                Clamped : float = Clamp(Throttle, 0.0, 1.0)
                FuelUsed : float = BurnRate * Clamped * Dt
                Fuel >= FuelUsed
                set Fuel -= FuelUsed
                Telemetry : thruster_telemetry = thruster_telemetry:
                    ThrustNewtons := MaxThrust * Clamped
                    FuelRemaining := Fuel
                set LastTelemetry = option{Telemetry}
                return Telemetry

            GetLastThrustOrZero<public>()<reads> : float =
                if (Info := LastTelemetry?):
                    return Info.ThrustNewtons
                return 0.0
