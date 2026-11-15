Game<public> := module:
    Intro<public> := module:
        greeter<public> := class:
            Prefix<public> : string = "Welcome to UE6"
            var<private> GreetCount : int = 0

            FormatGreeting<public>(PlayerName : string)<transacts> : string =
                set GreetCount += 1
                return "{Prefix}, {PlayerName}! (Visit #{GreetCount})"

            GetTotalGreets<public>()<reads> : int =
                return GreetCount
