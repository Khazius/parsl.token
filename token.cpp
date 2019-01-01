/**
 *  @file
 *  @copyright defined in LICENSE
 */

#include "token.hpp"

namespace eosio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::update( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before update" );
    const auto& st = *existing;

    eosio_assert( st.supply.amount <= maximum_supply.amount, "max-supply cannot be less than available supply");
    eosio_assert( maximum_supply.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, 0, [&]( auto& s ) {
      s.max_supply    = maximum_supply;
      s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer, true );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

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

void token::claim( account_name owner, symbol_type sym ) {
  do_claim(owner,sym,owner);
}

void token::do_claim( account_name owner, symbol_type sym, account_name payer ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  auto sym_name = sym.name();

  require_auth( payer );
  accounts owner_acnts( _self, owner );

  const auto& existing = owner_acnts.get( sym_name, "no balance object found" );
  if( !existing.claimed ) {
    //save the balance
    auto value = existing.balance;
    //erase the table freeing ram to the issuer
    owner_acnts.erase( existing );
    //create a new index
    auto replace = owner_acnts.find( sym_name );
    //confirm there are definitely no balance now
    eosio_assert(replace == owner_acnts.end(), "there must be no balance object" );
    //add the new claimed balance paid by owner
    owner_acnts.emplace( payer, [&]( auto& a ){
      a.balance = value;
      a.claimed = true;
    });
  }
}

void token::recover( account_name owner, symbol_type sym ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  auto sym_name = sym.name();

  stats statstable( _self, sym_name );
  auto existing = statstable.find( sym_name );
  eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
  const auto& st = *existing;

  require_auth( st.issuer );

  //fail gracefully so we dont have to take another snapshot
  accounts owner_acnts( _self, owner );
  auto owned = owner_acnts.find( sym_name );
  if( owned != owner_acnts.end() ) {
    if( !owned->claimed ) {
      sub_balance( owner, owned->balance );
      add_balance( st.issuer, owned->balance, st.issuer, true );
    }
  }
}


void token::stake(account_name owner, asset quantity)
{
  require_auth(owner);
  auto sym = quantity.symbol;
  eosio_assert(sym.is_valid(), "invalid symbol name");
  eosio_assert(quantity.amount > 0, "must specify positive quantity");

  //we allow any token on this contract to be staked individually
  accounts from_acnts(_self, owner);
  const auto &from = from_acnts.get(sym.name(), "no balance object found");

  //find staked + refunding balance
  uint64_t unavailable = 0;

  staking owner_stake(_self, owner);
  auto stk = owner_stake.find(sym.name());
  if (stk != owner_stake.end())
  {
    unavailable += stk->quantity.amount;
  }

  refunding owner_refund(_self, owner);
  auto ref = owner_refund.find(sym.name());
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
    owner_stake.modify(stk, 0, [&](auto &a) {
      a.quantity += quantity;
      a.updated_on = now();
    });
  }
}

void token::unstake(account_name owner, asset quantity)
{
  require_auth(owner);
  auto sym = quantity.symbol;
  eosio_assert(sym.is_valid(), "invalid symbol name");
  eosio_assert(quantity.amount > 0, "must specify positive quantity");

  accounts from_acnts(_self, owner);
  const auto &from = from_acnts.get(sym.name(), "no balance object found");

  //get the current stake
  staking owner_stake(_self, owner);
  auto stk = owner_stake.find(sym.name());
  eosio_assert(stk != owner_stake.end(), "you have no staked balance");
  eosio_assert(quantity.amount <= stk->quantity.amount, "overdrawn balance");

  //get the refunding table
  refunding owner_refund(_self, owner);
  auto ref = owner_refund.find(sym.name());

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
    owner_refund.modify(ref, 0, [&](auto &a) {
      a.quantity += quantity;
      a.updated_on = now();
    });
  }


  if(quantity.amount < stk->quantity.amount) {
    //update the stake
    owner_stake.modify(stk, 0, [&](auto &a) {
      a.quantity -= quantity;
      a.updated_on = now();
    });
  } else {
    //remove the stake
    owner_stake.erase(stk);
  }

  //send deferred refund action
  eosio::transaction out;
  out.actions.emplace_back(permission_level{_self, N(refunder)}, _self, N(refund), owner);
  out.delay_sec = sec_for_ref;
  cancel_deferred(owner); // TODO: Remove this line when replacing deferred trxs is fixed
  out.send(owner, owner, true);
}

void token::refund(account_name owner, symbol_type sym ) {
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  refunding owner_refund(_self, owner);
  const auto &ref = owner_refund.get(sym.name(), "no refunding entry found");
  eosio_assert(ref.updated_on + sec_for_ref <= now(), "refund not yet available - you must wait 3 days from last unstake");
  owner_refund.erase(ref);
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   //find staked + refunding balance
   uint64_t unavailable = 0;

   staking owner_stake(_self, owner);
   auto stk = owner_stake.find(value.symbol.name());
   if (stk != owner_stake.end())
   {
     unavailable += stk->quantity.amount;
   }

   refunding owner_refund(_self, owner);
   auto ref = owner_refund.find(value.symbol.name());
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

void token::add_balance( account_name owner, asset value, account_name ram_payer, bool claimed )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
        a.claimed = claimed;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(update)(issue)(transfer)(claim)(recover)(stake)(unstake)(refund) )
