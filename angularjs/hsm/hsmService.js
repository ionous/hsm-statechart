'use strict';

/** 
 * port of https://github.com/ionous/hsm-statechart/blob/master/hsm/hsm_machine.c
 * fix: consider removing angular dependency in favor of something like requirejs...
 */
angular.module('hsm', [])

.factory('hsmService', function($log) {
  //-----------------------------------------------
  // empty helper function.
  var doNothing = function() {};

  //-----------------------------------------------
  // validate the passed object is indeed a state.
  var checkState = function(state, message) {
    if (!(state instanceof State)) {
      var msg = message || "state invalid";
      $log.error(msg, state);
      throw new Error(msg || "state invalid");
    }
  };

  //-----------------------------------------------
  // pairs target state and transition actions.
  var TargetActions = function(src, tgt, actions) {
    this.src = src;
    this.tgt = tgt;
    this.actions = actions || doNothing;
  };

  //-----------------------------------------------
  // onEvent helper for designating state transitions and transition actions.
  var EventSink = function() {
    var tgt, acts;
    this.newHandler = function() {
      return {
        goto: function(target) {
          tgt = target;
          return {
            run: function(actions) {
              acts = actions;
            }
          };
        }
      };
    };
    this.targetActions = function(src) {
      var ret;
      if (tgt) {
        ret = new TargetActions(src, tgt, acts);
        tgt = acts = null;
      }
      return ret;
    };
  };
  // we're single threaded, only need one reusable sink.
  var eventSink = new EventSink();

  //-----------------------------------------------
  var copyCallbacks = function(f, opt) {
    f.onEnter = opt.onEnter || doNothing;
    f.onInit = opt.onInit || doNothing;
    f.onEvent = opt.onEvent || doNothing;
    f.onExit = opt.onExit || doNothing;
  };

  //-----------------------------------------------
  // NOTE: state are considered read-only once created.
  var State = function(name, parent, depth, opt) {
    this.name = name;
    this.parent = parent;
    this.depth = depth;
    this.children = [];
    this.parallel = opt.parallel;
    // extend event callbacks?
    copyCallbacks(this, opt);
  };
  // returns true if this is the end of a region chain
  State.prototype.terminal = function() {
    return this.parallel;
  };
  // no return value
  State.prototype.enter = function(cause) {
    this.onEnter(this, cause);
  };
  // returns desired child state
  State.prototype.init = function(cause) {
    var next;
    if (this.children.length) {
      next = this.onInit(this, cause) || this.children[0];
    }
    return next;
  };
  // returns null or TargetActions
  State.prototype.signal = function(cause) {
    this.onEvent(this, cause, eventSink.newHandler());
    return eventSink.targetActions(this);
  };
  // returns parent state (could be null)
  State.prototype.exit = function(cause) {
    this.onExit(this, cause);
    return this.parent;
  };

  //-----------------------------------------------
  // active region:
  // the head state is not parallel, but the leaf state may be.
  var Region = function(state) {
    this.leafState = state || null;
    this.leafSet = null;
  };
  // exit all states withing this region; after:
  // our region's leafState and leafSet will be null.
  Region.prototype.exitRegion = function(ctx) {
    return this.matchedExit(ctx, function(leaf) {
      return leaf == null;
    });
  };
  // exits until the region's leaf state matches the passed state
  // ( doesn't exit the passed state )
  Region.prototype.exitUntil = function(ctx, state) {
    return this.matchedExit(ctx, function(leaf) {
      return leaf === state;
    });
  };
  // exit within this region only, stopping when the match function returns true.
  // match receives the current leaf state; which can be null.
  Region.prototype.matchedExit = function(ctx, match) {
    var matched = false;
    var leaf = this.leafState;
    if (match(leaf)) {
      matched = true;
    } else if (leaf) {
      //this.exitSet(ctx);
      var set = this.leafSet;
      if (set) {
        set.exitSet(ctx);
        this.leafSet = null;
      };
      do {
        var parent = ctx.exitState(leaf);
        leaf = (parent && !parent.terminal()) ? parent : null;
        matched = match(leaf);
      }
      while (leaf && !matched);
      this.leafState = leaf;
    }
    return matched;
  };

  //-----------------------------------------------
  // the container state is parallel
  // there is a region for every state in the container state..
  var RegionSet = function() {
    this.regions;
  };
  // exit all regions contained by this set.
  RegionSet.prototype.exitSet = function(ctx) {
    this.regions.forEach(function(region) {
      region.exitRegion(ctx);
    });
    this.regions = null;
  };

  //-----------------------------------------------
  // we need to find the lca of source and target
  var Xfer = function(ctx, sig) {
    this.ctx = ctx;
    this.src = sig.src;
    this.tgt = this.track = sig.tgt;
    this.actions = sig.actions;
    this.path = [];
  };
  // private helper for tracking re-entry path
  Xfer.prototype._addToPath = function(state) {
    this.path.push(state);
    return state.parent;
  };
  // transition at or below the passed src ( if we're able ).
  // alters the passed region.
  Xfer.prototype.innerExit = function(region, src) {
    var src = this.src;
    var selfTransition = src === this.tgt;
    var exitUntil;
    if (!selfTransition) {
      // exit to the point of the event listener
      exitUntil = src;
      // bring up the our target to the same depth as the listener
      // ( we will shortly be seeking lowest common ancestor ) 
      this.track = this._rollUp(src);
    } else {
      // on selfTransion, we want to to exit the src state -- so exit until its parent.
      var parent = src.parent;
      // but, if the src state is a local root then we need to pass a parent of null. 
      exitUntil = (parent && !parent.terminal()) ? parent : null;
      // save the self-exit for later re-entry.
      this.track = this._addToPath(src);
    }
    // exit within this region to reach the desired state:
    if (!region.exitUntil(this.ctx, exitUntil)) {
      if (selfTransition) {
        throw new Error("selfTransition failed");
      }
    }
    return selfTransition || this.finishExit(region);
  };
  // move the target up to src, recording the re-entry path as we go.
  Xfer.prototype._rollUp = function(src) {
    var goal = src.depth;
    var track = this.track;
    for (var i = track.depth; i > goal; i -= 1) {
      track = this._addToPath(track);
    }
    return track;
  };
  // try to complete the transition, altering the passed region.
  // if the transition couldn't complete ( needs work higher in the tree )
  // returns false ( with region.leaf == null )
  Xfer.prototype.finishExit = function(region) {
    var ok = this._exitUp(region) && this._matchUp(region);
    if (!ok && region.leafState != null) {
      throw new Error("expected fully exited region");
    }
    return ok;
  };
  // exit the region's edge until it's at target depth ( returns true )
  // or until we've left the region ( returns false, region.leaf becomes null )
  Xfer.prototype._exitUp = function(region) {
    var ctx = this.ctx;
    var depth = this.track.depth;
    return region.matchedExit(ctx, function(leaf) {
      return leaf && (leaf.depth == depth);
    });
  };
  // given that the tracking state and the edge state are at equal depths:
  // move both tracking and edge up until they are the same node.
  Xfer.prototype._matchUp = function(region) {
    var xfer = this; // uses Xfer.exitState
    return region.matchedExit(xfer, function(leaf) {
      return leaf === xfer.track;
    });
  };
  // duck-type context pair state exits to track exits.
  Xfer.prototype.exitState = function(state) {
    this.track = this._addToPath(this.track);
    return this.ctx.exitState(state);
  };

  //-----------------------------------------------
  var Entry = function(region, xf) {
    this.region = region;
    this.xf = xf;
  };

  //-----------------------------------------------
  var Reenters = function() {
    this.transitions = null;
  };

  //
  Reenters.prototype.resetEnters = function() {
    this.transitions = null;
  };
  //
  Reenters.prototype.addEnter = function(region, xf) {
    var transitions = this.transitions;
    var reentry = new Entry(region, xf);
    if (!transitions) {
      this.transitions = [reentry];
    } else {
      transitions.push(reentry);
    }
  };
  Reenters.prototype.finish = function(region, xf) {
    return xf ? [new Entry(region, xf)] : this.transitions;
  };

  //-----------------------------------------------
  var Context = function(calls, cause) {
    this.calls = calls;
    this.cause = cause;
    this.reentry = new Reenters();
  };
  //
  Context.prototype.enterState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    ctx.calls.onEnter(state, cause);
    state.enter(cause);
  };
  //
  Context.prototype.initState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    var res = state.init(cause);
    if (res) {
      ctx.calls.onInit(state, cause, res);
    }
    return res;
  };
  //
  Context.prototype.signalState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    var res = state.signal(cause);
    ctx.calls.onEvent(state, cause, res);
    return res;
  };
  //
  Context.prototype.exitState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    var res = state.exit(cause);
    ctx.calls.onExit(state, cause, res);
    return res;
  };
  //
  Context.prototype.followPath = function(region, path) {
    var ctx = this;
    var prime = region.leafState;
    if (!prime) {
      prime = path.pop();
      ctx.enterState(prime);
    }
    region.leafState = ctx.recursivePath(region, prime, path);
  };
  // returns the lowest child of the region.
  Context.prototype.recursivePath = function(region, entered, path) {
    var ctx = this;
    var ret = entered;
    // if we have a path, then enter than state: 
    // otherwise init the current state and enter it.
    var next = path.pop() || ctx.initState(entered);
    if (!entered.parallel) {
      if (next) {
        ctx.enterState(next);
        ret = ctx.recursivePath(region, next, path);
      }
    } else {
      var set = region.leafSet = new RegionSet();
      set.regions = entered.children.map(function(child) {
        var sub = new Region(child);
        ctx.enterState(child);
        sub.leafState = ctx.recursivePath(sub, child, (child === next) ? path : []);
        return sub;
      });
    }
    return ret;
  };
  // recursive emit into region and its leaves
  Context.prototype.remit = function(region) {
    var ret, ctx = this;
    var pendingExit = ctx.emitToLeaves(region.leafState, region.leafSet);

    // try completing the transfer within this region; 
    // if not, this region gets exited inside finishExit.
    if (pendingExit && !pendingExit.finishExit(region)) {
      // since we didnt complete in this region, we don't want to signal this region.
      ret = pendingExit;
    } else {
      // signal the remaining states in the region.
      // QUESTION:  in cases where target was an ancestor of source, 
      // the target can still be active (re: internal transitions)
      // so the target might get to handle this event as well. 
      // is that reasonable? 
      var sig = ctx.signalRegion(region);
      if (!sig && pendingExit) {
        ctx.reentry.addEnter(region, pendingExit);
      } else if (sig) {
        // we are transitioning. 
        // regardless whether we complete this new transition,
        // our src is above our previous leaf/set; 
        // ours transition subsumes and supplants any previous transition.
        var xf = new Xfer(ctx, sig);
        if (xf.innerExit(region)) {
          ctx.reentry.addEnter(region, xf);
        } else {
          ret = xf;
        }
      }
    }
    return ret;
  }; // remit
  //
  Context.prototype.emitToLeaves = function(leaf, leafSet) {
    var pendingExit, ctx = this;
    if (leafSet) {
      var regions = leafSet.regions;
      for (var i = 0; i < regions.length; i += 1) {
        var sub = regions[i];
        var xf = ctx.remit(sub);
        if (xf) {
          // target is as high, or higher than the leaf containing the regions.
          // if the target was deeper, then the lca would deeper than the leaf;
          // the transition would have been handled *inside* in sub-region remit.
          if (xf.tgt.depth <= leaf.depth) {
            throw new Error("expected target at or above leaf");
          }
          // hsm uses internal transitions except for self-transitions, therefore:
          // if the target is the container, we shouldn't interrupt the siblings.
          // note: the leaf itself can't be the *source* of the transit --
          // that's handled outside of the loop, *after* all its child regions,
          // ie. this can't be a self-transition.
          if (xf.tgt === leaf) { // test equality, not just depth.
            ctx.reentry.addEnter(sub, xf);
          } else {
            // im not explicitly exiting the leaf set -
            // theoretically that happens magically inside of remit's finishExit
            // as a result of "finishExit"
            ctx.reentry.resetEnters();
            pendingExit = xf;
            break;
          }
        }
      }
    }
    return pendingExit;
  };
  // emit the current event to the region.
  // returns a request for a new transition which includes
  // whichever state requested the transition ( the source(src) ); 
  // and some target(tgt) state anywhere in the tree ( possibly even the source again )
  Context.prototype.signalRegion = function(region) {
    var ctx = this;
    var ret, curr = region.leafState;
    if (curr) {
      do {
        var targetActions = ctx.signalState(curr);
        // are we asking for a new state?
        if (targetActions) {
          ret = targetActions;
          break;
        }
        // lets keep walking up.
        curr = curr.parent;
      } while (curr && !curr.terminal());
    }
    return ret;
  };

  //-----------------------------------------------
  // opt: onEnter, onExit, etc. callbacks
  var Machine = function(name, opt) {
    this.name = name;
    this.children = [];
    this.region = null;
    this.queuing = null;
    var c = this.callbacks = {}
    copyCallbacks(c, opt, true);
  };

  Machine.prototype.start = function(dst) {
    checkState(dst, "expected valid first state");
    if (this.region) {
      var msg = "machine already running";
      $log.error(msg, name);
      throw new Error(msg);
    }
    // path to enter:
    var tgtPath = [];
    do {
      tgtPath.push(dst);
      dst = dst.parent;
    } while (dst);
    // enter that path, note: it will also init all the way down.
    var region = this.region = new Region();
    var ctx = new Context(this.callbacks);
    ctx.followPath(region, tgtPath);
  };

  Machine.prototype.emit = function(cause) {
    if (this.queuing) {
      this.queuing.push(cause);
    } else {
      var queue = this.queuing = [];
      queue.push(cause);
      //
      var ctx = new Context(this.callbacks, cause);
      var region = this.region;
      //
      while (queue.length) {
        var q = queue.shift();
        var xf = ctx.remit(region);
        var transitions = ctx.reentry.finish(region, xf);
        if (!transitions) {
          // unhandled event:
          this.callbacks.onEvent(null, cause);
        } else {
          // FIX: catch competing re-entries
          // (ex. a state tree A/B,C where one transition says enter B the other, C.
          transitions.forEach(function(e) {
            var region = e.region;
            var xf = e.xf;
            xf.actions(); // catch and throw with more info?
            ctx.followPath(region, xf.path);
          });
        }
      }
      this.queuing = null;
    }
  };

  //-----------------------------------------------
  var StateBuilder = function(machine, parent, children, depth) {
    this.newState = function(name, opt) {
      var state = new State(name, parent, depth, opt || {});
      children.push(state);
      return new StateBuilder(machine, state, state.children, depth + 1);
    };
    this.state = function() {
      return parent;
    };
    // FIX? better would be if finalize walked builder tree
    // it would be nice, for instance, instead of passing opt:
    // to build opt via functions.
    this.finalize = function() {
      var m = machine;
      machine = null;
      return m;
    };
    this.start = function(state) {
      var m = machine;
      machine = null;
      m.start(state || children[0]);
      return m;
    }
  };

  //-----------------------------------------------
  // hsm service:
  return {
    newMachine: function(name, opt) {
      var machine = new Machine(name, opt || {});
      return new StateBuilder(machine, null, machine.children, 0);
    },
  };
});
