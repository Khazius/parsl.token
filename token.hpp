/**
 *  @file
 *  @copyright defined in LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>
#include <string>


namespace eosio {

   using std::string;

   class [[eosio::contract("parslseed123")]] token : public contract {
      const uint64_t sec_for_ref  = 7*24*60*60; //7 days

      public:
         using contract::contract;
         [[eosio::action]]
         void create( name issuer,
                      asset maximum_supply);
         [[eosio::action]]
         void update( name issuer,
                      asset maximum_supply);
         [[eosio::action]] void issue( name to, asset quantity, string memo );
         [[eosio::action]] void claim( name owner, const symbol& sym );
         [[eosio::action]] void recover( name owner, const symbol& sym );
         [[eosio::action]]
         void transfer( name from,
                        name to,
                        asset        quantity,
                        string       memo );
         [[eosio::action]] void stake (name owner, asset quantity);
         [[eosio::action]] void unstake (name owner, asset quantity);
         [[eosio::action]] void refund (name owner, const symbol& sym);

         static asset get_supply( name token_contract_account, symbol_code sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }
      private:
         struct [[eosio::table]] account {
            asset    balance;
            bool     claimed = false;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[eosio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         struct [[eosio::table]] stake_details {
           asset quantity;
           uint64_t updated_on;
           uint64_t primary_key()const { return quantity.symbol.code().raw(); }
         };

         struct [[eosio::table]] refund_details {
           asset quantity;
           uint64_t updated_on;
           uint64_t primary_key()const { return quantity.symbol.code().raw(); }
         };

         typedef eosio::multi_index<name("accounts"), account> accounts;
         typedef eosio::multi_index<name("stat"), currency_stats> stats;
         typedef eosio::multi_index<name("stake"), stake_details> staking;
         typedef eosio::multi_index<name("refund"), refund_details> refunding;
         
         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer, bool claimed );
         void do_claim( name owner, const symbol& sym, name payer );
      public:
         struct transfer_args {
            name     from;
            name     to;
            asset    quantity;
            string   memo;
         };
   };

} /// namespace eosio
