package = "hsm_statechart"
version = "0.5-1"

source = {
  url = "http://..." -- We don't have one yet
}

description = {
  summary = "State machine engine based on UML statecharts.",
  detailed = [[
    hsm-statechart is a hierarchical statechart engine, written in C. 
    The Lua extension makes it dead simple to write statemachines: 
    you just declare a table.

    chart = { state= { event= function() end, substate={ ... } } }
         
    Both event handlers and states are key:value pairs. 
    Event names map to functions ( or, for simple transitions, directly to a state name ), 
    while states names map to sub-tables.
     
     Two calls: hsm= HsmStatechart.new( chart ) and hsm.signal( event_name ) 
     are all that are required to run the statemahine.

     Statemachines can be used for everything from building roboust text parsers, 
     to managing the control flow for an application, or even writing AI in games.
     And, hsm-statechart is completely open source.
   ]],
   homepage = "http://code.google.com/p/hsm-statechart/",
   license = "New BSD License"
}

dependencies = {
   "lua >= 5.1"
   -- If you depend on other rocks, add them here
}

build = {
  type= "builtin",
  modules = {
    -- note: the module is the dll name
    hsm_statechart = {
        sources= {
            "../hsm/hsm_context.c",
            "../hsm/hsm_machine.c",
            "../hsm/builder/hash.c",
            "../hsm/builder/lower.c",
            "../hsm/builder/hsm_builder.c",
            "../hsm/hula/hula.c",
            "../hsm/hula/hula_lib.c",
        },
        incdirs= {".."},
      },
  },     
}
