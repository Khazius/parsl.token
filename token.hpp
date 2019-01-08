/**
 *  @file
 *  @copyright defined in LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
//#include <eosiolib/symbol.hpp>
#include <eosiolib/transaction.hpp>
#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class token : public contract {
     const uint64_t          sec_for_ref  = 7*24*3600; //7 days

      public:
         token( account_name self ):contract(self){}


         [[eosio::action]]
         void create( account_name issuer,
                      asset        maximum_supply);
         [[eosio::action]]
         void update( account_name issuer,
                      asset        maximum_supply);
         [[eosio::action]] void issue( account_name to, asset quantity, string memo );
         [[eosio::action]] void claim( account_name owner, symbol_type sym );
         [[eosio::action]] void recover( account_name owner, symbol_type sym );
         [[eosio::action]]
         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );
        [[eosio::action]] void stake (account_name owner, asset quantity);    //stake LLG
        [[eosio::action]] void unstake (account_name owner, asset quantity);  //unstake LLG
        [[eosio::action]] void refund (account_name owner, symbol_type sym);
        void signup( account_name owner, symbol_type sym );
         inline asset get_supply( symbol_name sym )const;
         inline asset get_balance( account_name owner, symbol_name sym )const;

      private:
         struct [[eosio::table]] account {
            asset    balance;
            bool     claimed = false;
            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct [[eosio::table]] currency_stats {
            asset          supply;
            asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         struct [[eosio::table]] stake_details {
           asset quantity;
           uint64_t updated_on;
           uint64_t primary_key()const { return quantity.symbol.name(); }
         };

         struct [[eosio::table]] refund_details {
           asset quantity;
           uint64_t updated_on;
           uint64_t primary_key()const { return quantity.symbol.name(); }
         };

         struct st_signups {
           uint64_t count;
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;
         typedef eosio::multi_index<N(stake), stake_details> staking;
         typedef eosio::multi_index<N(refund), refund_details> refunding;
         typedef eosio::singleton<N(signups), st_signups> signups;

         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer, bool claimed );
         void do_claim( account_name owner, symbol_type sym, account_name payer );
      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            asset         quantity;
            string        memo;
         };
   };

   asset token::get_supply( symbol_name sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   asset token::get_balance( account_name owner, symbol_name sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

} /// namespace eosio
