local chart= {
  active= {
    init = 'stopped',
    entry=
      function(watch)
        watch.time=0
        return watch
      end,
    evt_reset = 'active',
    stopped = {
      evt_toggle = 'running',
    },
    running = {
      evt_toggle = 'stopped',
      evt_tick   =
        function(watch, time)
          watch.time= watch.time + time
          io.write( watch.time, "," )
        end,
    }
  }
}

function run_watch_run()
  -- create a simple representation of a watch
  local watch= { time = 0 }

  -- create a state machine with the chart, and the watch
  local hsm= hsm_statechart.new{ chart, context= watch }

  print([[Hula's stopwatch sample.
    Keys:
        '1': reset button
        '2': generic toggle button]])

  local key_to_event = { ["1"]='evt_reset',
                         ["2"]='evt_toggle' }
  while hsm:is_running() do
    -- read one character
    local key= string.char( platform.get_key() )
    local event= key_to_event[key]
    if event then
      hsm:signal( event )
      io.write( "." )
    else
      hsm:signal( 'evt_tick', 1 )
      platform.sleep(500)
    end
  end
end

run_watch_run()
