/** 
 * port of https://github.com/ionous/hsm-statechart/blob/master/hsm/hsm_machine.c
 * fix: consider removing angular dependency in favor of something like requirejs...
 */
angular.module('hsm', [])

.factory('hsmService', function($log) {
  'use strict';


  //-----------------------------------------------
  // empty helper function.
  var doNothing = function() {};

  //-----------------------------------------------
  // validate the passed object is indeed a state.
  var checkState = function(state, message) {
    if (!(state instanceof State)) {
      throw new Error(message || "state invalid");
    }
  };

  //-----------------------------------------------
  // pairs target state and transition actions.
  // external transitions are only partially supported right now.
  // ( they work for self transitions )
  var TargetActions = function(src, tgt, actions, external) {
    this.src = src;
    this.tgt = tgt;
    this.actions = actions;
    this.external = external;
  };
  TargetActions.prototype.isSelfTransition = function() {
    return this.src === this.tgt;
  };

  //-----------------------------------------------
  // onEvent helper for designating state transitions and transition actions.
  var EventSink = function() {
    var tgt, act, ext;
    this.newHandler = function() {
      return {
        // defaults internal transition, except for self transitions
        goto: function(target, external) {
          tgt = target;
          ext = external;
          return {
            run: function(action) {
              act = action;
            }
          };
        }
      };
    };
    this.targetActions = function(src) {
      var ret;
      if (tgt) {
        var ex = !angular.isUndefined(ext) ? ext : src === tgt;
        ret = new TargetActions(src, tgt, act, ex);
      }
      // reset state:
      tgt = act = null;
      ext = undefined;
      return ret;
    };
  };
  // we're single threaded, only need one reusable sink.
  var eventSink = new EventSink();

  //-----------------------------------------------
  var copyCallbacks = function(f, opt, extra) {
    f.onEnter = opt.onEnter || doNothing;
    f.onEvent = opt.onEvent || doNothing;
    f.onInit = opt.onInit || doNothing;
    f.onExit = opt.onExit || doNothing;

    if (extra) {
      f.onTransition = opt.onTransition || doNothing;
    }
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
    var handler = eventSink.newHandler();
    this.onEvent(this, cause, handler);
    return eventSink.targetActions(this);
  };
  // returns parent state (could be null)
  State.prototype.exit = function(cause) {
    this.onExit(this, cause);
    return this.parent;
  };

  //-----------------------------------------------
  // an active region.
  // there is at least one, but possibly more than one active in a machine.
  var Region = function(state) {
    // the head state of a region is not parallel, but the leaf state may be.
    this.leafState = state || null;
    // if the leafState(State) is parallel, then the leafSet(RegionSet) exists.
    this.leafSet = null;
  };
  // exit all states within this region; after:
  // our region's leafState and leafSet will be null.
  Region.prototype.exitRegion = function(killer) {
    return this.evalExit(killer, function(leaf) {
      return leaf === null;
    });
  };
  // exits until the region's leaf state matches the passed state
  // ( doesn't exit the passed state )
  Region.prototype.exitUntil = function(killer, state) {
    return this.evalExit(killer, function(leaf) {
      return leaf === state;
    });
  };
  // exit within this region only, stopping when the match function returns true.
  // match receives the current leaf state; which can be null.
  // killer implements the "interface" exitState
  Region.prototype.evalExit = function(killer, match) {
    var matched = false;
    var leaf = this.leafState;
    if (match(leaf)) {
      matched = true;
    } else if (leaf) {
      // exiting the leaf, so exit all of the child regions
      var set = this.leafSet;
      if (set) {
        killer.exitSet(set);
        this.leafSet = null;
      }
      do {
        var parent = killer.exitState(leaf);
        leaf = (parent && !parent.terminal()) ? parent : null;
        matched = !!match(leaf);
      }
      while (leaf && !matched);
      this.leafState = leaf;
    }
    return matched;
  };

  //-----------------------------------------------
  // the container state is parallel
  // there is a region for every state in the container state.
  var RegionSet = function() {
    this.regions = null;
  };
  // exit all regions contained by this set.
  RegionSet.prototype.exitSet = function(killer) {
    this.regions.forEach(function(region) {
      region.exitRegion(killer);
    });
    this.regions = null;
  };

  //-----------------------------------------------
  // we need to find the lca of source and target
  // .lca valid with .finished
  var Xfer = function(ctx, region, sig) {
    this.ctx = ctx;
    this.region = region; // region of source.
    this.src = sig.src; // source of transition request ( aka. the event handler )
    this.tgt = this.lca = sig.tgt; // lca changes, tgt doesnt.
    this.actions = sig.actions; // pending actions
    this.path = []; // re-entry path, built while finding lca 
    this.finished = false; // lca is valid when true
    this.next = null; // linked list of sibling/inner transitions
  };
  //-----------------------------------------------
  // a child's inner exit requires a reentry somewhere in its region.
  Xfer.prototype.addEnter = function(newFirst) {
    newFirst.next = this;
    return newFirst;
  };
  // private helper for tracking re-entry path
  Xfer.prototype._addToPath = function(state) {
    this.path.push(state);
    return state.parent;
  };
  // transition within or below the current region. altering the region.
  Xfer.prototype.innerExit = function() {
    var ctx = this.ctx;
    var region = this.region;
    var src = this.src;
    // bring up the pending lca to the same depth as the listener
    this.lca = this._rollUp(src);
    // when our target is in a child region below the leaf, then,
    // due to the rules of inner transition, our leaf wont actually exit. 
    // without the leaf exit, the set wont exit --
    // yet, the descendant is still going to get re/entered.
    // it its active already, it will have two enters and no exits.
    // a solution is to exit the leaf manually if needed.
    // this means any signal handled in a region kills its leaf set.
    if (region.leafSet) {
      region.leafSet.exitSet(ctx);
      region.leafSet = null;
    }
    // exit to the point of the event listener
    if (!region.exitUntil(ctx, src)) {
      throw new Error("exit to source failed");
    }
    // begin seeking the lowest common ancestor
    return this.finishExit(region);
  };
  // self-transition within the current region. altering the region.
  Xfer.prototype.selfTransition = function(internal) {
    var ctx = this.ctx;
    var region = this.region;
    var exitUntil;
    if (internal) {
      exitUntil = this.src;
      // see note in innerExit; maybe we could share code
      if (region.leafSet) {
        region.leafSet.exitSet(ctx);
        region.leafSet = null;
      }
    } else {
      // save the self-exit for later re-entry.
      this.lca = this._addToPath(this.src);
      // on selfTransion, exit the src state; ie. up to its parent.
      var parent = this.src.parent;
      // but: if the src is a local root, we need to pass a parent of null. 
      exitUntil = (parent && !parent.terminal()) ? parent : null;
    }
    // should always succeed
    if (!region.exitUntil(ctx, exitUntil)) {
      throw new Error("self transition failed");
    }
    // since we arent calling finish exit, update the status ourselves
    this.finished = true;
  };
  // move the target edge up to the depth of src, 
  // recording the re-entry path as we go.
  Xfer.prototype._rollUp = function(src) {
    var goal = src.depth;
    var track = this.lca;
    for (var i = track.depth; i > goal; i -= 1) {
      track = this._addToPath(track);
    }
    return track;
  };
  // try to complete the transition, altering the passed region.
  // if the transition couldn't complete ( needs work higher in the tree )
  // returns false ( with region.leaf == null )
  Xfer.prototype.finishExit = function(region) {
    if (!this.finished) {
      var ok = this._exitUp(region) && this._matchUp(region);
      if (ok) {
        this.finished = true;
        // PATCH: 6/28/16: see Fini note as well.
        // we want to update the xfer region,
        // otherwise when we run the entry transition
        // we will be pointing to some other region.
        // NOTE: 8/29/16: when we exit into a state containing regions
        // we want to exit those regions; this code is littered in a few places
        // it makes me think this should be handled in one place elsewhere....
        if (region.leafSet) {
          region.leafSet.exitSet(this.ctx);
          region.leafSet = null;
        }
        this.region = region;
      } else {
        if (region.leafState !== null) {
          throw new Error("expected fully finished region");
        }
      }
    }
    return this.finished;
  };
  // exit the region's edge until it's at target depth ( returns true )
  // or until we've left the region ( returns false, region.leaf becomes null )
  Xfer.prototype._exitUp = function(region) {
    var ctx = this.ctx;
    var depth = this.lca.depth;
    return region.evalExit(ctx, function(leaf) {
      return leaf && (leaf.depth === depth);
    });
  };
  // given that the tracking state and the edge state are at equal depths:
  // move both tracking and edge up until they are the same node.
  Xfer.prototype._matchUp = function(region) {
    var xfer = this; // uses Xfer.exitState
    return region.evalExit(xfer, function(leaf) {
      return leaf === xfer.lca;
    });
  };
  // implement the state killer interface to track state exits
  Xfer.prototype.exitState = function(state) {
    this.lca = this._addToPath(this.lca);
    return this.ctx.exitState(state);
  };
  // reflect set exit back to the context so that we dont track those exits
  Xfer.prototype.exitSet = function(set) {
    return this.ctx.exitSet(set);
  };

  //-----------------------------------------------
  var Context = function(calls, cause) {
    this.calls = calls;
    this.cause = cause;
  };
  Context.prototype.enterState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    ctx.calls.onEnter(state, cause);
    state.enter(cause);
  };
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
  Context.prototype.signalState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    var res = state.signal(cause);
    ctx.calls.onEvent(state, cause, res);
    return res;
  };
  Context.prototype.exitState = function(state) {
    checkState(state);
    var ctx = this;
    var cause = ctx.cause;
    var res = state.exit(cause);
    ctx.calls.onExit(state, cause, res);
    return res;
  };
  // this ugly thunk helps implement the "killer" interface for evalExit
  Context.prototype.exitSet = function(set) {
    return set.exitSet(this);
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
      if (!entered.children || !entered.children.length) {
        $log.warn("creating leaf set", region, path, "but it has no children");
      }
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
    var ctx = this;
    // signal the children: calls back to remit.
    var xf = ctx.captureLeaves(region.leafState, region.leafSet);
    // try completing any pending transfer within this region;
    // if not, we'll try again at a higher level.
    if (xf && !xf.finished) {
      xf.finishExit(region);
    } else {
      // since no transition reached us,
      // attempt to handle the signal within this region.
      var sig = ctx.bubbleRegion(region);
      if (sig) {
        // we are transitioning; this overrides all child transitions. 
        xf = new Xfer(ctx, region, sig);
        if (sig.isSelfTransition()) {
          xf.selfTransition(!sig.external);
        } else {
          xf.innerExit();
        }
      }
    }
    return xf;
  }; // remit
  //
  Context.prototype.captureLeaves = function(leaf, leafSet) {
    var ret, ctx = this;
    if (leafSet) {
      var regions = leafSet.regions;
      // note: we shouldnt be able to have null regions with a leaf set,
      // it does happen... somehow.... maybe due to thrown errors in creation
      if (!regions) {
        $log.warn("avoiding leafSet with null regions", leaf, leafSet);
        leafSet.regions = [];
      } else {
        for (var i = 0; i < regions.length; i += 1) {
          var sub = regions[i];
          var xf = ctx.remit(sub);
          if (xf) {
            // target is as high, or higher than the leaf containing the regions.
            // if the target was deeper, then the lca would deeper than the leaf;
            // the transition would have been handled *inside* in sub-region remit.
            //
            // PATCH: 6/28/16 - the test seems reversed from the comment --
            // and there's a set of states in the game which trigger this.
            // Fini is a middle child of GameStarted(parallel),
            // its transitioning to a high, direct, non-root, non-leaf state with a nil hsm-init.
            //
            // if (xf.tgt.depth < leaf.depth) {
            //   throw new Error("expected target at or above leaf");
            // }
            // hsm uses internal transitions except for self-transitions:
            // if the target is the container, but the source is below it -- 
            // we shouldn't interrupt the siblings.
            if (xf.finished || (xf.tgt === leaf)) {
              xf.finished = true; // consider it finished.
              ret = ret ? ret.addEnter(xf) : xf;
            } else {
              // abandons sibling transitions, 
              // we will be exiting this set soon.
              ret = xf;
              break;
            }
          }
        }
      }
    }
    return ret;
  };
  // emit the current event to the region.
  // returns a request for a new transition.
  Context.prototype.bubbleRegion = function(region) {
    var ctx = this;
    var ret, curr = region.leafState;
    if (curr) {
      do {
        var sig = ctx.signalState(curr);
        if (sig) {
          ret = sig;
          break;
        }
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
    this.emitting = null;
    this.useQueue = opt.queue;
    var c = this.callbacks = {};
    copyCallbacks(c, opt, true);
  };

  Machine.prototype.start = function(dst) {
    checkState(dst, "expected valid first state");
    if (this.region) {
      throw new Error("already running");
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
    if (this.emitting) {
      if (this.useQueue) {
        this.emitting.push(cause);
      } else {
        cause.reject("already emitting");
      }
    } else {
      if (!this.useQueue) {
        this.emitting = cause;
        this.emitone(cause);
      } else {
        var queue = this.emitting = [];
        var q = cause;
        do {
          this.emitone(q);
          q = queue.shift();
        } while (q);
      }
      this.emitting = null;
    }
  };
  Machine.prototype.emitone = function(cause) {
    try {
      this.emitoneUnsafe(cause);
      cause.resolve(cause);
    } catch (e) {
      $log.error(e);
      cause.reject(e);
    }
  };
  Machine.prototype.emitoneUnsafe = function(cause) {
    var ctx = new Context(this.callbacks, cause);
    var xs = ctx.remit(this.region);
    if (!xs) {
      // unhandled event:
      this.callbacks.onEvent(null, cause);
    } else {
      // need processing order, but have the reverse.
      var transitions = []; // could resize based on number of addEnter(s)
      do {
        transitions.push(xs);
        xs = xs.next;
      } while (xs);
      // 
      do {
        var xf = transitions.pop();
        xf.next = null; // help gc.
        this.callbacks.onTransition(xf.src, cause, xf.tgt);

        var act = xf.actions; // catch and throw with more info?
        if (act) {
          act();
        }
        // FIX: catch competing re-entries
        // (ex. parallel tree A-B,A-C: one xf wants B; the other, C.
        // note; path can be empty when the target is a direct ancestor of src.
        ctx.followPath(xf.region, xf.path);
      } while (transitions.length);
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
    };
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
