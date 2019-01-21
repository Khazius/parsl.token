describe('Token contract', function() {
  describe('Create', function() {
    it('parslseed123@active should be able to create tokens', function(done) {
      global.EOSContract.sendAction(
      'token', 
      'create', 
      {issuer: "parslseed123", maximum_supply: "10000000000.0000 SEED"}, 
      "parslseed123@active", 
      global.EOSContract.getPrivateKey("parslseed123@active")
      ).then(r => {
        if(r.processed.action_traces[0].trx_id){
          done()
        }else{
          global.EOSContract.logTX(r)
          throw new Error("Fail")
        }
      }).catch(err => done(err))
    });
    it('parslseed123 has created tokens', function(done) {
      global.EOSContract.getTableScope(
      'token', 
      'stat',
      'SEED'
      ).then(r => {
        //console.log(r.rows[0].max_supply == "10000000000.0000 SEED");
        if(r.rows[0].max_supply == "10000000000.0000 SEED"){
          done()
        }else{
          global.EOSContract.logTX(r)
          throw new Error("Fail")
        }
      }).catch(err => done(err))
    });
    it('parslseed123@active should be able to issue tokens to self', function(done) {
      global.EOSContract.sendAction(
      'token', 
      'issue', 
      {to: "parslseed123", quantity: "10000.0000 SEED", memo: "User1"}, 
      "parslseed123@active", 
      global.EOSContract.getPrivateKey("parslseed123@active")
      ).then(r => {
        if(r.processed.action_traces[0].trx_id){
          done()
        }else{
          global.EOSContract.logTX(r)
          throw new Error("Fail")
        }
      }).catch(err => done(err))
    });
    it('parslseed123@active should be able to transfer tokens', function(done) {
      global.EOSContract.sendAction(
      'token', 
      'transfer', 
      {from:"parslseed123", to: "user1", quantity: "2000.0000 SEED", memo: "User1"}, 
      "parslseed123@active", 
      global.EOSContract.getPrivateKey("parslseed123@active")
      ).then(r => {
        if(r.processed.action_traces[0].trx_id){
          done()
        }else{
          global.EOSContract.logTX(r)
          throw new Error("Fail")
        }
      }).catch(err => done(err))
    });
  });
});
