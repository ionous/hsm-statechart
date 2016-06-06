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

  var flatten = function(p) {
    var n = [p.name + (p.parallel ? "!" : "")];
    p.children.forEach(function(c) {
      n = n.concat(flatten(c));
    });
    return n;
  };

  var hierarchyTest = "should build hierarchy";
  it(hierarchyTest, function() {
    testName = hierarchyTest;
    var m = hsmService.newMachine("test", {});
    var s0 = m.newState("s0", {});
    var s1 = s0.newState("s1");
    var s11 = s1.newState("s11");
    var s12 = s1.newState("s12");
    var s2 = s0.newState("s2");
    var s21 = s2.newState("s21");
    var s211 = s21.newState("s211");
    var machine = m.finalize();

    var list = flatten(machine);
    expect(list).toEqual([
      "test", "s0", "s1", "s11", "s12", "s2", "s21", "s211"
    ]);
  });

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

  var samekTest = "should handle samek tests";
  it(samekTest, function() {
    testName = samekTest;

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
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "e":
            then.goto(s211.state());
            break;
          case "i":
            then.goto(s12.state());
            break;
        };
      }, //onEvent
    });
    var s1 = s0.newState("s1", {
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "a":
            then.goto(s1.state());
            break;
          case "b":
            then.goto(s11.state());
            break;
          case "c":
            then.goto(s2.state());
            break;
          case "d":
            then.goto(s0.state());
            break;
          case "f":
            then.goto(s211.state());
            break;
        };
      },
    });
    var s11 = s1.newState("s11", {
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "g":
            then.goto(s211.state());
            break;
          case "h":
            // so the original test by samek actually runs a self transition.
            if (plus.value) {
              then.goto(s11.state()).run(function() {
                plus.clearValue();
              });
            }
            break;
        };
      }, //onEvent
    });
    var s12 = s1.newState("s12");
    var s2 = s0.newState("s2", {
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "c":
            then.goto(s1.state());
            break;
          case "f":
            then.goto(s11.state());
            break;
        };
      }, // onEvent
    });
    var s21 = s2.newState("s21", {
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "b":
            then.goto(s211.state());
            break;
          case "h":
            if (!plus.value) {
              then.goto(s21.state()).run(function() {
                plus.setValue();
              });
            }
            break;
        };
      }, //onEvent
    });
    var s211 = s21.newState("s211", {
      onEvent: function(state, cause, then) {
        switch (cause) {
          case "d":
            then.goto(s21.state());
            break;
          case "g":
            then.goto(s0.state());
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

  var parallelTreeTest = "should build parallel trees";
  it(parallelTreeTest, function() {
    testName = parallelTreeTest;
    // [h-[A|2,1(i---B)|3,1(l-y)
    //      |2,2(j-w)  |3,2(m-z)    
    //      |2,3(k-x)   
    var m = hsmService.newMachine("test", {});
    var sh = m.newState("h");
    var sA = sh.newState("A", {
      parallel: true
    });
    var si = sA.newState("i");
    var sB = si.newState("B", {
      parallel: true
    });
    //
    var sj = sA.newState("j");
    var sk = sA.newState("k");
    //
    var sl = sB.newState("l");
    var sm = sB.newState("m");
    //
    var sw = sj.newState("w");
    var sx = sk.newState("x");
    var sy = sl.newState("y");
    var sz = sm.newState("z");
    //
    var machine = m.finalize();
    var list = flatten(machine);
    expect(list).toEqual([
      "test", "h", "A!", "i", "B!", "l", "y", "m", "z", "j", "w", "k", "x"
    ]);
  });
});
