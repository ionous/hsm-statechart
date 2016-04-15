'use strict';

/** 
 
 */
angular.module('hsm')
  .controller('SamekPlus',
    function(hsm, $log, $scope) {
      var test; // set at end.
      var data= $scope.data = {
        value: 0
      };
      var rec;
      $scope.enter = function(state) {
        var l = state.name + "-ENTRY";
        if (rec) {
          rec.push(l);
        } else {
          $log.info(l);
        }
      }
      $scope.exit = function(state) {
        var l = state.name + "-EXIT";
        if (rec) {
          rec.push(l);
        } else {
          $log.info(l);
        }
      }
      $scope.init = function(state) {
        var l = state.name + "-INIT";
        if (rec) {
          rec.push(l);
        } else {
          $log.info(l);
        }
      }
      $scope.test = function(state) {
        $log.info("testing...");
        test.forEach(function(t, index) {
          $log.info("test", index, t.evt, t.seq || "unhandled event");
          rec = [];
          var evt = t.evt;
          var seq = t.seq || [];
          var val = t.val;
          $scope.$broadcast(evt);
          var mismatched;
          if (rec.length != seq.length) {
            var msg = "test " + index + " failed.";
            $log.error(msg, evt, "recorded:", rec);
            throw new Error(msg);
          } else {
            for (var i = 0; i < rec.length; ++i) {
              if (seq[i] !== rec[i]) {
                var msg = "test " + index + " failed.";
                $log.error(msg, evt, "recorded:", rec);
                throw new Error(msg);
              }
            }
          }
        });
        $log.info("test succeeded!");
        rec = null;
      }
      $scope.clearValue = function(next) {
        if (data.value) {
          data.value = 0;
          return next;
        }
      };
      $scope.setValue = function(next) {
        if (!data.value) {
          data.value = 1;
          return next;
        }
      };
      $scope.clicked = function(evt) {
        $log.info("clicked", evt);
        $scope.$broadcast(evt);
      };

      test = [
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
