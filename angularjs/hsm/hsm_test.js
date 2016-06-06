'use strict';

describe("hsmService", function() {
  beforeEach(module('hsm'));

  var hsmService, $log;
  beforeEach(inject(function(_hsmService_, _$log_) {
    hsmService = _hsmService_;
    $log = _$log_;
  }));

  var testName = "";

  afterEach(function() {
    console.log("---", testName, "---");
    ["log", "debug", "info", "warn", "error"].forEach(function(l) {
      var text = $log[l].logs
      if (text.length) {
        console.log(l, text.join("\n"));
      }
    });
  });

  it("should build hierarchy", function() {
    testName = "should build hierarchy";
    var m = hsmService.newMachine("test", {});
    var s0 = m.newState("s0", {});
    var s1 = s0.newState("s1");
    var s11 = s1.newState("s11");
    var s12 = s1.newState("s12");
    var s2 = s0.newState("s2");
    var s21 = s2.newState("s21");
    var s211 = s2.newState("s211");
    var machine = m.finalize();

    var flatten = function(p) {
      var n = [p.name];
      p.children.forEach(function(c) {
        n = n.concat(flatten(c));
      });
      return n;
    };
    var list = flatten(machine);
    expect(list).toEqual([
      "test", "s0", "s1", "s11", "s12", "s2", "s21", "s211"
    ]);

  });
  it("should handle samek tests", function() {
    testName = "should evental";
    var Value = function() {
      this.value = 0;
    };
    Value.prototype.setValue = function() {
      this.value = 1;
    };
    Value.prototype.clearValue = function() {
      this.value = 0;
    };
    var plus = new Value();

    var Sequence = function() {
      var s = [];
      var add = function(name, reason) {
        var text = [name, reason].join("-");
        s.push(text);
      };
      this.enter = function(state, cause) {
        add(state.name, "ENTRY");
      };
      this.init = function(state, cause) {
        add(state.name, "INIT");
      };
      this.exit = function(state, cause) {
        add(state.name, "EXIT");
      };
      this.unhandled = function(cause) {
        add("EVT", cause);
      };
      this.result = function(num) {
        var res = s.slice();
        res.push(num);
        return res;
      };
    };
    var seq = new Sequence();

    var m = hsmService.newMachine("test", {
      onEnter: function(state, cause) {
        seq.enter(state);
      },
      onInit: function(state, cause) {
        seq.init(state);
      },
      onExit: function(state, cause) {
        seq.exit(state);
      },
      onEvent: function(state, cause) {
        if (!state) {
          seq.unhandled(cause);
        }
      },
    });
    var s0 = m.newState("s0", {
      onEvent: function(h) {
        switch (h.evt) {
          case "e":
            h.goto(s211.state());
            break;
          case "i":
            h.goto(s12.state());
            break;
        };
      }, //onEvent
    });
    var s1 = s0.newState("s1", {
      onEvent: function(h) {
        switch (h.evt) {
          case "a":
            h.goto(s1.state());
            break;
          case "b":
            h.goto(s11.state());
            break;
          case "c":
            h.goto(s2.state());
            break;
          case "d":
            h.goto(s0.state());
            break;
          case "f":
            h.goto(s211.state());
            break;
        };
      },
    });
    var s11 = s1.newState("s11", {
      onEvent: function(h) {
        switch (h.evt) {
          case "g":
            h.goto(s211.state());
            break;
          case "h":
            // so the original test by samek actually runs a self transition.
            if (plus.value) {
              h.goto(s11.state()).run(function() {
                plus.clearValue();
              });
            }
            break;
        };
      }, //onEvent
    });
    var s12 = s1.newState("s12");
    var s2 = s0.newState("s2", {
      onEvent: function(h) {
        switch (h.evt) {
          case "c":
            h.goto(s1.state());
            break;
          case "f":
            h.goto(s11.state());
            break;
        };
      }, // onEvent
    });
    var s21 = s2.newState("s21", {
      onEvent: function(h) {
        switch (h.evt) {
          case "b":
            h.goto(s211.state());
            break;
          case "h":
            if (!plus.value) {
              h.goto(s21.state()).run(function() {
                plus.setValue();
              });
            }
            break;
        };
      }, //onEvent
    });
    var s211 = s21.newState("s211", {
      onEvent: function(h) {
        switch (h.evt) {
          case "d":
            h.goto(s21.state());
            break;
          case "g":
            h.goto(s0.state());
            break;
        };
      }, //onEvent
    });

     // testing 1,2,3 
    var send = function(e) {
      seq = new Sequence();
      machine.emit(e);
      return seq.result(plus.value);
    };

    var machine = m.start(s0.state());
    // the expected sequence of initial events due to HsmStart() 
    var testStart = seq.result(0);
    expect(testStart).toEqual([
      "s0-ENTRY", "s0-INIT", "s1-ENTRY",
      "s1-INIT", "s11-ENTRY",
      0,
    ]);

    // s1 handles 'a' in a self-transition, init'ing back down to s11 
    var test1 = send("a");
    expect(test1).toEqual([
      "s11-EXIT", "s1-EXIT",
      "s1-ENTRY", "s1-INIT", "s11-ENTRY",
      0,
    ]);

    // s0 handles 'e' and directs entry down to s211 
    var test2 = send("e");
    expect(test2).toEqual([
      "s11-EXIT", "s1-EXIT",
      // "s0-EXIT", "s0-ENTRY", hsm-statechart, doesn't exit the source state, unless the source state target itself.
      "s2-ENTRY", "s21-ENTRY", "s211-ENTRY",
      0,
    ]);
    
    // s0 handles 'e' and directs entry down to s211 again 
    var test3 = send("e");
    expect(test3).toEqual([
      "s211-EXIT", "s21-EXIT", "s2-EXIT",
      /*"s0-EXIT","s0-ENTRY",*/
      "s2-ENTRY", "s21-ENTRY", "s211-ENTRY",
      0,
    ]);
    var test4 = send("a")
      // unhandled 
    expect(test4).toEqual([
      "EVT-a",
      0,
    ]);
    
    // s21 handles 'h', f==0, guard passes, sets foo, self-transition to s21, inits down to s211 
    var test5 = send("h");
    expect(test5).toEqual([
      "s211-EXIT", "s21-EXIT",
      "s21-ENTRY", "s21-INIT", "s211-ENTRY",
      1,
    ]);
    
    // s1 hears 'h', foo==1, guard filters, 'h' is unhandled 
    var test6 = send("h");
    expect(test6).toEqual([
      "EVT-h",
      1,
    ]);

    // s211 handles 'g', directs to 's0', inits down to s11. 
    var test7 = send("g");
    expect(test7).toEqual([
      "s211-EXIT", "s21-EXIT", "s2-EXIT",
      // "s0-EXIT","s0-ENTRY",
      // since we haven't exited s0, and it's not a leaf state, 
      // we get an init without an immediately preceeding enter 
      "s0-INIT", "s1-ENTRY", "s1-INIT", "s11-ENTRY",
      1,
    ]);
    
    // s11 handles 'h', clears foo 
    var test8 = send("h");
    expect(test8).toEqual([
      // "EVT-h", --> FIX? dont have an "absorb event" anymore.
      's11-EXIT', 's11-ENTRY',
      0,
    ]);
    
    // s0 handles 'i', directs down to 's12' 
    var test9 = send("i");
    expect(test9).toEqual([
      "s11-EXIT", "s1-EXIT",
      /*"s0-EXIT","s0-ENTRY",*/
      "s1-ENTRY", "s12-ENTRY",
      0,
    ]);

    // x isn't handled anywhere 
    var test10 = send("x");
    expect(test10).toEqual([
      "EVT-x",
      0,
    ]);
    return;
  });
});
