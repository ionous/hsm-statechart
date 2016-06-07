'use strict';

angular.module('hsm')

// 
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
  hsmLog.prototype.debug = function(msg) {
    $log.debug.apply($log, arguments);
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

.service("hsmCause", function() {
  var Cause = function(name, data) {
    this.name = name;
    this.data = data;

  };
  Cause.prototype.reason = function() {
    return this.name;
  };
  return {
    newCause: function(name, data) {
      return new Cause(name, data);
    },
  };
})

// parse angular html attributes
.service("hsmParse", function($interpolate, $parse) {
  var makeCallback = function(p, scope) {
    return function(state, cause, target) {
      var extra = {
        "$state": state,
        "$source": state,
        "$evt": cause, // .name, .data
        "$target": target,
      };
      return p(scope, extra);
    };
  };
  var service = {
    getString: function(key, scope, attrs) {
      var n = attrs ? attrs[key] : key;
      return n && $interpolate(n)(scope.$parent);
    },
    getEvalFunction: function(key, attrs) {
      var v = attrs[key];
      return v && $parse(v);
    },
    getOption: function(key, scope, attrs) {
      var p = service.getEvalFunction(key, attrs);
      return p && makeCallback(p, scope);
    },
  };
  return service;
})

.directive('hsmEvent', function(hsmService, hsmParse, $log) {
  return {
    restrict: 'E',
    require: ["^^hsmState"],
    link: function(scope, element, attrs, controllers) {
      var on = hsmParse.getString("on", scope, attrs);
      var when = hsmParse.getEvalFunction("when", attrs);
      var dest = hsmParse.getString("goto", scope, attrs);
      var run = hsmParse.getEvalFunction("run", attrs);
      var hsmState = controllers[0];
      if (!dest) {
        // automatic self-transition
        dest = hsmState.name;
      }
      hsmState.addEventHandler(on, when, dest, run)
    }
  }
})

.directive('hsmState', function(hsmService, hsmParse, $log) {
  var GuardedFunction = function(when, dest, cb) {
    this.handle = function(hsmMachine, scope, args, then) {
      var ok = !when || when(scope, args);
      if (ok) {
        var state = hsmMachine.getState(dest);
        var after = then.goto(state);
        if (cb) {
          after.run(function() {
            cb(scope, args);
          });
        }
      }
      return ok;
    };
  };

  // our controller constructor function
  var hsmState = function($scope) {
    var events = {};
    //
    this.init = function(hsmMachine, hsmParent, name, opt) {
      this.name = name;
      this.active = false;
      //
      var userEnter = opt.userEnter;
      var userInit = opt.userInit;
      var userExit = opt.userExit;
      //
      // create our state
      var self = this;
      var state = hsmMachine.addState(this, hsmParent, {
        parallel: opt.parallel,
        onEnter: function(state, cause) {
          self.active = true;
          if (userEnter) {
            userEnter(state, cause && cause.reason());
          }
        },
        onInit: function(state, cause) {
          var ret;
          if (userInit) {
            var name = userInit(state, cause && cause.reason());
            if (name) {
              ret = hsmMachine.getState(name);
            }
          }
          return ret;
        },
        onEvent: function(state, cause, then) {
          var fns = events[cause.name];
          if (fns) {
            var extra = {
              "$evt": cause.data || cause.name
            };
            for (var i = 0; i < fns.length; i += 1) {
              var fn = fns[i];
              if (fn.handle(hsmMachine, $scope, extra, then)) {
                break;
              }
            }
          }
        },
        onExit: function(state, cause) {
          self.active = false;
          if (userExit) {
            userExit(state, cause.reason());
          }
        }
      });
      this.getState = function() {
        return state.state();
      };
      // make a child state
      this.makeKid = function(name, opts) {
        return state.newState(name, opts);
      };
      //
      this.addEventHandler = function(evt, when, dest, run) {
        var key = evt.toLowerCase();
        var prev = events[key];
        var fn = new GuardedFunction(when, dest, run);
        if (prev) {
          prev.push(fn);
        } else {
          events[key] = [fn];
        }
      };
    }; // init
  }; // hsmState constructor

  // ctrl api: add a new event handler
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
      // one-time bindings
      var name = hsmParse.getString(srcExp, scope) || ("hsmState" + stateCount);
      var parallel = hsmParse.getEvalFunction("parallel", attrs);
      //
      var opt = {
        userEnter: hsmParse.getOption("hsmEnter", scope, attrs),
        userInit: hsmParse.getOption("hsmInit", scope, attrs),
        userExit: hsmParse.getOption("hsmExit", scope, attrs),
        parallel: parallel && parallel(scope)
      };
      hsmState.init(hsmMachine, hsmParent, name, opt);
      //
      transcludeFn(scope, function(clone) {
        element.append(clone);
      });
    }, //link
  }
})

