angular.module('hsm')

.directive("hsmLog",
  function($log) {
    'use strict';
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
        var name = attrs.hsmLog;
        scope[name] = ctrl;
      },
    };
  })

.service("hsmCause",
  function() {
    'use strict';
    var Cause = function(name, data, defer) {
      this.name = name;
      this.data = data;
      this.defer = defer; // can be null
    };
    Cause.prototype.toString = function() {
      return this.name;
    };
    Cause.prototype.reason = function() {
      return this.name;
    };
    Cause.prototype.resolve = function(value) {
      if (this.defer) {
        this.defer.resolve(value);
      }
    };
    Cause.prototype.reject = function(reason) {
      if (this.defer) {
        this.defer.reject(reason);
      }
    };
    var service = {
      normalizeName: function(name) {
        // angular's camelcasing alg
        var SPECIAL_CHARS_REGEXP = /([\:\-\_]+(.))/g;
        return name.replace(SPECIAL_CHARS_REGEXP, function(_, separator, letter, offset) {
          return offset ? letter.toUpperCase() : letter;
        });
      },
      newCause: function(name, data, defer) {
        var normalizedName = service.normalizeName(name);
        return new Cause(normalizedName, data, defer);
      },
    };
    return service;
  })

// parse angular html attributes
.service("hsmParse",
  function($interpolate, $log, $parse) {
    'use strict';
    var makeCallback = function(name, p, scope) {
      return function(state, cause, target) {
        var extra = {
          "$state": state,
          "$source": state,
          // see aslo onEvent()
          "$evt": cause && (cause.data || cause.name),
          "$evtname": cause && cause.name,
          "$target": target,
        };
        var ret;
        try {
          ret = p(scope, extra);
        } catch (e) {
          $log.error("hsmParse", name, "caught", e);
        }
        return ret;
      };
    };
    var service = {
      getString: function(key, scope, attrs) {
        var ret;
        var n = attrs ? attrs[key] : key;
        if (!angular.isUndefined(n)) {
          ret = $interpolate(n)(scope.$parent);
        }
        return ret;
      },
      getEvalFunction: function(key, attrs) {
        var ret;
        var v = attrs[key];
        if (!angular.isUndefined(v)) {
          ret = $parse(v);
        }
        return ret;
      },
      getOption: function(key, scope, attrs) {
        var p = service.getEvalFunction(key, attrs);
        return p && makeCallback(key, p, scope);
      },
    };
    return service;
  })

.directive('hsmEvent',
  function(hsmCause, hsmParse, hsmService, $log) {
    'use strict';
    return {
      restrict: 'E',
      require: ["^^hsmState"],
      link: function(scope, element, attrs, controllers) {
        var on = hsmParse.getString("on", scope, attrs);
        var when = hsmParse.getEvalFunction("when", attrs);
        var dest = hsmParse.getString("goto", scope, attrs);
        var run = hsmParse.getEvalFunction("run", attrs);
        var hsmState = controllers[0];
        if (angular.isUndefined(dest) && angular.isUndefined(run)) {
          throw new Error("empty event handler?");
        }
        var evt = hsmCause.normalizeName(on);
        hsmState.addEventHandler(evt, when, dest, run);
      }
    };
  })

