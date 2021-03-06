/*jslint sloppy: true, white: true, indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var errors = arangodb.errors;
var db = arangodb.db;

var replication = require("org/arangodb/replication");
var console = require("console");
var sleep = require("internal").wait;
var masterEndpoint = arango.getEndpoint();
var slaveEndpoint = masterEndpoint.replace(/:3(\d+)$/, ':4$1');


// -----------------------------------------------------------------------------
// --SECTION--                                                 replication tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  var cn  = "UnitTestsReplication";
  var cn2 = "UnitTestsReplication2";

  // these must match the values in the Makefile!
  var replicatorUser = "replicator-user";
  var replicatorPassword = "replicator-password";

  var connectToMaster = function () {
    arango.reconnect(masterEndpoint, db._name(), replicatorUser, replicatorPassword);
  };
  
  var connectToSlave = function () {
    arango.reconnect(slaveEndpoint, db._name(), "root", "");
  };

  var collectionChecksum = function (name) {
    var c = db._collection(name).checksum(true, true);
    return c.checksum + "-" + c.revision;
  };
  
  var collectionCount = function (name) {
    return db._collection(name).count();
  };


  var compare = function (masterFunc, slaveFunc, applierConfiguration) {
    var state = { };

    masterFunc(state);  

    var masterState = replication.logger.state();
    assertTrue(masterState.state.running);

    replication.logger.stop();
    masterState = replication.logger.state();
    assertFalse(masterState.running);

    connectToSlave();
    replication.applier.stop();

    var syncResult = replication.sync({ 
      endpoint: masterEndpoint, 
      username: replicatorUser,
      password: replicatorPassword,
      verbose: true 
    });

    assertTrue(syncResult.hasOwnProperty('lastLogTick'));
  
    if (typeof applierConfiguration === 'object') {
      console.log("using special applier configuration: " + JSON.stringify(applierConfiguration));
    }

    applierConfiguration = applierConfiguration || { };
    applierConfiguration.endpoint = masterEndpoint;
    applierConfiguration.username = replicatorUser;
    applierConfiguration.password = replicatorPassword;

    if (! applierConfiguration.hasOwnProperty('chunkSize')) {
      applierConfiguration.chunkSize = 16384;
    }

    replication.applier.properties(applierConfiguration);
    replication.applier.start(syncResult.lastLogTick);

    // console.log("waiting for slave to catch up");

    while (1) {
      var slaveState = replication.applier.state();

      if (! slaveState.state.running || slaveState.state.lastError.errorNum > 0) {
        break;
      }

      if (slaveState.state.lastAppliedContinuousTick === syncResult.lastLogTick || 
          slaveState.state.lastProcessedContinuousTick === syncResult.lastLogTick ||
          slaveState.state.lastAvailableContinuousTick === syncResult.lastLogTick) { 
        break;
      }

      sleep(1);
    }

    slaveFunc(state);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      connectToMaster();
      replication.logger.stop();

      db._drop(cn);
      db._drop(cn2);
    
      replication.logger.properties({ maxEvents: 1048576 });
      replication.logger.start();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      connectToMaster();
      replication.logger.stop();
      replication.logger.properties({ maxEvents: 1048576 });
      
      db._drop(cn);
      db._drop(cn2);

      connectToSlave();
      replication.applier.stop();
      db._drop(cn);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid credentials
////////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials1 : function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: replicatorUser,
        password: replicatorPassword + "xx" // invalid
      };

      try {
        replication.applier.properties(configuration);
      }
      catch (err) {
        require("internal").print(err);
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid credentials
////////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials2 : function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: replicatorUser + "xx", // invalid
        password: replicatorPassword
      };

      try {
        replication.applier.properties(configuration);
      }
      catch (err) {
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid credentials
////////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials3 : function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: "root",
        password: "abc"
      };

      try {
        replication.applier.properties(configuration);
      }
      catch (err) {
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test exceeding cap
////////////////////////////////////////////////////////////////////////////////

    testCapped : function () {
      connectToMaster();
      // set a low cap which we'll exceed easily
      replication.logger.properties({ maxEvents: 4096 });

      compare(
        function (state) {
          var c = db._create(cn), i;
 
          for (i = 0; i < 50000; ++i) {
            c.save({ "value" : i });
          }
      
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test not exceeding cap
////////////////////////////////////////////////////////////////////////////////

    testUncapped : function () {
      connectToMaster();
      // set a low cap which we'll exceed easily
      replication.logger.properties({ maxEvents: 0, maxEventsSize: 0 });

      compare(
        function (state) {
          var c = db._create(cn), i;
 
          for (i = 0; i < 50000; ++i) {
            c.save({ "value" : i });
          }
      
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test documents
////////////////////////////////////////////////////////////////////////////////

    testDocuments1 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
 
          for (i = 0; i < 1000; ++i) {
            c.save({ "value" : i, "foo" : true, "bar" : [ i , false ], "value2" : null, "mydata" : { "test" : [ "abc", "def" ] } });
          }
      
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test documents
////////////////////////////////////////////////////////////////////////////////

    testDocuments2 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 1000; ++i) {
            c.save({ "abc" : true, "_key" : "test" + i });
            if (i % 3 == 0) {
              c.remove(c.last());
            }
            else if (i % 5 == 0) {
              c.update("test" + i, { "def" : "hifh" });
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(666, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test documents
////////////////////////////////////////////////////////////////////////////////

    testDocuments3 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 50000; ++i) {
            c.save({ "_key" : "test" + i, "foo" : "bar", "baz" : "bat" });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test documents
////////////////////////////////////////////////////////////////////////////////

    testDocuments4 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;

          for (i = 0; i < 50000; ++i) {
            c.save({ "_key" : "test" + i, "foo" : "bar", "baz" : "bat" });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        },
        {
          chunkSize: 512
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test edges
////////////////////////////////////////////////////////////////////////////////

    testEdges : function () {
      compare(
        function (state) {
          var v = db._create(cn), i;
          var e = db._createEdgeCollection(cn2);

          for (i = 0; i < 1000; ++i) {
            v.save({ "_key" : "test" + i });
          }

          for (i = 0; i < 5000; i += 10) {
            e.save(cn + "/test" + i, cn + "/test" + i, { "foo" : "bar", "value" : i });
          }

          state.checksum1 = collectionChecksum(cn);
          state.count1 = collectionCount(cn);
          assertEqual(1000, state.count1);

          state.checksum2 = collectionChecksum(cn2);
          state.count2 = collectionCount(cn2);
          assertEqual(500, state.count2);
        },
        function (state) {
          assertEqual(state.checksum1, collectionChecksum(cn));
          assertEqual(state.count1, collectionCount(cn));
          
          assertEqual(state.checksum2, collectionChecksum(cn2));
          assertEqual(state.count2, collectionCount(cn2));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transactions
////////////////////////////////////////////////////////////////////////////////

    testTransaction1 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;

          try {
            db._executeTransaction({
              collections: { 
                write: cn
              },
              action: function () {
                for (i = 0; i < 1000; ++i) {
                  c.save({ "_key" : "test" + i });
                }

                throw "rollback!";
              } 
            });
            fail();
          }
          catch (err) {
          }
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transactions
////////////////////////////////////////////////////////////////////////////////

    testTransaction2 : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
              
          for (i = 0; i < 1000; ++i) {
            c.save({ "_key" : "test" + i });
          }

          try {
            db._executeTransaction({
              collections: { 
                write: cn
              },
              action: function () {
                for (i = 0; i < 1000; ++i) {
                  c.remove("test" + i);
                }
              } 
            });
            fail();
          }
          catch (err) {

          }
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transactions
////////////////////////////////////////////////////////////////////////////////

    testTransaction3 : function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: { 
              write: cn
            },
            action: function (params) {
              var c = require("internal").db._collection(params.cn), i;

              for (i = 0; i < 1000; ++i) {
                c.save({ "_key" : "test" + i });
              }
            },
            params: { "cn": cn }, 
          });
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test transactions
////////////////////////////////////////////////////////////////////////////////

    testTransaction4 : function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: { 
              write: cn
            },
            action: function (params) {
              var c = require("internal").db._collection(params.cn), i;

              for (i = 0; i < 1000; ++i) {
                c.save({ "_key" : "test" + i });
              }
              
              for (i = 0; i < 1000; ++i) {
                c.update("test" + i, { "foo" : "bar" + i });
              }
              
              for (i = 0; i < 1000; ++i) {
                c.update("test" + i, { "foo" : "baz" + i });
              }

              for (i = 0; i < 1000; i += 10) {
                c.remove("test" + i);
              }
            },
            params: { "cn": cn }, 
          });
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(900, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test big transaction
////////////////////////////////////////////////////////////////////////////////

    testTransactionBig : function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: { 
              write: cn
            },
            action: function (params) {
              var c = require("internal").db._collection(params.cn), i;

              for (i = 0; i < 50000; ++i) {
                c.save({ "_key" : "test" + i, value : i });
                c.update("test" + i, { value : i + 1 });

                if (i % 5 == 0) {
                  c.remove("test" + i);
                }
              }
            },
            params: { "cn" : cn }, 
          });
          
          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(40000, state.count);
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
        },
        {
          chunkSize: 2048
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection
////////////////////////////////////////////////////////////////////////////////

    testCreateCollection1 : function () {
      compare(
        function (state) {
          var c = db._create(cn, { 
            isVolatile : true, 
            waitForSync : false, 
            doCompact : false, 
            journalSize : 1048576
          });
          
          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(state.cid, db._collection(cn)._id);
          assertEqual(cn, db._collection(cn).name());
          assertTrue(properties.isVolatile);
          assertFalse(properties.waitForSync);
          assertFalse(properties.deleted);
          assertFalse(properties.doCompact);
          assertEqual(1048576, properties.journalSize);
          assertTrue(properties.keyOptions.allowUserKeys);
          assertEqual("traditional", properties.keyOptions.type);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create collection
////////////////////////////////////////////////////////////////////////////////

    testCreateCollection2 : function () {
      compare(
        function (state) {
          var c = db._create(cn, { 
            keyOptions : { 
              type : "autoincrement", 
              allowUserKeys : false 
            }, 
            isVolatile : false, 
            waitForSync : true, 
            doCompact : true, 
            journalSize : 2097152 
          });
          
          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(state.cid, db._collection(cn)._id);
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.isVolatile);
          assertTrue(properties.waitForSync);
          assertFalse(properties.deleted);
          assertTrue(properties.doCompact);
          assertEqual(2097152, properties.journalSize);
          assertFalse(properties.keyOptions.allowUserKeys);
          assertEqual("autoincrement", properties.keyOptions.type);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test drop collection
////////////////////////////////////////////////////////////////////////////////

    testDropCollection : function () {
      compare(
        function (state) {
          var c = db._create(cn);
          c.drop();
        },
        function (state) {
          assertNull(db._collection(cn));
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cap constraint
////////////////////////////////////////////////////////////////////////////////

    testCapConstraint : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
          c.ensureCapConstraint(128);
              
          for (i = 0; i < 1000; ++i) {
            c.save({ "_key" : "test" + i });
          }
          state.last = c.last(3);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(128, state.count);
          assertEqual("test999", state.last[0]._key);
          assertEqual("test998", state.last[1]._key);
          assertEqual("test997", state.last[2]._key);
          
          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.last, db._collection(cn).last(3));
          
          assertEqual(state.idx.id, db._collection(cn).getIndexes()[1].id);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hash index
////////////////////////////////////////////////////////////////////////////////

    testUniqueConstraint : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
          c.ensureHashIndex("a", "b");
              
          for (i = 0; i < 1000; ++i) {
            c.save({ "_key" : "test" + i, "a" : parseInt(i / 2), "b" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual("hash", state.idx.type);
          assertFalse(state.idx.unique);
          assertEqual([ "a" ], state.idx.fields);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique constraint
////////////////////////////////////////////////////////////////////////////////

    testUniqueConstraint : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
          c.ensureUniqueConstraint("a");
              
          for (i = 0; i < 1000; ++i) {
            try {
              c.save({ "_key" : "test" + i, "a" : parseInt(i / 2) });
            }
            catch (err) {
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(500, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual("hash", state.idx.type);
          assertTrue(state.idx.unique);
          assertEqual([ "a" ], state.idx.fields);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test skiplist
////////////////////////////////////////////////////////////////////////////////

    testSkiplist : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
          c.ensureSkiplist("a", "b");
              
          for (i = 0; i < 1000; ++i) {
            c.save({ "_key" : "test" + i, "a" : parseInt(i / 2), "b" : i });
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual("skiplist", state.idx.type);
          assertFalse(state.idx.unique);
          assertEqual([ "a", "b" ], state.idx.fields);
        }
      );
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique skiplist
////////////////////////////////////////////////////////////////////////////////

    testUniqueSkiplist : function () {
      compare(
        function (state) {
          var c = db._create(cn), i;
          c.ensureUniqueSkiplist("a");
              
          for (i = 0; i < 1000; ++i) {
            try {
              c.save({ "_key" : "test" + i, "a" : parseInt(i / 2) });
            }
            catch (err) {
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(500, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.checksum, collectionChecksum(cn));
          assertEqual(state.count, collectionCount(cn));
          
          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual("skiplist", state.idx.type);
          assertTrue(state.idx.unique);
          assertEqual([ "a" ], state.idx.fields);
        }
      );
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
