'use strict';

angular.module('hsm')

.service("hsmParse", function($interpolate, $parse) {
  var service = {
    getString: function(key, scope, attrs) {
      var n = attrs[key];
      return n && $interpolate(n)(scope.$parent);
    },
    getEvalFunction: function(key, attrs) {
      var v = attrs[key];
      return v && $parse(v);
    },
    // turn html attributes into helper functions
    getOptions: function(scope, attrs) {
      var map = {
        "hsmEnter": "onEnter",
        "hsmExit": "onExit",
        "hsmInit": "onInit",
        "hsmError": "onError",
        "hsmTransition": "onTransition",
        "hsmExternal": "isExternal"
      };
      var keys = Object.keys(map);
      // this helps with capturing the right closure.
      var parsed = keys.map(function(el) {
        var attr = attrs[el];
        return attr && $parse(attr);
      });
      var opt = {};
      keys.forEach(function(el, index) {
        var p = parsed[index];
        if (p) {
          var out = map[el];
          opt[out] = function(state, cause, target) {
            var extra = {
              "$state": state,
              "$source": state,
              "$cause": cause,
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
      var msg = "invalid registration";
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
              $log.error(msg, dest);
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
    if (this.states[name]) {
      var msg = "duplicate state";
      $log.error(msg, name);
      throw new Error(msg);
    }
    if (!hsmParent.state) {
      var msg = "parent not initialized";
      $log.error(msg, hsmParent.name);
    }
    this.states[name] = hsmState;
    var state = hsmParent.state.newState(name, opts);
    return state;
  };
  //
  hsmMachine.prototype.findState = function(name) {
    return this.states[name];
  };
  //
  // parallel regions are not meant to be addressable.
  // their containing state can be though
  hsmMachine.prototype.newParallelName = function() {
    var name = "parallel-" + pcount;
    pcount += 1;
    return name;
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
      transcludeFn(scope, function(clone) {
        var offErrored = scope.$on("$includeContentError", function() {
          //
        });
        var offRequested = scope.$on("$includeContentRequested", function() {
          includes += 1;
          $log.info("content requested", includes);
        });
        var offLoaded = scope.$on("$includeContentLoaded", function() {
          $log.info("content loaded", includes);
          includes -= 1;
          if (!includes) {
            hsmMachine.start();
            offErrored();
            offRequested();
            offLoaded();
          }
        });
        //
        element.append(clone);
        // this is insanity guys.
        $timeout(function() {
          if (!includes) {
            hsmMachine.start();
            offErrored();
            offRequested();
            offLoaded();
          }
        });
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
      }
    });
  };
  hsmState.prototype.onEnter = function(status) {
    this.active = true;
    if (!this.state.region.exists()) {
      // FIX: should separate the dynamic tree and the region desc.
      // the machine, or tree, not the controller api should handle this.
      this.hsmMachine.activateLeaf(name, this);
    }
    if (this.userEnter) {
      this.userEnter(this.scope, {
        "$evt": status.reason()
      });
    }
  };
  hsmState.prototype.onExit = function(status) {
    this.active = false;
    this.hsmMachine.activateLeaf(name, false);
    if (this.userExit) {
      this.userExit(this.scope, {
        "$evt": status.reason()
      });
    }
  };
  hsmState.prototype.onInit = function(status) {
    if (this.userInit) {
      var next = this.userInit(this.scope, {
        "$evt": status.reason()
      });
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

  return {
    controller: hsmState,
    restrict: 'E',
    transclude: true,
    template: '',
    scope: true,
    controllerAs: "hsmState",
    require: ["^^hsmMachine", "?^^hsmState", "hsmState"],
    link: function(scope, element, attrs, controllers, transcludeFn) {
      var hsmMachine = controllers[0];
      var hsmParent = controllers[1] || hsmMachine;
      var hsmState = controllers[2];
      var name = hsmParse.getString("name", scope, attrs);
      var parallel = hsmParse.getEvalFunction("parallel", attrs);
      parallel = parallel && parallel(scope);
      if (!name) {
        var msg = "invalid state name";
        $log.error(msg, name);
        throw new Error(msg);
      }
      var opt = hsmParse.getOptions(scope, attrs);
      hsmState.init(hsmMachine, hsmParent, name, opt);
      //
      transcludeFn(scope, function(clone) {
        element.append(clone);
      });
    }, //link
  }
})
