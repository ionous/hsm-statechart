'use strict';

/** 
 
 */
angular.module('hsm')
  .controller('TestController', function($log) {
    $log.info("created test controller");
    this.bloop = function() {
      $log.info("bloop");
    };
  })
  .directive('highlighter', function($interval) {
    return {
      restrict: 'A',
      scope: {
        model: '=highlighter'
      },
      link: function(scope, element) {
        var ready, promise;

        scope.$watch('model', function(nv) {
          // apply class
          ready = false;
          if (!promise) {
            element.addClass('highlight');
            promise = $interval(function() {
              if (!ready) {
                ready = true;
              } else {
                element.removeClass('highlight');
                $interval.cancel(promise);
                promise = false;
              }
            }, 600);
          }
        });
      }
    };
  })

.controller('SamekPlus',
  function(hsm, $log, $scope) {
    var plus = this;
    var tests; // set at end.
    plus.value = 0;
    var Test = function(src) {
      var seq = src.slice();
      this.next = function(l) {
        var c = seq.shift();
        if (c != l) {
          var err = "test failed";
          $log.error(err, "expected", c, "got", l);
          throw new Error(err);
        }
      };
      this.run = function(evt) {
        $scope.hsmMachine.emit(evt);
        if (seq.length) {
          var err = "test incomplete.";
          $log.error(err, evt, "remaining", seq);
          throw new Error(err);
        }
      };
    };

    plus.test = function(state) {
      $log.info("testing...");
      tests.forEach(function(t, index) {
        $log.warn("test", index, t.evt, t.seq || "unhandled event");
        var evt = t.evt;
        var val = t.val;
        var test = $scope.logger.test = new Test(t.seq || []);
        $scope.logger.test = test.run(evt);

      });
      $log.info("tests succeeded!");
    }
    plus.clearValue = function(next) {
      $log.warn("clearing value", plus.value);
      if (plus.value) {
        $log.warn("clearing value");
        plus.value = 0;
        return next;
      }
    };
    plus.setValue = function(next) {
      $log.warn("setting value", plus.value);
      if (!plus.value) {
        plus.value = 1;
        return next;
      }
    };
    plus.clicked = function(evt) {
      $log.info("clicked", evt);
      $scope.hsmMachine.emit(evt);
    };

    // expected startup sequence:
    // currently, not part of the test:
    //  "s0-ENTRY","s0-INIT","s1-ENTRY","s1-INIT","s11-ENTRY",
    tests = [
      /* test 0 */
      {
        evt: "a",
        seq: ["s11-EXIT", "s1-EXIT",
          "s1-ENTRY", "s1-INIT", "s11-ENTRY"
        ],
        val: 0
      },
      /* test 1: * s0 handles 'e' and directs entry down to s211 */
      {
        evt: "e",
        seq: ["s11-EXIT", "s1-EXIT",
          /*"s0-EXIT", "s0-ENTRY", hsm-statechart, doesn't exit the source state, unless the source state target itself.*/
          "s2-ENTRY", "s21-ENTRY", "s211-ENTRY"
        ],
        val: 0
      },
      /* test 2: s0 handles 'e' and directs entry down to s211 again */
      {
        evt: "e",
        seq: ["s211-EXIT", "s21-EXIT", "s2-EXIT",
          /*"s0-EXIT",
                      "s0-ENTRY",*/
          "s2-ENTRY", "s21-ENTRY", "s211-ENTRY"
        ],
        val: 0
      },
      /* test3: unhandled */
      {
        evt: "a",
        // seq: ["EVT-a"],
        val: 0
      },
      /* test 4: s21 handles 'h', f==0, guard passes, sets foo, self-transition to s21, inits down to s211 */
      {
        evt: "h",
        seq: ["s211-EXIT", "s21-EXIT",
          "s21-ENTRY", "s21-INIT", "s211-ENTRY"
        ],
        val: 1
      },
      /* test 5: s1 hears 'h', foo==1, guard filters, 'h' is unhandled */
      {
        evt: "h",
        // seq: ["EVT-h"],
        val: 1
      },
      /* test 6: s211 handles 'g', directs to 's0', inits down to s11. */
      {
        evt: "g",
        seq: ["s211-EXIT", "s21-EXIT", "s2-EXIT",
          /*"s0-EXIT",
                      "s0-ENTRY",*/
          /* a little odd, but since we haven't exited s0, and it's not a leaf state, 
          we get an init without an immediately preceeding enter */
          "s0-INIT", "s1-ENTRY", "s1-INIT", "s11-ENTRY"
        ],
        val: 1
      },

      /* test 7: s11 handles 'h', clears foo */
      {
        evt: "h",
        // seq: ["EVT-h"],
        val: 0
      },
      /* test 8: s0 handles 'i', directs down to 's12' */
      {
        evt: "i",
        seq: ["s11-EXIT", "s1-EXIT",
          /*"s0-EXIT",
                      "s0-ENTRY",*/
          "s1-ENTRY", "s12-ENTRY"
        ],
      },
      /* test 9: x isn't handled anywhere */
      {
        evt: "x",
        // seq: ["EVT-x"],
        val: 0
      },
    ];
  });
