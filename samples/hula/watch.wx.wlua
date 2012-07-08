-------------------------------------------------------------------------=---
-- Name:        dialog.wx.wlua
-- Purpose:     A visual stop watch, based on the wxLua Dialog sample
-- Created:     July 2012
-- Copyright:   Copyright (c) 2012, everMany, LLC. All rights reserved.
-- Licence:     hsm-statechart
-------------------------------------------------------------------------=---
--package.cpath = package.cpath..";C:/dev/lua5.1/clibs/?.dll"
--;./?.dll;./?.so;../lib/?.so;../lib/vc_dll/?.dll;../lib/bcc_dll/?.dll;../lib/mingw_dll/?.dll;"

require "wx"
require "hsm_statechart"

-- Load the wxLua module, does nothing if running from wxLua, wxLuaFreeze, or wxLuaEdit
-- package.cpath = package.cpath..";./?.dll;./?.so;../lib/?.so;../lib/vc_dll/?.dll;../lib/bcc_dll/?.dll;../lib/mingw_dll/?.dll;"


-- Create the dialog, there's no reason why we couldn't use a wxFrame and
-- a frame would probably be a better choice.

local dialog = wx.wxDialog(wx.NULL, wx.wxID_ANY, "wxLua Stopwatch",
                     wx.wxDefaultPosition,
                     wx.wxDefaultSize)


-- IDs of the controls in the dialog
local controls= {
    time_ctrl = 2,
    reset_button  = 9,
    toggle_button = 10,
}

-- Create a wxPanel to contain all the buttons. It's a good idea to always
-- create a single child window for top level windows (frames, dialogs) since
-- by default the top level window will want to expand the child to fill the
-- whole client area. The wxPanel also gives us keyboard navigation with TAB key.
local panel = wx.wxPanel(dialog, wx.wxID_ANY)

-- Layout all the buttons using wxSizers
local mainSizer = wx.wxBoxSizer(wx.wxVERTICAL)

-- local staticBox      = wx.wxStaticBox(panel, wx.wxID_ANY, "Enter temperature")
-- local staticBoxSizer = wx.wxStaticBoxSizer(staticBox, wx.wxVERTICAL)

local flexGridSizer  = wx.wxFlexGridSizer( 3, 3, 0, 0 )
flexGridSizer:AddGrowableCol(1, 0)

local staticText = wx.wxStaticText( panel, wx.wxID_ANY, " Time ")

local textCtrl   = wx.wxTextCtrl( panel, controls.time_ctrl, "0", wx.wxDefaultPosition, wx.wxDefaultSize, wx.wxTE_PROCESS_ENTER )
flexGridSizer:Add( textCtrl,   0, wx.wxGROW+wx.wxALIGN_CENTER+wx.wxALL, 5 )
mainSizer:Add(      flexGridSizer, 1, wx.wxGROW+wx.wxALIGN_CENTER+wx.wxALL, 5 )

local buttonSizer = wx.wxBoxSizer( wx.wxHORIZONTAL )

local resetButton = wx.wxButton( panel, controls.reset_button, "&Reset")
local toggleButton = wx.wxButton( panel, controls.toggle_button, "&Toggle")

buttonSizer:Add( resetButton, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )
buttonSizer:Add( toggleButton, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )
mainSizer:Add(    buttonSizer, 0, wx.wxALIGN_CENTER+wx.wxALL, 5 )

panel:SetSizer( mainSizer )
mainSizer:SetSizeHints( dialog )


------------------------------------------------------------
-- create a simple representation of a watch
local stop_watch_chart= {
  active= {
    -- each chart has its own watch data
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
--- create the watch data and the statemachine
local watch= { time = 0 }
local hsm= hsm_statechart.new{ stop_watch_chart, context=watch }

-----------------------------------------------------------------------------
-- Connect a central event handler that responds to all button clicks.
--
-- Note: the stop_watch_chart could have used the control ids as strings directly, except:
-- i wanted to show that the above chart is exactly the same as in watch.lua,
-- it's just driven in a different way. Not too big a deal here, but reusable behavior is a powerful concept.
--
local event_table= {
   [controls.reset_button]=  "evt_reset",
   [controls.toggle_button]=  "evt_toggle"
}

dialog:Connect(wx.wxEVT_CLOSE_WINDOW,
    function (event)
        dialog:Destroy()
        event:Skip()
    end)

dialog:Connect(wx.wxID_ANY, wx.wxEVT_COMMAND_BUTTON_CLICKED,
    function(event)
        evt= event_table[ event:GetId() ]
        okay= evt and hsm:signal( evt )
        event:Skip()
    end)

dialog:Connect(wx.wxEVT_IDLE,
    function (event)
        hsm:signal( "evt_tick", 1 )
        textCtrl:SetValue( string.format("%.3f", watch.time/1000 ) )
        -- wx.wxSleep(1)
        event:RequestMore()
        event:Skip()
    end)
-- ---------------------------------------------------------------------------

-- Centre the dialog on the screen
dialog:Centre()
-- Show the dialog
dialog:Show(true)


-- Call wx.wxGetApp():MainLoop() to start the wxWidgets event loop,
wx.wxGetApp():MainLoop()
--print("okay")
