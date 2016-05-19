'use strict';

/** 
 * port of https://github.com/ionous/hsm-statechart/blob/master/hsm/hsm_machine.c
 * fix: consider removing angular dependency in favor of something like requirejs...
 */
angular.module('hsm', [])

.service("hsmParse", function($interpolate, $parse) {
  var service = {
    getString: function(key, scope, attrs) {
      var n = attrs ? attrs[key] : key;
      return n && $interpolate(n)(scope.$parent);
    },
    getEvalFunction: function(key, attrs) {
      var v = attrs[key];
      return v && $parse(v);
    },
    // turn html attributes into helper functions
    getOptions: function(scope, attrs, base) {
      var map = {
        "hsmEnter": "onEnter",
        "hsmExit": "onExit",
        "hsmInit": "onInit",
        "hsmError": "onError",
        "hsmEmit": "onEmit",
        "hsmTransition": "onTransition",
        "hsmExternal": "isExternal"
      };
      var keys = Object.keys(map);
      // this helps with capturing the right closure.
      var parsed = keys.map(function(el) {
        var attr = attrs[el];
        return attr && $parse(attr);
      });
      var opt = base || {};
      keys.forEach(function(el, index) {
        var p = parsed[index];
        if (p) {
          var out = map[el];
          opt[out] = function(state, cause, target) {
            var extra = {
              "$state": state,
              "$source": state,
              "$evt": cause,
              "$target": target,
            };
            return p(scope, extra);
          };
        }
      });
      return opt;
    }
  };
  return service;
})


