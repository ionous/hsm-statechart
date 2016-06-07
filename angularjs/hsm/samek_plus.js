'use strict';

/** 
 */
angular.module('hsm')

.controller('SamekPlus',
  function($log, $scope) {
    var plus = this;
    plus.value = 0;
    plus.set = function(v) {
      if (plus.value != v) {
        $log.warn("changing value", v);
        plus.value = v;
      }
      return v;
    };
    plus.clicked = function(evt) {
      $log.info("clicked", evt);
      $scope.machine.emit(evt);
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
});