.directive('hsmState',
  function(hsmService, hsmParse, $injector, $log) {
    'use strict';
    var displayStates;
    try {
      displayStates = $injector.get("ShowHsm");
    } catch (e) {}
    //
    var GuardedFunction = function(when, dest, cb) {
      this.handle = function(hsmMachine, src, scope, args, then) {
        var finished;
        if (!when || when(scope, args)) {
          // no destination specified? 
          // run the action in place and continue with the event.
          if (angular.isUndefined(dest)) {
            cb(scope, args);
          } else {
            var tgt, external;
            if (!dest) {
              tgt = src; // soft transition on a blank destination ( goto="" )
              external = false; // internal, no self exit/enter.
            } else {
              tgt = hsmMachine.getState(dest);
              if (!tgt) {
                $log.warn("GuardedFunction, no such state", dest);
              }
              external = undefined; // for default settings: external on self-transition.
            }
            var after = then.goto(tgt, external);
            if (cb) {
              // the action gets run on the
              after.run(function() {
                cb(scope, args);
              });
            }
            // skip the rest of the event handlers on this state if we initiated a change
            finished = true;
          }
        }
        return finished;
      };
    };

    // our controller constructor function
    var hsmState = function($log, $scope) {
      var events = {};
      //
      this.init = function(hsmMachine, hsmParent, name, opt) {
        this.name = (name || (hsmParent.name + "/" + hsmParent.kidCount));
        this.label = this.name + (opt.parallel ? "(p)" : "");
        this.autonamed = !name;
        this.active = false;
        this.kidCount = 0;
        this.nameDepth = hsmParent.nameDepth + (this.autonamed ? 0 : 1);
        this.parallel = opt.parallel;
        //
        var hsmEnter = opt.userEnter;
        var userInit = opt.userInit;
        var hsmExit = opt.userExit;
        var userEnters = [];
        var userExits = [];
        //
        // create our state
        var self = this;
        var state = hsmMachine.addState(this, hsmParent, {
          parallel: opt.parallel,
          onEnter: function(state, cause) {
            self.active = true;
            userEnters.forEach(function(userEnter) {
              userEnter(state, cause);
            });
            // run last so that local enters can construct resources used in hsm-enter
            if (hsmEnter) {
              hsmEnter(state, cause);
            }
          },
          onInit: function(state, cause) {
            var ret;
            if (userInit) {
              var name = userInit(state, cause);
              if (name) {
                var next = hsmMachine.getState(name);
                if (!next) {
                  $log.warn("onInit, no such state", state.name, "->", name);
                } else if (next.parent !== state) {
                  $log.error("onInit", next.name, "is not a child of", state.name);
                } else {
                  ret = next;
                }
              }
            }
            return ret;
          },
          onEvent: function(state, cause, then) {
            var fns = events[cause.name];
            if (fns) {
              var extra = {
                // see also makeCallback
                "$evt": (cause.data || cause.name),
                "$evtname": cause.name
              };
              for (var i = 0; i < fns.length; i++) {
                var fn = fns[i];
                if (fn.handle(hsmMachine, state, $scope, extra, then)) {
                  break;
                }
              }
            }
          },
          onExit: function(state, cause) {
            self.active = false;
            // exit in reverse ordre.
            if (hsmExit) {
              hsmExit(state, cause);
            }
            for (var i = userExits.length - 1; i >= 0; i--) {
              var userExit = userExits[i];
              userExit(state, cause);
            }
          },
        });
        this.onEnter = function(fn) {
          userEnters.push(fn);
        };
        this.onExit = function(fn) {
          userExits.push(fn);
        };
        this.getInstance = function() {
          return state.state();
        };
        // make a child state
        this.makeKid = function(name, opts) {
          this.kidCount++;
          return state.newState(name, opts);
        };
        // evt should be a "normalized" (camelCase) name
        this.addEventHandler = function(evt, when, dest, run) {
          var prev = events[evt];
          var fn = new GuardedFunction(when, dest, run);
          if (prev) {
            prev.push(fn);
          } else {
            events[evt] = [fn];
          }
        };
      }; // init
    }; // hsmState constructor

    // ctrl api: add a new event handler
    return {
      controller: hsmState,
      transclude: true,
      template: displayStates ? '<div class="hsm-state-label">{{hsmState.active && (!hsmState.autonamed || hsmState.parallel) ?hsmState.label:""}}</div>' : '',
      scope: true,
      controllerAs: "hsmState",
      require: ["^^hsmMachine", "?^^hsmState", "hsmState"],
      link: function(scope, element, attrs, controllers, transcludeFn) {
        var hsmMachine = controllers[0];
        var hsmParent = controllers[1] || hsmMachine;
        var hsmState = controllers[2];
        var srcExp = attrs.hsmState || attrs.name;
        // one-time bindings
        var name = hsmParse.getString(srcExp, scope);
        var parallel = hsmParse.getEvalFunction("parallel", attrs);
        //
        var opt = {
          userEnter: hsmParse.getOption("hsmEnter", scope, attrs),
          userInit: hsmParse.getOption("hsmInit", scope, attrs),
          userExit: hsmParse.getOption("hsmExit", scope, attrs),
          parallel: parallel && parallel(scope)
        };
        hsmState.init(hsmMachine, hsmParent, name, opt);
        transcludeFn(scope, function(clone) {
          element.append(clone);
        });
      }, //link
    };
  })

.directive('hsmMachine',
  function(hsmCause, hsmParse, hsmService, $log, $q, $timeout) {
    'use strict';
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
      this.kidCount = 0;
      this.nameDepth = 0;
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
        queue: true,
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
          return machine.emit(namespace, name, data);
        };
      };
      return new machineScope(this);
    };
    hsmMachine.prototype.start = function() {
      if (this.stage !== stage.registration) {
        throw new Error(["hsmMachine starting, invalid stage:", this.stage].join(" "));
      }
      this.stage = stage.initialized;
      var state;
      try {
        this.machine = this.machine.start(); // overwrite with actual machine.
        this.stage = stage.started;
      } catch (e) {
        $log.error("error starting machine", e);
        this.stage = stage.dead;
      }
    };
    hsmMachine.prototype.emit = function(_namespace, _name, _data) {
      var scoped = !angular.isUndefined(_data);
      var name = scoped ? [_namespace, _name].join("-") : _namespace;
      var data = scoped ? _data : _name;
      var defer = $q.defer();
      var cause = hsmCause.newCause(name, data, defer);

      var rest = this.stage;
      if ((rest !== stage.started) && (rest !== stage.emitting)) {
        defer.reject(rest);
      } else {
        this.stage = stage.emitting;
        this.machine.emit(cause);
        defer.promise.catch(function() {
          rest = stage.dead;
        });
        this.stage = rest;
      }
      return defer.promise;
    };
    // add a substate
    hsmMachine.prototype.addState = function(hsmState, hsmParent, opts) {
      if (this.stage !== stage.registration) {
        throw new Error(["invalid registration", this.stage].join(" "));
      }
      var name = hsmState.name;
      var key = name.toLowerCase();
      if (this.states[key]) {
        throw new Error(["duplicate state", name].join(" "));
      }
      this.states[key] = hsmState;
      return hsmParent.makeKid(name, opts);
    };
    hsmMachine.prototype.makeKid = function(name, opts) {
      this.kidCount++;
      return this.machine.newState(name, opts);
    };
    // returns hsmState
    hsmMachine.prototype.findState = function(name) {
      var key = name.toLowerCase();
      return this.states[key];
    };
    // returns service state
    hsmMachine.prototype.getState = function(name) {
      var ret = this.findState(name);
      return ret && ret.getInstance();
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
        var srcExp = attrs.hsmMachine || attrs.name;
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
