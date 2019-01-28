const expect = require('chai').expect;

describe('Token contract', function() {
  describe('Create', function() {
    it('parslseed123@active should be able to create tokens', async function() {
      try {
        const exist = await global.EOSContract.getTableScope(
          'token', 
          'stat',
          'SEED'
        );
        expect(exist.rows[0].max_supply).to.equal("10000000000.0000 SEED");
      } catch (err) {
        console.log(err);
        const r = await global.EOSContract.sendAction(
          'token', 
          'create', 
          {issuer: "parslseed123", maximum_supply: "10000000000.0000 SEED"}, 
          "parslseed123@active", 
          global.EOSContract.getPrivateKey("parslseed123@active")
        );
        expect(r.processed.action_traces[0]).to.have.property("trx_id");
      }
    });
    it('parslseed123 has created tokens', async function() {
      const r = await global.EOSContract.getTableScope(
        'token', 
        'stat',
        'SEED'
      );
      expect(r.rows[0].max_supply).to.equal("10000000000.0000 SEED");
    });
  });
  describe('Issue', function() {
    it('parslseed123@active should be able to issue tokens to self', async function() {
        const r = await global.EOSContract.sendAction(
        'token', 
        'issue', 
        {to: "parslseed123", quantity: "10000.0000 SEED", memo: "User1"}, 
        "parslseed123@active", 
        global.EOSContract.getPrivateKey("parslseed123@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
    it('parslseed123@active should be able to issue tokens to others', async function() {
      const r = await global.EOSContract.sendAction(
        'token', 
        'issue', 
        {to: "user2", quantity: "10000.0000 SEED", memo: "User2"}, 
        "parslseed123@active", 
        global.EOSContract.getPrivateKey("parslseed123@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
  });
  describe('Transfer', function() {
    it('parslseed123@active should be able to transfer tokens', async function() {
      const r = await global.EOSContract.sendAction(
        'token', 
        'transfer', 
        {from:"parslseed123", to: "user1", quantity: "2000.0000 SEED", memo: "User1"}, 
        "parslseed123@active", 
        global.EOSContract.getPrivateKey("parslseed123@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
    it('user2@active should not be able to transfer tokens they don\'t have', async function() {
      try {
        const r = await global.EOSContract.sendAction(
          'token', 
          'transfer', 
          {from:"user2", to: "user1", quantity: "1000000000.0000 SEED", memo: "User1"}, 
          "user2@active", 
          global.EOSContract.getPrivateKey("user2@active")
        );
        throw new Error(r);
      } catch (err) {
        expect(err.json.error.name).to.equal("eosio_assert_message_exception");
      }
    });
  });
  describe('Staking', function() {
    let tokens = {
      total: 0,
      stake: 0,
      refund: 0,
    };
    it('user1@active should be able to stake tokens', async function() {
      //start before
        const r1 = await global.EOSContract.getTableScope(
          'token', 
          'stake',
          'user1'
        );

        const r2 = await global.EOSContract.getTableScope(
          'token', 
          'refund',
          'user1'
        );

        tokens.stake = Number(r1.rows[0] ? r1.rows[0].quantity.split(" ")[0] : 0);
        tokens.refund = Number(r2.rows[0] ? r2.rows[0].quantity.split(" ")[0] : 0);
      //end before

      const r = await global.EOSContract.sendAction(
        'token', 
        'stake', 
        {owner:"user1", quantity: "2000.0000 SEED"}, 
        "user1@active", 
        global.EOSContract.getPrivateKey("user1@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
    it('user1@active should be able to unstake tokens', async function() {
      const r = await global.EOSContract.sendAction(
        'token', 
        'unstake', 
        {owner:"user1", quantity: "1000.0000 SEED"}, 
        "user1@active", 
        global.EOSContract.getPrivateKey("user1@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");      
    });
    it('user1@active should not be able to refund tokens', async function() {
      try {
        const r = await global.EOSContract.sendAction(
          'token', 
          'refund', 
          {owner:"user1",sym:"4,SEED"}, 
          "user1@active", 
          global.EOSContract.getPrivateKey("user1@active")
        );
        throw new Error(r);
      } catch (err) {
        expect(err.json.error.name).to.equal("eosio_assert_message_exception");
      }
    });
    it('user1@active should have staked and unstaked tables', async function() {
      const r1 = await global.EOSContract.getTableScope(
        'token', 
        'stake',
        'user1'
      );
      expect(r1.rows[0].quantity).to.equal(`${Number(tokens.stake + 1000).toFixed(4)} SEED`);

      const r2 = await global.EOSContract.getTableScope(
        'token', 
        'refund',
        'user1'
      );
      expect(r2.rows[0].quantity).to.equal(`${Number(tokens.refund + 1000).toFixed(4)} SEED`);
    });
  });  
  describe('Reward Drop', function() {
    let tokens = {
      total: 0,
      stake: 0,
      refund: 0,
    };
    it('parslseed123@active should be able to issue tokens to self', async function() {
      //start before
        const r1 = await global.EOSContract.getTableScope(
          'token', 
          'stake',
          'user3'
        );

        tokens.stake = Number(r1.rows[0] ?  r1.rows[0].quantity.split(" ")[0] : 0);
      //end before

      const r = await global.EOSContract.sendAction(
        'token', 
        'issue', 
        {to: "parslseed123", quantity: "10000.0000 SEED", memo: "User3"}, 
        "parslseed123@active", 
        global.EOSContract.getPrivateKey("parslseed123@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
    it('parslseed123@active should be able to rewarddrop tokens to user3', async function() {
      const r = await global.EOSContract.sendAction(
        'token', 
        'rewarddrop', 
        {from:"parslseed123", to: "user3", quantity: "10000.0000 SEED", memo: "User3"}, 
        "parslseed123@active", 
        global.EOSContract.getPrivateKey("parslseed123@active")
      );
      expect(r.processed.action_traces[0]).to.have.property("trx_id");
    });
    it('user3@active should have fully staked balance', async function() {
      const r1 = await global.EOSContract.getTableScope(
        'token', 
        'stake',
        'user3'
      );
      expect(r1.rows[0].quantity).to.equal(`${Number(tokens.stake + 10000).toFixed(4)} SEED`);
    });
  });
  
});
