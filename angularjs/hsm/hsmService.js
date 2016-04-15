'use strict';

/** 
 */
angular.module('hsm', [])
  .factory('hsm', function($log) {

    var doNothing = function() {};

    // FIX: currently states must be globally unique
    // 
    var stateByName = {};

    // states are separate from machines
    var State = function(name, parent, enter, exit, init) {
      if (stateByName[name]) {
        throw new Error("state " + name + " already registered");
      }
      stateByName[name] = this;
      this.name = name;
      this.parent = parent;
      this.enter = enter || doNothing;
      this.exit = exit || doNothing;
      this.depth = parent ? parent.depth + 1 : 0;
      this.initial = init;
      this.children = [];
      var par = this;
      this.newState = function(name, enter, exit, init) {
        var child = new State(name, par, enter, exit, init);
        par.children.push(child);
        return child;
      };
    };

    var fini = new State("fini");
    var error = new State("error");

    // opt: onEnter,onExit,etc.
    var Machine = function(name, opt) {
      var current;
      this.name = name;
      var onEnter = opt.onEnter || doNothing;
      var onExit = opt.onExit || doNothing;
      var onInit = opt.onInit || doNothing;
      var onError = opt.onError || doNothing;
      var isExternal = opt.isExternal || doNothing;

      // note: alters current.
      var enter = function(state, cause) {
        // $log.debug("enter", state, onEnter);
        onEnter(state, cause);
        state.enter(cause);
        current = state;
      };
      // note: alters current.
      var exit = function(cause) {
        // $log.debug("exit", current);
        var state = current;
        onExit(state, cause);
        state.exit(cause);
        return current = state.parent;
      };
      var init = function(cause) {
        while (current.children.length) {
          var initial = current.initial || current.children[0];
          var movesToChild = initial.parent === current;
          if (!movesToChild) {
            current = error;
            throw new Error("malformed statechart");
          }
          onInit(current, cause);
          // enter:
          enter(initial, cause);
        }
      };
      var isRunning = this.isRunning = function() {
        return (current !== fini) && (current !== error);
      };

      this.start = function(first) {
        if (!first) {
          throw new Error("expected valid first state for init");
        }
        if (!!current) {
          throw new Error("state machine already started");
        }
        // the first state isnt necessarily a top state, nor a leaf state.
        // walk up, then down.
        var recusiveEnter = function(state) {
          if (state) {
            recusiveEnter(state.parent);
            enter(state);
          }
        };
        recusiveEnter(first);
        // state charts run enter than init; init can move to a new state.
        init();
        // 
        return isRunning();
      };
      var transition = function(source, target, cause) {
        if (!(source instanceof State)) {
          throw new Error("invalid source:" + source);
        }
        if (!(target instanceof State)) {
          throw new Error("invalid target:" + target);
        }
        // first: bring the current state up to the source of the transition.
        while (current !== source) {
          if (!exit(cause)) {
            throw new Error("jumped past top seeking source");
          };
        }
        // check for a self transition ( the source, now current, targeted itself )
        if (current === target) {
          exit(cause);
          enter(target, cause);
        } else {
          var track = target;
          var pathToTarget = [];
          // source deeper than target?
          if (current.depth > track.depth) {
            // exit up to the same level
            do {
              if (!exit(cause)) {
                throw new Error("jumped past top seeking depth");
              }
            } while (current.depth > track.depth);
          } else {
            // target deeper than source?
            // record the path up to the same depth in the tree.
            while (track.depth > current.depth) {
              pathToTarget.push(track);
              var up = track.parent;
              if (!up) {
                $log.error("jumping past top building path", track, pathToTarget);
                throw new Error("jumping past top");
              }
              track = up;
            }
          }

          // if current === track, then souce was an ancestor of target
          // "an internal transition will not exit and re-enter its source state, while an external one will."
          if (current === track) {
            var x = isExternal();
            //$log.info("xter", x);
            if (x) {
              exit(cause); // move current state up
              pathToTarget.push(track); // record that we need to enter it
              track = current; // move track up to the same level.
            }
          } else {
            // now at same depth, but possibly still in different branches of the tree: go up till both sides meet
            do {
              if (!exit(cause)) {
                throw new Error("jumped past top matching source");
              }
              pathToTarget.push(track);
              var up = track.parent;
              if (!up) {
                $log.error("jumping past top matching target ", track, pathToTarget);
                throw new Error("jumping past top");
              }
              track = up;
            } while (current !== track);
          }

          for (var i = pathToTarget.length - 1; i >= 0; --i) {
            var el = pathToTarget[i];
            enter(el, cause);
          }
        }
      };
      // source: state containing the event causing the transition
      // next: destination state
      // cause: optional event info
      this.transition = function(source, next, cause) {
        var okay;
        if (current !== fini && current !== error) {
          if (next === fini || next === error) {
            current = next;
            okay = true;
          } else {
            try {
              // transition, and if all is well, init
              transition(source, next, cause);
              init(cause);
            } catch (e) {
              $log.error(e, current, source, next, cause);
              current = error;
            }
          }
        }
        return okay;
      };
    };

    return {
      newMachine: function(name, o) {
        var opt = o || {};
        return new Machine(name, opt);
      },
      findState: function(name) {
        return stateByName[name];
      },
      newState: function(name, parent, enter, exit, init) {
        return new State(name, parent, enter, exit, init);
      },
    };
  });
