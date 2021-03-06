#include "crgicoico.hpp"

namespace crg {

	void crgicoico::setapp(name application)
	{
		require_auth(_self);
		require_auth(application);

		eosio_assert(!configs(_self, _self).exists(), "Configuration already exists");
		configs(_self, _self).set(config{ application }, application);
	}

	void crgicoico::setico(name crgoracle, uint32_t icostarttime, uint32_t icoendtime)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		require_auth(configs(_self, _self).get().application);

		icoconfigs(_self, _self).set(icoconfig{crgoracle,  icostarttime,  icoendtime }, configs(_self, _self).get().application);
	}

	void crgicoico::addauser(uint64_t ico_id, name user)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured.");
		require_auth(configs(_self, _self).get().application);

		auto iter = allowedicos.find(ico_id);

		if (iter == allowedicos.end()) {

			allowedicos.emplace(configs(_self, _self).get().application, [&](auto& row) {
				row.ico_id = ico_id;
				row.user = user;
			});
		}
		else{
			allowedicos.modify(iter, configs(_self, _self).get().application, [&](auto& row) {
				row.user = user;
			});
		}
	}

	void crgicoico::addausers(vector<allowedico> users)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured.");
		require_auth(configs(_self, _self).get().application);

		for (auto& n : users)
		{
			auto iter = allowedicos.find(n.ico_id);

			if (iter == allowedicos.end()) {

				allowedicos.emplace(configs(_self, _self).get().application, [&](auto& row) {
					row.ico_id = n.ico_id;
					row.user = n.user;
				});
			}
			else
			{
				allowedicos.modify(iter, configs(_self, _self).get().application, [&](auto& row) {
					row.user = n.user;
				});
			}
		}	
	}

	void crgicoico::delauser(uint64_t ico_id)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		require_auth(configs(_self, _self).get().application);

		auto iter = allowedicos.find(ico_id);

		if (iter != allowedicos.end()) {
			allowedicos.erase(iter);
		}
	}

	void crgicoico::delall(name application)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		require_auth(configs(_self, _self).get().application);

		auto iter = allowedicos.begin();

		while (iter != allowedicos.end())
		{
			iter = allowedicos.erase(iter);
		}
	}

	void crgicoico::logdata(name application)
	{
		uint64_t oracle_id = 1;

		eosio_assert(configs(_self, _self).exists(), "Application account not configured ");
		require_auth(configs(_self, _self).get().application);

		auto icocon = icoconfigs(_self, _self).get();

		name crgoracle = icocon.crgoracle;
		uint64_t currentTime = current_time();

		logs l(_self, _self);

		oracles o(crgoracle, crgoracle);

		auto iterOracle = o.find(oracle_id);
		auto iterLog = l.find(currentTime);

		if (iterLog == l.end()) {
			l.emplace(configs(_self, _self).get().application, [&](auto& row) {
				row.logtime = currentTime;
				row.crgoracle = icocon.crgoracle;
				row.exchangerate = iterOracle->value;
				row.icostarttime = icocon.icostarttime;
				row.icoendtime = icocon.icoendtime;
			});
		}

	}

	void crgicoico::dellogdata(name application)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		require_auth(configs(_self, _self).get().application);

		logs l(_self, _self);

		auto iter = l.begin();

		while (iter != l.end())
		{
			iter = l.erase(iter);
		}
	}

	void crgicoico::addpurchase(name purchaser, asset crgbought, asset eospaid)
	{
		eosio_assert(configs(_self, _self).exists(), "Application account not configured.");
		require_auth(configs(_self, _self).get().application);

		purchases p(_self, _self);
		auto iter = p.find(purchaser);
		
		if (iter == p.end()) {
			p.emplace(configs(_self, _self).get().application, [&](auto& row) {
				row.purchaser = purchaser;
				row.crgbought = crgbought;
				row.eospaid = eospaid;
				
			});
		}
		else {
			p.modify(iter, configs(_self, _self).get().application, [&](auto& row) {
				row.crgbought = crgbought;
				row.eospaid = eospaid;

			});
		}
	}

	void crgicoico::apply(const name contract, const name act) {

		if (act == N(transfer)) {
			transferReceived(unpack_action_data<currency::transfer>(), contract);
			return;
		}

		auto &thiscontract = *this;

		switch (act) {
			EOSIO_API(crgicoico, (setapp)(setico)(addauser)(addausers)(delauser)(delall)(logdata)(dellogdata)(addpurchase)(returntokens))
		};
	}

	void crgicoico::returntokens(name purchaser) {
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		require_auth(configs(_self, _self).get().application);

		purchases p(_self, _self);
		auto iterPurchase = p.find(purchaser);

		eosio_assert(iterPurchase != p.end(), "no tokens to return");

		//sending back EOS TOKENS
		action(permission_level{ _self, N(active) }, N(eosio.token), N(transfer),
			make_tuple(_self, purchaser, iterPurchase->eospaid, string("Return of EOS tokens after unsuccessful ICO"))).send();

		//deleting the row in the table
		p.erase(iterPurchase);
	}

	void crgicoico::transferReceived(const currency::transfer &transfer, const name code) {
		eosio_assert(configs(_self, _self).exists(), "Application account not configured");
		
		if (transfer.from != N(crgappapp))
		{
			if (transfer.to != _self) {
				return;
			}
			eosio_assert(icoconfigs(_self, _self).exists(), "ICO not configured");

			//transfers of EOS - take part in ICO
			if (static_cast<uint32_t>(transfer.quantity.symbol == S(4, EOS)))
			{
				eosio_assert(static_cast<uint32_t>(code == N(eosio.token)), "needs to come from eosio.token");
				eosio_assert(static_cast<uint32_t>(transfer.memo.length() > 0), "needs a memo with the name");
				//eosio_assert(static_cast<uint32_t>(transfer.quantity.symbol == S(4, EOS)), "only EOS token allowed");
				eosio_assert(static_cast<uint32_t>(transfer.quantity.is_valid()), "invalid transfer");
				eosio_assert(static_cast<uint32_t>(transfer.quantity.amount >= 1000), "must be at least 0.1 EOS");

				uint64_t oracle_id = 1;
				auto icocon = icoconfigs(_self, _self).get();
				name crgoracle = icocon.crgoracle;
				uint32_t icostarttime = icocon.icostarttime;
				uint32_t icoendtime = icocon.icoendtime;
				uint32_t nowTime = now();

				//dates within ICO
				eosio_assert(icostarttime < nowTime, "ICO has not started");
				eosio_assert(nowTime  < icoendtime, "ICO has ended");

				oracles o(crgoracle, crgoracle);
				auto iterOracle = o.find(oracle_id);
				uint64_t USDrate = iterOracle->value;

				eosio_assert(30000 <= USDrate && USDrate <= 120000, "Check EOS/USD rate");

				uint64_t ico_id = std::stoull(transfer.memo);
				auto iterUser = allowedicos.find(ico_id);

				//the sending account must match the one registered in our app - memo should contain the ico_id that can be found in the account settings
				require_auth(iterUser->user);

				asset receivedEOS = transfer.quantity;
				uint64_t EOSamount = receivedEOS.amount;
				uint64_t priceInUSDcents = 15;
				uint64_t crgamount = (EOSamount * USDrate) / priceInUSDcents;

				asset crgtoSend = asset(crgamount, S(6, crg));
				//sending crg TOKENS
				action(permission_level{ _self, N(active) }, N(crgtokens), N(transfer),
					make_tuple(_self, transfer.from, crgtoSend, string("Thank you for taking part in our ICO!"))).send();

				//adding to purchase table
				purchases p(_self, _self);
				auto iterPurchase = p.find(transfer.from);

				if (iterPurchase == p.end())
				{
					p.emplace(configs(_self, _self).get().application, [&](auto& row) {
						row.purchaser = transfer.from;
						row.crgbought = crgtoSend;
						row.eospaid = receivedEOS;
					});
				}
				else
				{
					p.modify(iterPurchase, configs(_self, _self).get().application, [&](auto& row) {
						row.crgbought += crgtoSend;
						row.eospaid += receivedEOS;
					});
				}
			}
			//return of crg if ICO unsuccessful
			else
			{
				uint64_t ICO_MIN_AMOUNT = 800'000'000'0000;
				uint32_t TIME_TO_RETURN = 1535529600;

				eosio_assert(static_cast<uint32_t>(code == N(crgtokens)), "needs to come from crgtokens");
				eosio_assert(static_cast<uint32_t>(transfer.memo.length() > 0), "needs a memo with the name");
				eosio_assert(static_cast<uint32_t>(transfer.quantity.symbol == S(6, crg)), "only crg token allowed");
				eosio_assert(static_cast<uint32_t>(transfer.quantity.is_valid()), "invalid transfer");
				eosio_assert(static_cast<uint32_t>(transfer.quantity.amount > 0), "must be at positive");
				eosio_assert(now() < TIME_TO_RETURN, "the return can be made before August 29th 8 am - week after ICO");

				purchases p(_self, _self);
				auto iterPurchase = p.find(transfer.from);

				eosio_assert(iterPurchase != p.end(), "no tokens to return");
				eosio_assert(iterPurchase->crgbought == transfer.quantity, "the returned number of tokens is different than purchased");

				//checking if the number of crg tokens on crgicoico is bigger than 68 mln
				const asset crgIcoTokenBalance = crgtokens(N(crgtokens)).get_balance(_self, transfer.quantity.symbol.name());
				eosio_assert(crgIcoTokenBalance.amount - iterPurchase->crgbought.amount > ICO_MIN_AMOUNT, "ICO min amount has been collected");

				//sending back EOS TOKENS
				action(permission_level{ _self, N(active) }, N(eosio.token), N(transfer),
					make_tuple(_self, transfer.from, iterPurchase->eospaid, string("Return of EOS tokens"))).send();

				//deleting the row in the table
				p.erase(iterPurchase);
			}
		}
		
	}

} /// namespace crg

extern "C" {

	using namespace crg;
	using namespace eosio;

	void apply(uint64_t receiver, uint64_t code, uint64_t action) {
		auto self = receiver;
		crgicoico contract(self);
		contract.apply(code, action);
		eosio_exit(0);
	}
}