Game<public> := module:
  Economy<public> := module:
    wallet<public> := class:
      var<private> Gold : int = 100
      var<private> Gems : int = 5

      GetGold<public>()<reads> : int =
        return Gold

      PurchaseItem<public>(Buyer : player, GoldCost : int, GemCost : int)<decides><transacts> : void =
        if (Gold < GoldCost or Gems < GemCost):
          false?
        set Gold -= GoldCost
        set Gems -= GemCost
        GrantItemToPlayer[Buyer]

    var<private> PlayerWallets : weak_map(player, wallet) = map{}

    GetOrCreateWallet<public>(P : player)<transacts> : wallet =
      if (Existing := PlayerWallets[P]):
        return Existing
      var Created : any = wallet{}
      set PlayerWallets[P] = Created
      return Created
