----------------------------------------------------------------------------
-- Name:        watch.lua
-- Purpose:     An command line stop watch that is meant to be run from C.
--              Relies on platform specific functions not included in the rock.
--              See: watch.wx.wlua for a runnable version.
-- Created:     July 2012
-- Copyright:   Copyright (c) 2012, everMany, LLC. All rights reserved.
-- Licence:     hsm-statechart
----------------------------------------------------------------------------

require "hsm_statechart"

------------------------------------------------------------
local stop_watch_chart= {
  -- each state is represented as a table
  -- active is the top-most state
  active= {
    -- entry to the active state clears the watch's timer
    -- the watch is provided by machines using this chart
    entry=
      function(watch) 
        watch.time=0
        return watch
      end,

    -- reset causes a self transition which re-enters active
    -- and clears the time no matter which state the machine is in
    evt_reset = 'active',  

    -- the active state is stopped by default:
    init = 'stopped',

    -- while the watch is stopped: 
    stopped = {
      -- the toggle button starts the watch running
      evt_toggle = 'running',
    },

    -- while the watch is running:
    running = {
      -- the toggle button stops the watch
      evt_toggle = 'stopped',

      -- the tick of time updates the watch
      evt_tick   =
        function(watch, time)
          watch.time= watch.time + time
        end,
    }
  }
}

------------------------------------------------------------
function run_watch_run()
  print([[Hula stopwatch sample.
    Keys:
        '1': reset button,
        '2': generic toggle button,
        'x': quit.]])

  -- create a simple representation of a watch
  local watch= { time = 0 }

  -- create a state machine with the chart, and the watch as context
  local hsm= hsm_statechart.new{ stop_watch_chart, context= watch }

  -- lookup table for mapping keyboard to events
  local key_to_event = { ["1"]='evt_reset',
                         ["2"]='evt_toggle' }

  -- keep going until the watch breaks
  while hsm:is_running() do
    -- read one character
    local key= string.char( platform.get_key() )
    if key == 'x' then break end
    -- change it into an event
    local event= key_to_event[key]
    -- send it to the statemachine
    if event then
      hsm:signal( event )
      io.write( "." )
    else
    -- otherwise mimic the passage of time
      local t= watch.time
      hsm:signal( 'evt_tick', 1 )
      platform.sleep(500)
      if t ~= watch.time then
          io.write( watch.time, "," )
      end          
    end
  end
end

------------------------------------------------------------
run_watch_run()
