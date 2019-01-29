# parsl.token
The PARSL SEED token based on claimable.token https://github.com/airdropsdac/claimable.token from AirdropsDAC that allows a more rapid recovery of RAM. 

Additioanl features include stake, unstake, reclaim, and "airdrop into staked state".

The accounts table shows the full SEED balance at all times regardless of staked, refunding, or liquid state.

# How does this token recover ram
Added to the balance table is the claimed boolean flag.

When the token is issued claimed = false and the RAM is paid by the issuer.

When the token is transferred the balance row is dropped and re-added using the RAM of the owner.

Tokens can also be explicitly claimed using the ```claim``` action, if the owner doesn't want to transfer.

In theory, unclaimed tokens can be ```recover```ed after some claiming period to have guaranteed RAM recovery.

# New functions

* stake

Allows a user to lock SEED into a staked state for the purpose of the Reward Drop and future functionality. Once staked that portion of SEED cannot be transferred to another user.

```cleos push action parslseed123 stake '{"owner":"user1","quantity":"1000.0000 SEED"}'```

* unstake

Allows a user to begin unstaking SEED so that it can become transferable again. This process takes 7 days. A deferred `refund` action is sent after 7 days, or can be called manually if the deferred transaction fails.

```cleos push action parslseed123 unstake '{"owner":"user1","quantity":"1000.0000 SEED"}'```

* refund

Makes refunding SEED available to transfer after the 7 days unstaking time. Called automatically by deferred transaction, but can be used as a fallback.

```cleos push action parslseed123 refund '{"owner":"user1"}'```

* rewarddrop

Transfers SEED from parslseed123 to the recipient of a reward drop, and places the new tokens immediately into a staked state.

```cleos push action parslseed123 transfer '{"from":"parslseed123", "to":"user1", "quantity":"1000.0000 SEED, "memo":"January RewardDrop"}'```


* update

This action uses the same ABI as create and allows you to change the issuer and/or max supply of a token

Only the token contract itself can authorize this action, not the previous issuer

```cleos push action parslseed123 update '{"issuer":"newissuer","maximum_supply":"100000.0000 SEED"}'```

* claim

This action allows a token balance owner to claim their token without transferring it

This is only really neccessary if the token contract will recover unclaimed Tokens

```cleos push action parslseed123 claim '{"owner":"user1","sym":"4,SEED"}'```

* recover

This action allows the token contract to recover unclaimed tokens, thus freeing RAM

The unclaimed tokens are added to the current issuer balance

This command only does something if the balance is truly unclaimed, otherwise it does nothing

```cleos push action parslseed123 recover '{"owner":"user1","sym":"4,SEED"}'```

# New Tables

* stake

Displays the staked SEED balance for an account. Just like accounts this is scoped to the eos account.

```
asset quantity;         //how much SEED is staked
uint64_t updated_on;    //when the stake was last updated
```

* refund

Displays how much SEED is currently refunding. `updated_on` is when the unstake request was made in linux epoch time in seconds. The refund is available 7 days after this timestamp.

```
asset quantity;         //how much SEED is staked
uint64_t updated_on;    //when the stake was last updated
```

# Test Driven Development - Unit Testing

Thanks to https://github.com/azarusio/AZAkazam for providing a Mocha nodejs based unit testing framework. To use the unit tests simply run the following steps:

* You'll need to have the EOSIO.CDT toolkit installed. You can find the setup instructions here: https://developers.eos.io/eosio-home/v1.7.0/docs/installing-the-contract-development-toolkit

* To use the default hello world test template, you'll need a local Nodeos image.
You can set one up with docker by running it with `source reset_env.sh -d`
Or you may use the locally installed instance `source reset_env.sh -l`

* You'll need node v10 - installation instructions can be found here: https://nodejs.org/en/download/package-manager/
At the root of this repo, run `npm i` to install dependencies

You can then run the tests by calling `npm run test`


