'use strict';

/** 
 * port of https://github.com/ionous/hsm-statechart/blob/master/hsm/hsm_machine.c
 * fix: consider removing angular dependency in favor of something like requirejs...
 */
angular.module('hsm', [])
  .factory('hsm', function($log) {
    // currently state names must be globally unique
    var stateByName = {};
    // helper: to avoid if function testing.
    var doNothing = function() {};

    // regions hold child states
    var Region = function(name, initialize, isConcurrent) {
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
      if (stateByName[name]) {
        throw new Error("state " + name + " already registered");
      }
      stateByName[name] = this;
      this.name = name;
      this.parent = parent;
      this.depth = parent ? parent.depth + 1 : 0;
      //
      var myEnter = opt.onEnter || doNothing;
      var myExit = opt.onExit || doNothing;
      var myRegion = new Region(name, opt.onInit, opt.parallel);
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
        // $log.debug("entering children of", name);
        var best, err;
        if (!myRegion.exists()) {
          if (path.length) {
            err = "empty region";
          }
        } else {
          best = path.pop() || myRegion.init(status);
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

      // for debugging; but combine?
      var activeStates = {};
      var activeChildren = {};

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
      var makeStatus = function(cause) {
        return {
          onEnter: function(state) {
            toggle(state.name, true);
            return onEnter(state, cause);
          },
          onInit: onInit ? function(state) {
            return onInit(state.parent, cause);
          } : doNothing,
          onExit: function(state) {
            toggle(state.name, false);
            return onExit(state, cause);
          },
          reason: function() {
            return cause;
          },
          validate: function(name, awake) {
            return validate(name, awake);
          },
          // helpers to keep regional choices in machine memory.
          setChild: function(name, child) {
            var noChildLeftBehind = !activeChildren[name];
            if (noChildLeftBehind) {
              //$log.debug("setting child", name + "." + child.name);
              activeChildren[name] = child;
            }
            return noChildLeftBehind;
          },
          clearChild: function(name) {
            var prev = activeChildren[name];
            if (prev) {
              //$log.debug("clearing child", name + "." + prev.name);
              delete activeChildren[name];
            }
            return prev;
          },
        };
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
          lca.enter(status,pathToTarget);
        } else {
          lca.exitChildren(status);
          lca.enterChildren(status, pathToTarget);
        }
      };
      // returns true unless errored or finished.
      this.isRunning = function() {
        return !!running && !terminated && !errored;
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
        var status = makeStatus();
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
      this.changeStates = function(source, target, cause) {
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
            var status = makeStatus(cause);
            onTransition(source, cause, target);
            try {
              transition(source, target, status);
              running = target;
              okay = true;
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
      findState: function(name) {
        return stateByName[name];
      },
      newState: function(name, opt) {
        return new State(name, opt);
      },
    };
  });