.directive('hsmMachine',
  function(hsmCause, hsmParse, hsmService, $log, $timeout) {
    var stage = {
      default: "default",
      registration: "registration",
      initialized: "initialized",
      started: "started",
      emitting: "emitting",
      dead: "dead",
    };

    var hsmMachine = function() {
      this.name = "";
      this.machine = null;
      this.states = {}; // we need a name map for state return lookup
      this.stage = stage.default;
    };
    hsmMachine.prototype.$onDestroy = function() {
      $log.info("destroying", this.name);
      this.stage = stage.dead;
    };
    // directive initialization.
    hsmMachine.prototype.init = function(name, opt) {
      this.name = name;
      this.stage = stage.registration;

      var userEnter = opt.userEnter;
      var userInit = opt.userInit;
      var userEvent = opt.userEvent;
      var userExit = opt.userExit;
      var userTransition = opt.userTransition;

      this.machine = hsmService.newMachine(name, {
        onEnter: function(state, cause) {
          return userEnter && userEnter(state, cause);
        },
        onInit: function(state, cause) {
          return userInit && userInit(state, cause);
        },
        onEvent: function(state, cause) {
          // note: this for happens every state; you may want smething different
          return userEvent && userEvent(state, cause);
        },
        onExit: function(state, cause) {
          return userExit && userExit(state, cause);
        },
        onTransition: function(state, cause, target) {
          return userTransition && userTransition(state, cause, target);
        },
      });

      // that which we expose to the scope:
      var machineScope = function(machine) {
        this.name = name;
        this.emit = function(namespace, name, data) {
          machine.emit(namespace, name, data);
        };
      };
      return new machineScope(this);
    };
    hsmMachine.prototype.start = function() {
      if (this.stage != stage.registration) {
        var msg = "hsmMachine starting, invalid stage:";
        $log.error(msg, this.stage);
        throw new Error(msg);
      };
      this.stage = stage.initialized;
      this.machine = this.machine.start(); // overwrite with actual machine.
      this.stage = stage.started;
    };
    hsmMachine.prototype.emit = function(_namespace, _name, _data) {
      var scoped = !angular.isUndefined(_data);
      var name = scoped ? [_namespace, _name].join("-") : _namespace;
      var data = scoped ? _data : _name;
      var cause = hsmCause.newCause(name, data);

      var stage = this.stage;
      this.stage = "emitting";
      this.machine.emit(cause);
      this.stage = stage;
    };
    // add a substate
    hsmMachine.prototype.addState = function(hsmState, hsmParent, opts) {
      if (this.stage != stage.registration) {
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
      this.states[key] = hsmState;
      return hsmParent.makeKid(name, opts);
    };
    hsmMachine.prototype.makeKid = function(name, opts) {
      return this.machine.newState(name, opts);
    };
    // returns hsmState
    hsmMachine.prototype.findState = function(name) {
      var key = name.toLowerCase();
      return this.states[key];
    };
    // returns hsmState
    hsmMachine.prototype.getState = function(name) {
      var ret = this.findState(name);
      if (!ret) {
        var msg = "state not found";
        $log.error(msg, name);
        throw new Error(msg);
      }
      return ret.getState();
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
        var hsmMachine = ctrl;
        // fix? add hsmEmit again?
        var opt = {
          userEnter: hsmParse.getOption("hsmEnter", scope, attrs),
          userInit: hsmParse.getOption("hsmInit", scope, attrs),
          userEvent: hsmParse.getOption("hsmEvent", scope, attrs),
          userExit: hsmParse.getOption("hsmExit", scope, attrs),
          userTransition: hsmParse.getOption("hsmTransition", scope, attrs),
        };
        //
        scope[name] = hsmMachine.init(name, opt);

        //
        var includes = 0;
        // we have to wait for the digest to take place to get the include requested events. instead of relying side-effects ( as it so often seems to ), it would be better if angular had an actual api to manage content -- if that api were based on promises even better still.
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
