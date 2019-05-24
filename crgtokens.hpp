/**
*  @file
*  @copyright defined in eos/LICENSE.txt
*/
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
	class system_contract;
}

using namespace eosio;

namespace crg {

	using std::string;

	class crgtokens : public contract {
	public:
		crgtokens(name self) :contract(self) {}

		void create(name issuer,
			asset        maximum_supply,
			bool         transfer_locked);

		void issue(name to, asset quantity, string memo);

		void unlock(asset unlock);

		void transfer(name from,
			name to,
			asset        quantity,
			string       memo);


		inline asset get_supply(symbol_name sym)const;

		inline asset get_balance(name owner, symbol_name sym)const;

	private:
		struct account {
			asset    balance;

			uint64_t primary_key()const { return balance.symbol.name(); }
		};

		struct currency_stats {
			asset          supply;
			asset          max_supply;
			name   issuer;
			bool         transfer_locked;

			uint64_t primary_key()const { return supply.symbol.name(); }
		};

		typedef eosio::multi_index<N(accounts), account> accounts;
		typedef eosio::multi_index<N(stat), currency_stats> stats;

		void sub_balance(name owner, asset value);
		void add_balance(name owner, asset value, name ram_payer);

	public:
		struct transfer_args {
			name  from;
			name  to;
			asset         quantity;
			string        memo;
		};
	};

	asset crgtokens::get_supply(symbol_name sym)const
	{
		stats statstable(_self, sym);
		const auto& st = statstable.get(sym);
		return st.supply;
	}

	asset crgtokens::get_balance(name owner, symbol_name sym)const
	{
		accounts accountstable(_self, owner);
		const auto& ac = accountstable.get(sym);
		return ac.balance;
	}

} /// namespace crg