.factory('hsm', function($log) {
  // helper: to avoid if function testing.
  var doNothing = function() {};

  // regions hold child states
  var regionCounter = 0;
  var Region = function(name, initialize, isConcurrent) {
    var that = this;
    this.regionId = ++regionCounter;
    this.concurrent= function() {
      return isConcurrent;
    };
    //
    var kids = this.children = [];
    this.addChild = function(el) {
      kids.push(el);
    };
    this.exists = function() {
      return !!kids.length;
    };
    // init the node to determine a desired child.
    this.init = function(status) {
      var child;
      if (kids.length) {
        child = (initialize && initialize(status)) || kids[0];
        if (child) {
          status.onInit(child);
        }
      }
      return child;
    };
    // activate to enter the region's children
    this.activate = function(child, status, path) {
      if (status.setChild(name, child)) {
        child.enter(status, path);
        // although many states are active
        // we use the "initial" state for a parallel state as a key
        if (isConcurrent) {
          // all other parallel children are default entered.
          // note: we only use path for the initial child.
          kids.forEach(function(c) {
            if (c !== child) {
              c.enter(status);
            }
          });
        }
      }
    };
    // deactivate to exit the region's children.
    this.deactivate = function(status) {
      // because transitions move up to the lca from source
      // we lose information about which states were active
      // we cant assert on an inactive child therefore.
      var child = status.clearChild(name);
      if (child) {
        if (isConcurrent) {
          kids.forEach(function(c) {
            if (c !== child) {
              c.exit(status);
            }
          });
        }
        child.exit(status);
      }
    };
  }; // Region

  // states are independent of their machine(s)
  var State = function(name, parent, _opt) {
    var opt = _opt || {};
    this.name = name;
    this.parent = parent;
    this.depth = parent ? parent.depth + 1 : 0;
    //
    var myEnter = opt.onEnter || doNothing;
    var myExit = opt.onExit || doNothing;
    var myRegion = this.region = new Region(name, opt.onInit, opt.parallel);
    var mySelf = this;
    //
    this.newState = function(name, opt) {
      var child = new State(name, mySelf, opt);
      myRegion.addChild(child);
      return child;
    };
    // throws if no parent.
    this.upstate = function(state) {
      if (!parent) {
        throw new Error("jumped past top");
      }
      return parent;
    };
    // enter state and region following the specified path.
    this.enter = function(status, path) {
      //$log.info("State: enter", name, myRegion, myRegion.exists(), mySelf);
      status.onEnter(mySelf);
      myEnter(status);
      mySelf.enterChildren(status, path || []);
    };
    // exit this state and its region.
    this.exit = function(status) {
      mySelf.exitChildren(status);
      status.onExit(mySelf);
      myExit(status);
    };
    // throws if children have already been exited.
    this.exitChildren = function(status) {
      // $log.debug("exiting children of", name);
      if (myRegion.exists()) {
        myRegion.deactivate(status);
      }
    };
    // used by transitioning machines.
    // throws if children have already been entered.
    this.enterChildren = function(status, path) {
      //$log.debug("entering children of", name, myRegion.regionId);
      var best, err;
      if (!myRegion.exists()) {
        //$log.debug("entering children of", name, "no region");
        if (path.length) {
          err = "empty region";
        }
      } else {
        best = path.pop() || myRegion.init(status);
        //$log.debug("entering children of", name, best);
        if (!best || (best.parent !== mySelf)) {
          err = "mismatched child";
        } else {
          myRegion.activate(best, status, path);
        }
      }
      if (err) {
        $log.error(err, name, best && best.name, status, path);
        throw new Error(err);
      }
    };
  }; // State

  // note: that onError, onFini are these states enter/exit....
  var fini = new State("$fini");
  var error = new State("$error");

  // opt: onEnter,onExit,etc.
  var Machine = function(name, _opt) {
    var opt = _opt || {};
    this.name = name;

    var running = false;
    var terminated = false;
    var errored = false;
    var onEnter = opt.onEnter || doNothing;
    var onInit = opt.onInit;
    var onExit = opt.onExit || doNothing;
    var onTransition = opt.onTransition || doNothing;
    var isExternal = opt.isExternal || doNothing;

    var activeStates = {}; // for debugging
    var activeChildren = {}; // init paths

    var validate = function(name, canActivate) {
      if (activeStates) {
        var expect = !!canActivate; // coerce ...
        var active = !!activeStates[name]; // to bool
        if (expect == active) {
          var msg = expect ? "expected inactive state" : "expected active state";
          $log.error(msg, name);
          throw new Error(msg);
        }
        return true;
      }
    };
    var toggle = function(name, active) {
      if (validate(name, active)) {
        activeStates[name] = active;
      }
    };
    var checkState = function(state, msg) {
      if (!(state instanceof State)) {
        $log.error(msg, state);
        throw new Error(msg);
      }
    };

    // private helper to change external cause into status and back again.
    var Status = function(cause, data) {
      this.onEnter = function(state) {
        toggle(state.name, true);
        return onEnter(state, cause, data);
      };
      this.onInit = onInit ? function(state) {
        return onInit(state.parent, cause, data);
      } : doNothing;
      this.onExit = function(state) {
        toggle(state.name, false);
        return onExit(state, cause, data);
      };
      this.reason = function() {
        return data || cause;
      };
    };
    Status.prototype.validate = function(name, awake) {
      return validate(name, awake);
    };
    // helpers to keep regional choices in machine memory.
    Status.prototype.setChild = function(name, child) {
      var noChildLeftBehind = !activeChildren[name];
      if (noChildLeftBehind) {
        //$log.debug("setting child", name + "." + child.name);
        activeChildren[name] = child;
      }
      return noChildLeftBehind;
    };
    Status.prototype.clearChild = function(name) {
      var prev = activeChildren[name];
      if (prev) {
        //$log.debug("clearing child", name + "." + prev.name);
        delete activeChildren[name];
      }
      return prev;
    };

    // helper to start machine.
    var start = function(dst, status) {
      var pathToTarget = [];
      while (dst.parent) {
        pathToTarget.push(dst);
        dst = dst.upstate();
      }
      dst.enter(status, pathToTarget);
    };
    // move from src to dst via lowest common ancestor.
    // by default doesnt exit the lca itself: just the lca's children.
    var transition = function(src, dst, status) {
      // record the path to get back down to dst later.
      var pathToTarget = [];
      var selfTransition = (src === dst);
      if (!selfTransition) {
        // move src to the lowest common depth.
        while (src.depth > dst.depth) {
          src = src.upstate();
        }
        // (or) move dst to the lowest common depth.
        while (dst.depth > src.depth) {
          pathToTarget.push(dst);
          dst = dst.upstate();
        }
        // if not at lca yet, go up till both sides meet.
        while (src !== dst) {
          src = src.upstate();
          pathToTarget.push(dst);
          dst = dst.upstate();
        }
      }
      // lca, src, and dst are all the same.
      var lca = src;
      // decide whether to exit the lca:
      // hsm-statechart exits for self-transitions, 
      // but otherwise defaults to internal transitions.
      if (selfTransition || isExternal()) {
        lca.exit(status);
        return function() {
          lca.enter(status, pathToTarget);
        }
      } else {
        lca.exitChildren(status);
        return function() {
          lca.enterChildren(status, pathToTarget);
        }
      }
    };
    // returns true unless errored or finished.
    this.isActive = function(x) {
      var ok;
      if (!!running && !terminated && !errored) {
        if (angular.isUndefined(x)) {
          ok = true;
        } else {
          ok = !!activeStates[x];
        }
      }
      return ok;
    };
    // start the machine:
    // the first state can be any state, not just a root or leaf.
    this.start = function(first) {
      checkState(first, "expected valid first state");
      if (running) {
        var err = "machine already running";
        $log.error(err, running);
        throw new Error(err);
      }
      var status = new Status();
      try {
        start(first, status);
        running = first;
      } catch (e) {
        errored = e;
        $log.error(e);
      }
    };
    // source: state containing the event which caused the transition.
    // target: destination state.
    // cause: optional event info.
    this.changeStates = function(source, target, cause, data) {
      var okay;
      checkState(source, "invalid source");
      checkState(target, "invalid target");
      if (!errored && !terminated) {
        if (target === fini) {
          // FIX: terminate should probably exit all.
          okay = terminated = true;
        } else if (target === error) {
          okay = errored = true;
        } else {
          var status = new Status(cause, data);
          onTransition(source, cause, target);
          try {
            okay = transition(source, target, status);
            running = target;
          } catch (e) {
            errored = e;
            $log.error(e);
          }
        }
      }
      return okay;
    };
  };
  // hsm service:
  return {
    newMachine: function(name, opt) {
      return new Machine(name, opt);
    },
    newState: function(name, opt) {
      return new State(name, opt);
    },
  };
});
