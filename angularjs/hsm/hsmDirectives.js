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

.directive('hsmMachine', function(hsm, hsmParse, $log, $timeout) {
  var pcount = 0;

  var hsmMachine = function() {
    // leafs is kind of a hack 
    // its original intention was to keep a short list of event targets for bubbling
    // its not clear how useful this is: when we transition we still need to find our source,
    // if we were capturing: we'd already know that source location.
    this.name;
    this.states = {};
    this.leafs = {};
    this.machine;
    this.stage = "default";
  };
  hsmMachine.prototype.$onDestroy = function() {
    this.machine = null;
    this.leafs = null;
    this.states = null;
    this.stage = "dead";
  };
  // directive initialization.
  hsmMachine.prototype.init = function(name, opt) {
    this.name = name;
    this.stage = "registration";
    this.machine = hsm.newMachine(name, opt);
    // FIX: we shouldnt need a root state
    this.state = hsm.newState(name);
  };
  hsmMachine.prototype.start = function() {
    if (this.stage != "registration") {
      var msg = "hsmMachine starting, invalid stage:";
      $log.error(msg, this.stage);
      throw new Error(msg);
    };
    this.stage = "initialized";
    this.machine.start(this.state);
    this.stage = "started";
  };
  // enable the event handler for the passed
  hsmMachine.prototype.activateLeaf = function(name, hsmState) {
    if (hsmState) {
      this.leafs[name] = hsmState;
    } else {
      delete this.leafs[name];
    }
  };
  // send events to leafs
  hsmMachine.prototype.emit = function(name, data) {
    var evt = name.toLowerCase();
    //$log.info("hsm: emitting", evt, data);
    var change = [];
    var handlers = [];
    // copy handlers from the active leaf nodes
    for (var k in this.leafs) {
      handlers.push(this.leafs[k]);
    }
    var enters = [];
    var states = this.states;
    var machine = this.machine;
    handlers.forEach(function(hsmState) {
      while (hsmState && hsmState.onEvent) {
        var dest = hsmState.onEvent(evt, data);
        if (angular.isUndefined(dest)) {
          hsmState = hsmState.hsmParent;
          continue;
        } else {
          if (dest) {
            var key = dest.toLowerCase();
            var next = states[key];
            if (!next) {
              var msg = "missing state";
              $log.error(msg, dest, states);
              throw new Error(msg);
            }
            var e = machine.changeStates(hsmState.state, next.state, evt, data);
            enters.push(e);
          }
          break;
        }
      } // while
    }); // handlers
    enters.forEach(function(e) {
      e();
    });
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
    restrict: 'E',
    transclude: true,
    template: '',
    scope: true,
    controllerAs: "hsmMachine",
    link: function(scope, element, attrs, controller, transcludeFn) {
      var name = hsmParse.getString("name", scope, attrs) || "root";
      var opt = hsmParse.getOptions(scope, attrs);
      var hsmMachine = controller;
      hsmMachine.init(name, opt);
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
})

.directive('hsmEvent', function(hsm, hsmParse, $log) {
  return {
    controller: function() {
      this.init = function(hsmState, name, when, handler) {
        hsmState.addEventHandler(name, handler, when);
      };
    },
    // undocumented, but wif you use the name of the directive as the controller,
    // and use a function for the controller spec, 
    // you can gain access to the controller *and* have a require
    // using a named controller and requiring it *does not* work.
    // https://github.com/angular/angular.js/issues/5893#issuecomment-65968829
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
    if (!this.state.region.exists()) {
      // FIX: should separate the dynamic tree and the region desc.
      // the machine, or tree, not the controller api should handle this.
      this.hsmMachine.activateLeaf(this.name, this);
    }
    if (this.userEnter) {
      this.userEnter(this.state, status.reason());
    }
  };
  hsmState.prototype.onExit = function(status) {
    this.active = false;
    this.hsmMachine.activateLeaf(this.name, false);
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
    restrict: 'E',
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
      var name = hsmParse.getString("name", scope, attrs);
      var parallel = hsmParse.getEvalFunction("parallel", attrs);
      name = name || ("unnamed" + stateCount);
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
