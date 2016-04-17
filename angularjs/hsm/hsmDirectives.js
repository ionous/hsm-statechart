'use strict';

// turn html attributes into helper functions
function parse($parse, scope, attrs) {
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

// FIX: add onEnter, etc. to the states themselves.
// FIX: add a real end-to-end test
// FIX: find a way to allow "goto state" with a simple name, while still allowing states to be rused.
// ( maybe a path? maybe a parameterization? )
angular.module('hsm')
  .directive('hsmMachine', function(hsm, $log, $parse) {
    return {
      restrict: 'E',
      transclude: true,
      template: '<div></div>',
      link: function(scope, element, attrs, directiveCtrl, transcludeFn) {
        var newScope = scope.$new();
        var name = attrs['id'] || "root";

        var opt = parse($parse, newScope, attrs);
        var machine = hsm.newMachine(name, opt);
        var rt = hsm.newState(name);

        newScope.hsm = {
          machine: machine,
          state: rt,
        };
        transcludeFn(newScope, function(clone) {
          element.append(clone);
        });
        machine.start(rt);
      }
    };
  })
  .directive('hsmState', function(hsm, $log, $parse) {
    return {
      restrict: 'E',
      transclude: true,
      template: '<div></div>',
      // scope: { because of transclusion, this works poorly
      //   on: "<", // this gets the parsed on, but not hsm
      //   hsm: "=",
      // },
      link: function(scope, element, attrs, controller, transcludeFn) {
        var newScope = scope.$new();
        var machine = scope.hsm.machine;
        var parent = scope.hsm.state;

        // note: html is case insenstive, and angular camelizes:
        // so using attributes directly
        // ( as opposed to a single hsm-events={} )
        // means user events need to do the same.
        var events = {};
        for (var k in attrs) {
          if (/on[A-Z]/.test(k)) {
            var n = k.charAt(2).toLowerCase() + k.slice(3);
            var v = attrs[k];
            events[n] = $parse(v);
          }
        }
        var name = attrs['id'];
        var parallel= attrs['hsmParallel'];
        // would rather use real angular components
        // havent figured out how to do that with transclusion.
        var ctrl = newScope.$ctrl = {
          name: name,
          active: false,
        };

        var myState;
        var listeners = [];
        var enter = function() {
          ctrl.active = true;
          // would love to verify the states in advance
          // will take a bit of work, because states reference eachother in arbitrary order.
          var keys = Object.keys(events);
          listeners = keys.map(function(evt) {
            return scope.$on(evt, function() {
              var dest = events[evt](newScope);
              if (dest) {
                var next = angular.isString(dest) ?
                  hsm.findState(dest) : dest;
                machine.changeStates(myState, next, evt);
              }
            });
          });
        };
        var exit = function() {
          ctrl.active = false;
          listeners = listeners.filter(function(rub) {
            rub();
          });
        };

        myState = parent.newState(name, {
          onEnter: enter,
          onExit: exit,
          parallel: parallel
        });
        newScope.hsm = {
          machine: machine,
          state: myState,
        };

        transcludeFn(newScope, function(clone) {
          element.append(clone);
        });
      }
    }
  });
