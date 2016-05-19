'use strict';

angular.module('hsm')

.directive("hsmLog", function($log) {
  var log = function(logger, msg) {
    if (logger.test) {
      logger.test.next(msg);
    }
    $log.info("hsm:", msg);
  };

  var hsmLog = function() {};
  hsmLog.prototype.enter = function(state) {
    log(this, state.name + "-ENTRY");
  };
  hsmLog.prototype.exit = function(state) {
    log(this, state.name + "-EXIT");
  };
  hsmLog.prototype.init = function(state) {
    log(this, state.name + "-INIT");
  };
  hsmLog.prototype.error = function(msg) {
    $log.error.apply($log, arguments);
    throw new Error(msg);
  };
  hsmLog.prototype.warn = function(msg) {
    $log.warn.apply($log, arguments);
  };
  hsmLog.prototype.info = function(msg) {
    $log.info.apply($log, arguments);
  };
  return {
    controller: hsmLog,
    require: ["hsmLog"],
    link: function(scope, element, attrs, controllers) {
      var ctrl = controllers[0];
      var name = attrs["hsmLog"];
      scope[name] = ctrl;
    },
  };
})

.directive('hsmEvent', function(hsm, hsmParse, $log) {
  return {
    controller: function() {
      this.init = function(hsmState, name, when, handler) {
        hsmState.addEventHandler(name, handler, when);
      };
    },
    restrict: 'E',
    require: ["^^hsmState", "hsmEvent"],
    controllerAs: "hsmEvent",
    link: function(scope, element, attrs, controllers) {
      var hsmState = controllers[0];
      var hsmEvent = controllers[1];
      //
      var on = hsmParse.getString("on", scope, attrs);
      var goTo = hsmParse.getString("goto", scope, attrs);
      var run = hsmParse.getEvalFunction("run", attrs);
      var when = hsmParse.getEvalFunction("when", attrs);
      if (goTo) {
        if (run) {
          var msg = "only either 'run' or 'goto' may be specified.";
          $log.error(msg, run, goTo);
          throw new Error(msg)
        }
        run = function() {
          return goTo;
        }
      }
      //
      hsmEvent.init(hsmState, on, when, run);
    }
  }
})

.directive('hsmState', function(hsm, hsmParse, $log) {
  var GuardedFunction = function(cb, guard) {
    this.invoke = function(scope, args) {
      if (!guard || guard(scope, args)) {
        return cb(scope, args);
      }
    };
  };

  var hsmState = function($scope) {
    this.name;
    this.active;
    this.state;
    this.hsmParent;
    this.events = {};
    this.scope = $scope;
  };
  //
  hsmState.prototype.$onDestroy = function() {
    this.hsmMachine = null;
    this.hsmParent = null;
    this.state = null;
    this.events = null;
    this.userEnter = null;
    this.userExit = null;
    this.userInit = null;
    this.scope = null;
  };
  // record the real hsm state
  hsmState.prototype.init = function(hsmMachine, hsmParent, name, opt) {
    this.name = name;
    this.active = false;
    this.hsmMachine = hsmMachine;
    this.hsmParent = hsmParent;
    this.userEnter = opt.onEnter;
    this.userExit = opt.onExit;
    this.userInit = opt.onInit;
    //
    var self = this;
    this.state = hsmMachine.addState(this, hsmParent, {
      onEnter: function(status) {
        return self.onEnter(status);
      },
      onExit: function(status) {
        return self.onExit(status);
      },
      onInit: function(status) {
        return self.onInit(status);
      },
      parallel: opt.parallel
    });
  };
  hsmState.prototype.onEnter = function(status) {
    this.active = true;
    if (this.userEnter) {
      this.userEnter(this.state, status.reason());
    }
  };
  hsmState.prototype.onExit = function(status) {
    this.active = false;
    if (this.userExit) {
      this.userExit(this.state, status.reason());
    }
  };
  hsmState.prototype.onInit = function(status) {
    if (this.userInit) {
      var next = this.userInit(this.state, status.reason());
      if (next) {
        // FIX: we shouldnt be looking this up -- the machine should be.
        var nextState = this.hsmMachine.findState(next);
        if (!nextState) {
          var msg = "state not found";
          $log.error(msg, next);
          throw new Error(msg);
        } else {
          return nextState.state;
        }
      }
    }
  };
  // ctrl api: add a new event handler
  hsmState.prototype.addEventHandler = function(name, cb, guard) {
    var key = name.toLowerCase();
    var prev = this.events[key];
    var fn = new GuardedFunction(cb, guard);
    if (prev) {
      prev.push(fn);
    } else {
      this.events[key] = [fn];
    }
  };
  // handler for event callbacks.
  hsmState.prototype.onEvent = function(evt, data) {
    var dest;
    if (this.active) {
      var fns = this.events[evt];
      if (fns) {
        for (var i = 0; i < fns.length; i++) {
          var fn = fns[i];
          dest = fn.invoke(this.scope, {
            "$evt": data || evt
          });
          if (!angular.isUndefined(dest)) {
            break;
          }
        } // for
      }
    }
    return dest;
  };

  var stateCount = 0;

  return {
    controller: hsmState,
    transclude: true,
    template: '',
    scope: true,
    controllerAs: "hsmState",
    require: ["^^hsmMachine", "?^^hsmState", "hsmState"],
    link: function(scope, element, attrs, controllers, transcludeFn) {
      stateCount += 1;
      var hsmMachine = controllers[0];
      var hsmParent = controllers[1] || hsmMachine;
      var hsmState = controllers[2];
      var srcExp = attrs['hsmState'] || attrs['name'];
      var name = hsmParse.getString(srcExp, scope) || ("hsmState" + stateCount);
      var parallel = hsmParse.getEvalFunction("parallel", attrs);
      var opt = hsmParse.getOptions(scope, attrs, {
        parallel: parallel && parallel(scope)
      });
      hsmState.init(hsmMachine, hsmParent, name, opt);
      //
      transcludeFn(scope, function(clone) {
        element.append(clone);
      });
    }, //link
  }
})

