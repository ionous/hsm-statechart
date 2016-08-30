'use strict';

describe("hsmService", function() {
  beforeEach(module('hsm'));

  if (jasmine.version) { //the case for version 2.0.0
    console.log('jasmine-version:' + jasmine.version);
  } else { //the case for version 1.3
    console.log('jasmine-version:' + jasmine.getEnv().versionString());
  }

  var hsmService, $log;
  beforeEach(inject(function(_hsmService_, _$log_) {
    hsmService = _hsmService_;
    $log = _$log_;

  }));

  afterEach(function() {
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

  it("should build a single region", function() {
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

  it("should handle samek tests", function() {
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
    var sendone = function(e) {
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
    var test1 = sendone("a");
    expect(test1).toEqual([
      "s11-EXIT", "s1-EXIT",
      "s1-ENTRY", "s1-INIT", "s11-ENTRY",
      0,
    ]);

    // s0 handles 'e' and directs entry down to s211 
    var test2 = sendone("e");
    expect(test2).toEqual([
      "s11-EXIT", "s1-EXIT",
      // "s0-EXIT", "s0-ENTRY", hsm-statechart, doesn't exit the source state, unless the source state target itself.
      "s2-ENTRY", "s21-ENTRY", "s211-ENTRY",
      0,
    ]);

    // s0 handles 'e' and directs entry down to s211 again 
    var test3 = sendone("e");
    expect(test3).toEqual([
      "s211-EXIT", "s21-EXIT", "s2-EXIT",
      /*"s0-EXIT","s0-ENTRY",*/
      "s2-ENTRY", "s21-ENTRY", "s211-ENTRY",
      0,
    ]);
    var test4 = sendone("a")
      // unhandled 
    expect(test4).toEqual([
      "EVT-a",
      0,
    ]);

    // s21 handles 'h', f==0, guard passes, sets foo, self-transition to s21, inits down to s211 
    var test5 = sendone("h");
    expect(test5).toEqual([
      "s211-EXIT", "s21-EXIT",
      "s21-ENTRY", "s21-INIT", "s211-ENTRY",
      1,
    ]);

    // s1 hears 'h', foo==1, guard filters, 'h' is unhandled 
    var test6 = sendone("h");
    expect(test6).toEqual([
      "EVT-h",
      1,
    ]);

    // s211 handles 'g', directs to 's0', inits down to s11. 
    var test7 = sendone("g");
    expect(test7).toEqual([
      "s211-EXIT", "s21-EXIT", "s2-EXIT",
      // "s0-EXIT","s0-ENTRY",
      // since we haven't exited s0, and it's not a leaf state, 
      // we get an init without an immediately preceeding enter 
      "s0-INIT", "s1-ENTRY", "s1-INIT", "s11-ENTRY",
      1,
    ]);

    // s11 handles 'h', clears foo 
    var test8 = sendone("h");
    expect(test8).toEqual([
      // "EVT-h", --> FIX? dont have an "absorb event" anymore.
      's11-EXIT', 's11-ENTRY',
      0,
    ]);

    // s0 handles 'i', directs down to 's12' 
    var test9 = sendone("i");
    expect(test9).toEqual([
      "s11-EXIT", "s1-EXIT",
      /*"s0-EXIT","s0-ENTRY",*/
      "s1-ENTRY", "s12-ENTRY",
      0,
    ]);

    // x isn't handled anywhere 
    var test10 = sendone("x");
    expect(test10).toEqual([
      "EVT-x",
      0,
    ]);
    return;
  });

  // (0,h+--1,A)...| A/1 (2,j---3,w)
  //               | A/2 (2,i+--3,x) 
  //               |     (2,i+--3,B)...| B/1 (4,l--5,y)
  //               |                   |     (4,l--5,t)
  //               |                   | B/2 (4,m--5,z)
  //               | A/3 (2,k)
  // 0,(h+--1,q)
  var makeBase = function() {
    var Counter = function() {
      var counts = {
        enter: {},
        exit: {},
        init: {},
      };
      var get = this.get = function(state, kind) {
        var counter = counts[kind];
        return counter[state] || 0;
      };
      this.add = function(state, kind, v) {
        var counter = counts[kind];
        return counter[state] = get(state, kind) + v;
      };
    };
    var counter = new Counter();
    var active = {}; // or enter > exit
    var states = {};
    var recentActions = []; // reset on send

    var handle = function(then, srcname, tgtname) {
      var action = function() {
        recentActions.push(srcname + tgtname);
      };
      var tgt, ext;
      if (tgtname == "-") {
        tgtname = srcname;
        ext = false; // explictly an internal transition
      }
      var tgt = states[tgtname];
      if (!tgt) {
        throw new Error("invalid state " + tgt);
      } else {
        then.goto(tgt, ext).run(action);
      }
    };
    var common = {
      onEnter: function(state) {
        active[state.name] = true;
        counter.add(state.name, "enter", 1);
      },
      onInit: function(state) {
        counter.add(state.name, "init", 1);
      },
      // a common event handler allowing any state to be targeted with any destination
      onEvent: function(state, cause, then) {
        for (var i = 0; i < cause.length; i += 2) {
          var srcname = cause[i];
          if (srcname == state.name) {
            var tgtname = cause[i + 1];
            handle(then, srcname, tgtname);
            break;
          }
        }
      },
      onExit: function(state) {
        delete active[state.name];
        counter.add(state.name, "exit", 1);
      },
    };
    var parallel = {
      onEnter: common.onEnter,
      onExit: common.onExit,
      onEvent: common.onEvent,
      parallel: true
    };
    // FIX: state by name should really be part of the ... builder or state system.
    var add = function(s) {
      var state = s.state();
      states[state.name] = state;
      return s;
    };
    var isActive = function(val, args) {
      var fails = [];
      for (var i = 0; i < args.length; i++) {
        var n = args[i];
        var is = active[n];
        if (is !== val) {
          fails.push(n);
        }
      }
      return fails.length ? fails : true;
    };
    var b = {
      addCommon: function(c, n) {
        return add(c.newState(n, common));
      },
      addParallel: function(c, n) {
        return add(c.newState(n, parallel));
      },
      states: states,
      active: active,
      actions: recentActions,
      counter: counter,
      enters: function(s) {
        return enters[s] || 0;
      },
      start: function(m, first) {
        var machine = m.start(states[first]);
        b.machine = machine;
        // called "send" instead of "emit" to help with 
        // search/replace of jasmine "it" -> "xit".
        b.send = function(_arguments) {
          var es = Array.prototype.slice.call(arguments).join("");
          //console.log("send", es);
          b.actions.length = 0;
          machine.emit(es);
        };
        return b;
      },
    };
    return b;
  };

  var makeParallel = function(first) {
    var b = makeBase();
    //
    var m = hsmService.newMachine("test", {});
    var sh = b.addCommon(m, "h");
    var sA = b.addParallel(sh, "A");
    var sq = b.addCommon(sh, "q");
    // j (now) comes before i
    var sj = b.addCommon(sA, "j");
    // i switches between parallel and not parallel
    var si = b.addCommon(sA, "i");
    var sx = b.addCommon(si, "x");
    var sB = b.addParallel(si, "B");
    var sk = b.addCommon(sA, "k");
    var sl = b.addCommon(sB, "l");
    var sm = b.addCommon(sB, "m");
    var sw = b.addCommon(sj, "w");
    var sy = b.addCommon(sl, "y");
    var st = b.addCommon(sl, "t");
    var sz = b.addCommon(sm, "z");
    //
    return b.start(m, first);
  };

  it("should build parallel trees", function() {
    var p = makeParallel();
    var list = flatten(p.machine);
    expect(list).toEqual([
      "test", "h", "A!", "j", "w", "i", "x", "B!", "l", "y", "t", "m", "z", "k", "q",
    ]);
  });

  beforeEach(function() {
    var compare = function(r, n, wantList, gotList, sort) {
      var src = Array.prototype.slice.call(wantList);
      if (sort) {
        src = src.sort();
        gotList = gotList.slice().sort();
      }
      var expects = src.join(",");
      var actually = gotList.join(",");
      var ok = expects == actually;
      if (!ok) {
        r.message = function() {
          var m = "expected " + expects + " " + n + ", but actually " + actually;
          return [m, m];
        }
      }
      return ok;
    };
    this.addMatchers({
      actions: function(_arguments) {
        var got = this.actual.actions;
        return compare(this, "actions", arguments, got);
      },
      active: function(_arguments) {
        var got = Object.keys(this.actual.active);
        return compare(this, "active", arguments, got, true);
      },
      dump: function(_arguments) {
        var gotList = Object.keys(this.actual.active);
        var src = Array.prototype.slice.call(arguments);

        src = src.sort();
        gotList = gotList.slice().sort();

        var expects = src.join(",");
        var actually = gotList.join(",");
        console.log(expects, actually);
        return true;
      },
      enterExit: function(expected) {
        var p = this.actual;
        var fails = [];
        var check = function(state, kind, wanted) {
          var expects = wanted[kind];
          if (!angular.isUndefined(expects)) {
            var actually = p.counter.get(state, kind);
            if (expects != actually) {
              fails.push("'" + state + "' expected " + expects + " " + kind + "s, but actually had " + actually);
            }
          }
        };

        for (var state in expected) {
          var wants = {};
          var expects = expected[state];
          for (var x in expects) {
            var c = expects[x];
            switch (x) {
              case "enter":
              case "exit":
              case "init":
                wants[x] = c;
                break;
              default:
                throw new Error("invalid expectation " + x);
            }
          }
          check(state, "enter", wants);
          check(state, "init", wants);
          check(state, "exit", wants);
        };
        this.message = function() {
          var m = fails.join(", and\n") + ".";
          return [m, m];
        }
        return !fails.length;
      }
    });
  });

  // feels like there are a huge number of test combinations, here's a not done few:
  // transitions:
  // * switching to and from a parallel, parallel of parallels
  // construction:
  // * a state marked parallel with no contents
  // * a parallel directly as a child of another parallel
  it("should be default active", function() {
    var p = makeParallel();
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).actions("");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 0,
        init: 0,
      },
      "i": {
        enter: 1,
        exit: 0,
        init: 1,
      },
    });
  });
  it("should start into a leaf state", function() {
    var p = makeParallel("q");
    expect(p).active("h", "q");
    expect(p).actions("");
    expect(p).enterExit({
      "h": {
        enter: 1,
        exit: 0,
        init: 0,
      },
      "q": {
        enter: 1,
        exit: 0,
        init: 0,
      },
    });
  });
  it("should start into a parallel region", function() {
    var p = makeParallel("j");
    expect(p).active("h", "A", "j", "w", "k", "i", "x");
    expect(p).enterExit({
      "j": {
        init: 1,
      },
      "w": {
        init: 0,
      },
    });
  });
  it("should start into a parallel region of regions", function() {
    var p = makeParallel("y");
    expect(p).active("h", "A", "i", "B", "l", "y", "m", "z", "j", "w", "k");
  });
  it("should handle the self transition of a leaf region", function() {
    var p = makeParallel();
    p.send("jj");
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).enterExit({
      "A": {
        init: 0, // all children are automatically activated, so zero inits.
        exit: 0, // not leaving A
        enter: 1,
      },
      "j": {
        exit: 1, // self transitions default to external
        enter: 2, // self 
        init: 2, // no specific child selected
      },
      "w": {
        exit: 1,
        enter: 2,
        init: 0, // no children
      },
    });
  });
  it("should handle the internal self transition of a leaf region", function() {
    var p = makeParallel();
    p.send("j-");
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).enterExit({
      "A": {
        init: 0, // all children are automatically activated, so zero inits.
        exit: 0, // not leaving A
        enter: 1,
      },
      "j": {
        exit: 0, // self transition internally
        enter: 1, // no exit so only 1 enter
        init: 2, // 2 inits -- as if we exited, but we did so only internally,
      },
      "w": {
        exit: 1, // the child counts should match the same as the external case
        enter: 2,
        init: 0, // no children
      },
    });
  });
  it("should handle the self transition of a parallel region", function() {
    var p = makeParallel();
    p.send("AA");
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).enterExit({
      "A": {
        init: 0, // all children are automatically activated, so zero inits.
        exit: 1, // self transition, so it should exit self
        enter: 2,
      },
      "i": {
        init: 2,
        enter: 2,
        exit: 1,
      },
    });
  });
  it("should handle the internal transition of a parallel region", function() {
    var p = makeParallel();
    p.send("A-");
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).enterExit({
      "A": {
        init: 0, // all children are automatically activated, so zero inits.
        exit: 0, // internal-self transition, so it shouldnt exit self
        enter: 1,
      },
      "i": {
        exit: 1, // but A does exit its contents
        enter: 2,
        init: 2, // shouldnt exit region either
      },
    });
  });
  it("should transition from a parent of a region, out of the region", function() {
    var p = makeParallel();
    p.send("hq");
    expect(p).active("h", "q");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 1,
      }
    });
  });
  it("should transition from inside the region, out of the region", function() {
    var p = makeParallel();
    p.send("wq");
    expect(p).active("h", "q");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 1,
      }
    });
  });
  it("should transition from the region, out of the region", function() {
    var p = makeParallel();
    p.send("Aq");
    expect(p).active("h", "q");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 1,
      }
    });
  });
  it("should transition from the region, into the region", function() {
    var p = makeParallel();
    p.send("Ak");
    expect(p).active("h", "A", "i", "x", "j", "w", "k");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 0,
      },
      "k": {
        enter: 2,
        exit: 1,
      },
    });
  });
  it("should transition from the region, into a sub-region", function() {
    var p = makeParallel();
    p.send("Az");
    expect(p).active("h", "A", "i", "B", "l", "y", "m", "z", "j", "w", "k");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 0, // internal transition so no exit
      },
      "i": {
        enter: 2,
        exit: 1,
      },
      "x": {
        enter: 1,
        exit: 1,
      },
      "B": {
        enter: 1,
        exit: 0,
      },
    });
  });
  it("should allow a region's inner transition without exiting the region's container, and without exiting the region's siblings", function() {
    var p = makeParallel();
    p.send("xi");
    expect(p).active("h", "A", "i", "x", "j", "k", "w");
    expect(p).actions("xi");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 0,
      },
      "j": {
        enter: 1,
        exit: 0,
      },
      "k": {
        enter: 1,
        exit: 0
      },
      // shouldnt exit: internal transition
      "i": {
        enter: 1,
        exit: 0,
        init: 2,
      },
      "x": {
        enter: 2,
        exit: 1,
      },
    });
  });
  it("should allow multiple inner transitions with no parent exit", function() {
    var p = makeParallel();
    p.send("kk", "jw", "xi"); // simultanously trigger a parent-to-child, a child-to-parent, and a self transition.
    expect(p).active("h", "A", "i", "x", "j", "k", "w"); // same as default active
    expect(p).actions("jw", "xi", "kk"); // in processing order
    expect(p).enterExit({
      "A": {
        exit: 0,
        enter: 1,
      },
      "i": {
        exit: 0,
        enter: 1,
        init: 2,
      },
      "x": {
        exit: 1,
        enter: 2,
      },
      "j": {
        exit: 0,
        enter: 1,
        init: 1,
      },
      "w": {
        exit: 1,
        enter: 2,
      },
      "k": {
        exit: 1,
        enter: 2,
      },
    });
  });
  it("should merge the inner transitions from regions of different depths", function() {
    var p = makeParallel("B"); // start with B ( and A ) active.
    expect(p).active("h", "A", "i", "B", "l", "y", "m", "z", "j", "w", "k");
    p.send("yt", "kk", "jw"); //  trigger one transition in b and one in a
    expect(p).actions("jw", "yt", "kk"); // processing is depth first order
    expect(p).active("h", "A", "i", "B", "l", "t", "m", "z", "j", "w", "k");
    expect(p).enterExit({
      "A": {
        exit: 0,
        enter: 1,
      },
      "B": {
        exit: 0,
        enter: 1,
      },
      "l": {
        init: 1,
        enter: 1,
        exit: 0,
      },
      "y": {
        enter: 1,
        exit: 1,
        init: 0,
      },
      "t": {
        enter: 1,
        exit: 0,
        init: 0,
      },
      "m": {
        exit: 0,
        enter: 1,
        init: 1,
      },
      "z": {
        enter: 1,
        exit: 0,
        init: 0,
      },
      "i": {
        exit: 0,
        enter: 1,
        init: 0,
      },
      "x": {
        enter: 0,
      },
      "j": {
        init: 1,
        enter: 1,
        exit: 0,
      },
      "w": {
        exit: 1,
        enter: 2,
      },
      "k": {
        exit: 1,
        enter: 2,
      },
    });
  });
  it("should allow a parent region transition to override a child region's transition", function() {
    var p = makeParallel();
    p.send("xi", "hq");
    expect(p).active("h", "q");
    expect(p).actions("hq");
    expect(p).enterExit({
      "A": {
        enter: 1,
        exit: 1,
      }
    });
  });
  it("should allow entering a sibling region state", function() {
    var b = makeBase();
    //
    var m = hsmService.newMachine("test", {});
    var sR = b.addCommon(m, "R");
    var sP = b.addParallel(sR, "P");
    var sA = b.addCommon(sP, "A");
    var sx = b.addCommon(sA, "x");
    var sy = b.addCommon(sA, "y");
    var sB = b.addCommon(sP, "B");
    var sB = b.addCommon(sB, "z");
    //
    b.start(m);
    expect(b).active("R", "P", "A", "x", "B", "z");
    b.send("zy");
    expect(b).active("R", "P", "A", "y", "B", "z");
  });
});
