----------------------------------------------------------------------------
-- Name:        dialog.wx.wlua
-- Purpose:     A visual stop watch, based on the wxLua Dialog sample.
--              Requires the wxLua rock.
-- Created:     July 2012
-- Copyright:   Copyright (c) 2012, everMany, LLC. All rights reserved.
-- Licence:     hsm-statechart
-----------------------------------------------------------------------------
 require "wx"
require "hsm_statechart"


--------------------------------------------------------------------------------
-- Build a wx dialog for the stop watch display.
-- expects controls to containg time_ctrl, reset_button, and toggle_buutton ids
--------------------------------------------------------------------------------
function createDialog(controls)
  local dialog = wx.wxDialog(wx.NULL, wx.wxID_ANY, "wxLua Stopwatch",
                       wx.wxDefaultPosition,
                       wx.wxDefaultSize)
 
  -- Create a wxPanel to contain all the buttons. 
  local panel = wx.wxPanel(dialog, wx.wxID_ANY)

  -- Layout all the buttons using wxSizers
  local mainSizer = wx.wxBoxSizer(wx.wxVERTICAL)
  local flexGridSizer  = wx.wxFlexGridSizer( 10, 10, 0, 0 )
  flexGridSizer:AddGrowableCol(1, 0)

  local staticText = wx.wxStaticText( panel, wx.wxID_ANY, " Time ")
  local textCtrl   = wx.wxTextCtrl( panel, controls.time_ctrl, "0", wx.wxDefaultPosition, wx.wxDefaultSize, wx.wxTE_PROCESS_ENTER )
  flexGridSizer:Add( staticText,   0, wx.wxGROW+wx.wxALIGN_CENTER+wx.wxALL, 5 )
  flexGridSizer:Add( textCtrl,   0, wx.wxGROW+wx.wxALIGN_CENTER+wx.wxALL, 5 )
  mainSizer:Add( flexGridSizer, 1, wx.wxGROW+wx.wxALIGN_CENTER+wx.wxALL, 5 )

  local buttonSizer = wx.wxBoxSizer( wx.wxHORIZONTAL )

  local resetButton = wx.wxButton( panel, controls.reset_button, "&Reset")
  local toggleButton = wx.wxButton( panel, controls.toggle_button, "&Toggle")

  buttonSizer:Add( resetButton, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )
  buttonSizer:Add( toggleButton, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )
  mainSizer:Add(    buttonSizer, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )

  panel:SetSizer( mainSizer )
  mainSizer:SetSizeHints( dialog )
  dialog:Centre()

  dialog.textCtrl  = textCtrl
  return dialog
end

------------------------------------------------------------
-- Define the stop watch logic.
-- expects a watch object with a time field
------------------------------------------------------------
local stop_watch_chart= {
  active= {
    -- each chart needs watch data
    entry=
      function(watch)
        watch.time=0
        return watch
      end,

    -- reset causes a self transition which clears the time,
    -- no matter which sub-state the chart is in
    evt_reset= 'active',

    -- watches are stopped by default:
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
      evt_tick =
        function(watch, time)
          watch.time= watch.time + time
        end,
    }
  }
}

----------------------------------------------------------------------
--- Create the watch and the statemachine
-----------------------------------------------------------------------------
local watch= { time = 0 }
local hsm= hsm_statechart.new{ stop_watch_chart, context=watch }

-----------------------------------------------------------------------------
-- Create the dialog and the UI event responders
-----------------------------------------------------------------------------

-- dialog control ids
local controls= {
    time_ctrl = 2,
    reset_button  = 9,
    toggle_button = 10,
}
dialog= createDialog(controls)

-- map controls to stop watch chart events.
-- note: the chart (above) could have used the control ids as strings directly, except:
-- I wanted to show the chart can be exactly the same as the cmdline version.
-- Not too big a deal here, but reusable behavior is a powerful concept.
local event_table= {
   [controls.reset_button]=  "evt_reset",
   [controls.toggle_button]= "evt_toggle"
}

-- one button handler to rule them all.
-- not too big a deal when there's only two buttons, 
-- but for a more complex ui, keeps the ui handling simple, 
-- and abstracts the ui code from the behavior of the watch.
dialog:Connect(wx.wxID_ANY, wx.wxEVT_COMMAND_BUTTON_CLICKED,
    function(event)
        evt= event_table[ event:GetId() ]
        okay= evt and hsm:signal( evt )
        event:Skip()
    end)

-- on idle, tick time, and update the display.
dialog:Connect(wx.wxEVT_IDLE,
    function (event)
        hsm:signal( "evt_tick", 1 )
        dialog.textCtrl:SetValue( string.format("%.3f", watch.time/1000 ) )
        event:RequestMore()
        event:Skip()
    end)

-- close the dailog, kill the app.
dialog:Connect(wx.wxEVT_CLOSE_WINDOW,
    function (event)
        dialog:Destroy()
        event:Skip()
    end)

-----------------------------------------------------------------------------
-- Run the dialog
-----------------------------------------------------------------------------
dialog:Show(true)
wx.wxGetApp():MainLoop()