.directive('hsmMachine', function(hsm, hsmParse, $log, $timeout) {
  var pcount = 0;
  // FIX: should separate the dynamic tree and the region desc.
  // the machine, or tree, not the controller api should handle this.
  var Node = function(name, region) {
    this.name = name;
    this.region = region;
    this.kids = [];
  };
  // i want to kill the current implementation with fire
  // wrap things to build this dynamically, containing only active nodes.
  Node.prototype.build = function(state) {
    var node = this;
    state.region.children.forEach(function(child) {
      var parent = node;
      if (child.region.concurrent()) {
        var next = parent = new Node(child.name, child.region);
        node.kids.push(next);
      }
      parent.build(child);
    });
  };

  var hsmMachine = function() {
    this.name;
    this.states = {};
    this.machine;
    this.stage = "default";
  };
  hsmMachine.prototype.$onDestroy = function() {
    $log.info("destroying", this.name);
    this.machine = null;
    this.states = null;
    this.stage = "dead";
  };
  // directive initialization.
  hsmMachine.prototype.init = function(name, opt) {
    this.name = name;
    this.stage = "registration";
    // FIX: this should be onEvent -- and there should be an onEvent core handler and sniffer for hsmState as well.
    this.onEmit = opt.onEmit;
    this.machine = hsm.newMachine(name, opt);
    // FIX: we shouldnt need a root state
    this.state = hsm.newState(name);
    this.state.wantittowork = {
      onEvent: function() {}
    };
    // that which we expose to the scope:
    var machineScope = function(machine) {
      this.name = name;
      this.emit = function(evt, data) {
        machine.emit(evt, data);
      };
    };
    return new machineScope(this);
  };
  hsmMachine.prototype.start = function() {
    if (this.stage != "registration") {
      var msg = "hsmMachine starting, invalid stage:";
      $log.error(msg, this.stage);
      throw new Error(msg);
    };
    this.stage = "initialized";

    this.regionTree = new Node(this.state.name);
    this.regionTree.build(this.state);

    this.machine.start(this.state);
    this.stage = "started";
  };
  hsmMachine.prototype.emit = function(name, data) {
    if (this.onEmit) {
      //function(state, cause, target)
      this.onEmit(null, {
        name: name,
        data: data
      }, null);
    }
    // ive never found a good description of how propogation should occur
    // under parallel states.
    var machine = this.machine;
    var evt = name.toLowerCase();

    var tryit = function(state, depth) {
      //$log.debug(evt, "*".repeat(depth), state.name);
      var dest = state.wantittowork.onEvent(evt, data);
      if (!angular.isUndefined(dest)) {
        return {
          src: state,
          dest: dest,
        }
      }
    };
    // return a single change from a state tree.
    var handleTree = function(state, depth) {
      var handled;
      var region = state.region;
      if (!region.concurrent()) {
        var states = region.children;
        for (var i = 0; i < states.length; i += 1) {
          var c = states[i];
          if (machine.isActive(c.name)) {
            handled = handleTree(c, depth + 1);
            break;
          }
        }
      }
      return handled || tryit(state, depth);
    };
    // accumulate changes from regions
    var handleRegions = function(node, depth) {
      var resp;
      if (machine.isActive(node.name)) {
        // try deepest active region first
        for (var i = 0; i < node.kids.length; i += 1) {
          var kid = node.kids[i];
          resp = handleRegions(kid, depth + 1);
          if (resp) {
            break;
          }
        }
        // still not handled ( ex. no subregion, we are deepest )
        // we have to exclude the hacky root node which is a non-concurrent region.
        if (!resp && node.region) {
          //$log.debug(evt, "-".repeat(depth), node.name);
          // broadcast to every state tree in our region
          node.region.children.forEach(function(state) {
            var h = handleTree(state, depth);
            if (h) {
              resp = resp || [];
              resp.push(h);
            }
          });
        }
      }
      return resp;
    };
    var resp = handleRegions(this.regionTree, 1);
    if (!resp) {
      // we can pass zero for depth, the hacky root never has a handler
      var h = handleTree(this.state, 0);
      if (h) {
        resp = [h];
      }
    }
    if (resp) {
      var states = this.states;
      var enters = resp.map(function(h) {
        var dest = h.dest;
        if (angular.isString(dest)) {
          var key = dest.toLowerCase();
          var next = states[key];
          if (!next) {
            var msg = "missing state";
            $log.error(msg, dest, states);
            throw new Error(msg);
          }
          return machine.changeStates(h.src, next.state, evt, data);
        }
      });

      enters.forEach(function(e) {
        if (e) {
          e();
        }
      });
    }
  }; // emit
  // ctrl api: add a sub state
  hsmMachine.prototype.addState = function(hsmState, hsmParent, opts) {
    if (this.stage != "registration") {
      var msg = "invalid registration";
      $log.error(msg, this.stage);
      throw new Error(msg);
    };
    var name = hsmState.name;
    var key = name.toLowerCase();
    if (this.states[key]) {
      var msg = "duplicate state";
      $log.error(msg, name);
      throw new Error(msg);
    }
    if (!hsmParent.state) {
      var msg = "parent not initialized";
      $log.error(msg, hsmParent.name);
    }
    this.states[key] = hsmState;
    var state = hsmParent.state.newState(name, opts);
    // FIX: like i said, burn this implementation with fire.
    // the events are on the directive side of things, not the service.
    // but we only have the service nodes at emit time.
    state.wantittowork = hsmState;
    return state;
  };
  //
  hsmMachine.prototype.findState = function(name) {
    var key = name.toLowerCase();
    return this.states[key];
  };
  //
  return {
    controller: hsmMachine,
    transclude: true,
    template: '',
    scope: true,
    controllerAs: "hsmMachine",
    require: ["hsmMachine"],
    link: function(scope, element, attrs, controllers, transcludeFn) {
      var ctrl = controllers[0];
      var srcExp = attrs['hsmMachine'] || attrs['name'];
      var name = hsmParse.getString(srcExp, scope) || "hsmMachine";
      var opt = hsmParse.getOptions(scope, attrs);
      var hsmMachine = ctrl;
      scope[name] = hsmMachine.init(name, opt);
      //
      var includes = 0;
      // *sigh* we have to wait for the digest to take place to get the include requested events. instead of relying side-effects ( as it so often seems to ), it would be better if angular had an actual api to manage content -- if that api were based on promises even better still.
      var tryStartUp = function() {
        if (!includes) {
          $timeout(function() {
            if (!includes) {
              includes = -1;
              hsmMachine.start();
            }
          });
        }
      };
      transcludeFn(scope, function(clone) {
        scope.$on("$includeContentError", function() {
          //
        });
        scope.$on("$includeContentRequested", function() {
          if (includes >= 0) {
            includes += 1;
          }
        });
        scope.$on("$includeContentLoaded", function() {
          if (includes > 0) {
            includes -= 1;
            tryStartUp();
          }
        });
        //
        element.append(clone);
        tryStartUp();
      });
    }
  };
});
