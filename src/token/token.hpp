/**
 *  @file
 *  @copyright defined in LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <string>


namespace eosio {

   using std::string;

   CONTRACT token : public contract {
      const uint64_t sec_for_ref  = 7*24*60*60; //7 days

      public:
         using contract::contract;

         ACTION create( name issuer,
                      asset maximum_supply);

         ACTION update( name issuer,
                      asset maximum_supply);

         ACTION issue( name to, asset quantity, string memo );
         
         ACTION claim( name owner, const symbol& sym );
         
         ACTION recover( name owner, const symbol& sym );
         
         ACTION transfer( name from,
                        name to,
                        asset        quantity,
                        string       memo );
         
         ACTION stake (name owner, asset quantity);
         
         ACTION unstake (name owner, asset quantity);
         
         ACTION refund (name owner, const symbol& sym);

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
         TABLE account {
            asset    balance;
            bool     claimed = false;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         TABLE currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         TABLE stake_details {
           asset quantity;
           uint64_t updated_on;

           uint64_t primary_key()const { return quantity.symbol.code().raw(); }
         };

         TABLE refund_details {
           asset quantity;
           uint64_t updated_on;

           uint64_t primary_key()const { return quantity.symbol.code().raw(); }
         };

         typedef eosio::multi_index< "accounts"_n, account> accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats> stats;
         typedef eosio::multi_index< "stake"_n, stake_details> staking;
         typedef eosio::multi_index< "refund"_n, refund_details> refunding;
         
         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer, bool claimed );
         void do_claim( name owner, const symbol& sym, name payer );
   };

} /// namespace eosio
