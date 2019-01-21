/**
 *  @file
 *  @copyright defined in LICENSE
 */

#include "token.hpp"

namespace eosio {

void token::create( name   issuer,
                    asset  maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::update( name  issuer,
                    asset maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before update" );
    const auto& st = *existing;

    eosio_assert( st.supply.amount <= maximum_supply.amount, "max-supply cannot be less than available supply");
    eosio_assert( maximum_supply.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
      s.max_supply    = maximum_supply;
      s.issuer        = issuer;
    });
}


void token::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer, true );

    if( to != st.issuer ) {
      SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "issuer"_n} },
                          { st.issuer, to, quantity, memo }
      );
    }
}

void token::transfer( name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    do_claim( from, quantity.symbol, from );
    sub_balance( from, quantity );
    add_balance( to, quantity, from, from != st.issuer );

    //account needs to exist first, dont auto claim when issuer
    if(from != st.issuer) {
      do_claim( to, quantity.symbol, from );
    }
}


void token::claim( name owner, const symbol& sym ) {
  do_claim(owner,sym,owner);
}

void token::do_claim( name owner, const symbol& sym, name payer ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  auto sym_code_raw = sym.code().raw();

  require_auth( payer );
  accounts owner_acnts( _self, owner.value );

  const auto& existing = owner_acnts.get( sym_code_raw, "no balance object found" );
  if( !existing.claimed ) {
    //save the balance
    auto value = existing.balance;
    //erase the table freeing ram to the issuer
    owner_acnts.erase( existing );
    //create a new index
    auto replace = owner_acnts.find( sym_code_raw );
    //confirm there are definitely no balance now
    eosio_assert(replace == owner_acnts.end(), "there must be no balance object" );
    //add the new claimed balance paid by owner
    owner_acnts.emplace( payer, [&]( auto& a ){
      a.balance = value;
      a.claimed = true;
    });
  }
}

void token::recover( name owner, const symbol& sym ) {
  auto sym_code_raw = sym.code().raw();

  stats statstable( _self, sym_code_raw );
  auto existing = statstable.find( sym_code_raw );
  eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
  const auto& st = *existing;

  require_auth( st.issuer );

  //fail gracefully so we dont have to take another snapshot
  accounts owner_acnts( _self, owner.value );
  auto owned = owner_acnts.find( sym_code_raw );
  if( owned != owner_acnts.end() ) {
    if( !owned->claimed ) {
      sub_balance( owner, owned->balance );
      add_balance( st.issuer, owned->balance, st.issuer, true );
    }
  }
}


void token::stake(name owner, asset quantity)
{
  require_auth(owner);

  auto sym_code_raw = quantity.symbol.code().raw();
  stats statstable( _self, sym_code_raw );
  const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );

  eosio_assert( quantity.is_valid(), "invalid quantity" );
  eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
  eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

  //we allow any token on this contract to be staked individually
  accounts from_acnts(_self, owner.value);
  const auto &from = from_acnts.get(sym_code_raw, "no balance object found");

  //find staked + refunding balance
  uint64_t unavailable = 0;

  staking owner_stake(_self, owner.value);
  auto stk = owner_stake.find(sym_code_raw);
  if (stk != owner_stake.end())
  {
    unavailable += stk->quantity.amount;
  }

  refunding owner_refund(_self, owner.value);
  auto ref = owner_refund.find(sym_code_raw);
  if (ref != owner_refund.end())
  {
    unavailable += ref->quantity.amount;
  }

  //confirm the transfer is less than liquid-stake-refunding
  eosio_assert(from.balance.amount - unavailable >= quantity.amount, "overdrawn balance");

  //create the stake record
  if (stk == owner_stake.end())
  {
    //brand new stake
    owner_stake.emplace(owner, [&](auto &a) {
      a.quantity = quantity;
      a.updated_on = now();
    });
  }
  else
  {
    //has an existing stake
    owner_stake.modify(stk, same_payer, [&](auto &a) {
      a.quantity += quantity;
      a.updated_on = now();
    });
  }
}

void token::unstake(name owner, asset quantity)
{
  require_auth(owner);
  auto sym_code_raw = quantity.symbol.code().raw();

  eosio_assert( quantity.is_valid(), "invalid quantity" );
  eosio_assert( quantity.amount > 0, "must transfer positive quantity" );

  accounts from_acnts(_self, owner.value);
  const auto &from = from_acnts.get(sym_code_raw, "no balance object found");

  //get the current stake
  staking owner_stake(_self, owner.value);
  auto stk = owner_stake.find(sym_code_raw);
  eosio_assert(stk != owner_stake.end(), "you have no staked balance");
  eosio_assert(quantity.amount <= stk->quantity.amount, "overdrawn balance");

  //get the refunding table
  refunding owner_refund(_self, owner.value);
  auto ref = owner_refund.find(sym_code_raw);

  //update refunding table
  if (ref == owner_refund.end())
  {
    //brand new refund
    owner_refund.emplace(owner, [&](auto &a) {
      a.quantity = quantity;
      a.updated_on = now();
    });
  }
  else
  {
    owner_refund.modify(ref, same_payer, [&](auto &a) {
      a.quantity += quantity;
      a.updated_on = now();
    });
  }


  if(quantity.amount < stk->quantity.amount) {
    //update the stake
    owner_stake.modify(stk, same_payer, [&](auto &a) {
      a.quantity -= quantity;
      a.updated_on = now();
    });
  } else {
    //remove the stake
    owner_stake.erase(stk);
  }

  //send deferred refund action
  eosio::transaction out;
  out.actions.emplace_back(permission_level{_self, "refund"_n},
    _self, "refund"_n,
    std::make_tuple(owner,quantity.symbol)
  );
  out.delay_sec = sec_for_ref;
  cancel_deferred(owner.value); // TODO: Remove this line when replacing deferred trxs is fixed
  out.send(owner.value, owner, true);
}

void token::refund(name owner, const symbol& sym ) {
  refunding owner_refund(_self, owner.value);
  const auto &ref = owner_refund.get(sym.code().raw(), "no refunding entry found");
  eosio_assert(ref.updated_on + sec_for_ref <= now(), "refund not yet available - you must wait 7 days from last unstake");
  owner_refund.erase(ref);
}

void token::sub_balance( name owner, asset value ) {
  auto sym_code_raw = value.symbol.code().raw();
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( sym_code_raw, "no balance object found" );
   //find staked + refunding balance
   uint64_t unavailable = 0;

   staking owner_stake(_self, owner.value);
   auto stk = owner_stake.find(sym_code_raw);
   if (stk != owner_stake.end())
   {
     unavailable += stk->quantity.amount;
   }

   refunding owner_refund(_self, owner.value);
   auto ref = owner_refund.find(sym_code_raw);
   if (ref != owner_refund.end())
   {
     unavailable += ref->quantity.amount;
   }

   //confirm the transfer is less than liquid-stake-refunding
   eosio_assert(from.balance.amount - unavailable >= value.amount, "overdrawn balance");

   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( name owner, asset value, name ram_payer, bool claimed )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
        a.claimed = claimed;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_DISPATCH(eosio::token, (create)(update)(issue)(transfer)(claim)(recover)(stake)(unstake)(refund) )
