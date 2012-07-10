require "hsm_statechart"

chart= {
  s0 = {
    entry=function()
            print("entered s0")
            return { foo= 0 }
          end,
    exit= function()
            print("exited s0")
          end,
    -- exit =
    init = 's1',
    e    = 's211',
    i    = 's12',

    s1 = {
      entry=function()
            print("entered s1")
            end,
      exit= function()
              print("exited s1")
            end,
      init = 's11',
      a    = 's1',
      b    = 's11',
      c    = 's2',
      d    = 's0',
      f    = 's211',

      s11 = {
        entry=function()
                  print("entered s11")
              end,
        exit= function()
                print("exited s11")
              end,
        g = 's211',
        h = function(context)
              if (context.foo~=0) then
                context.foo=0
                return true
              end
            end
      },

      s12 = {
        -- empty leaf state
        entry=function()
                  print("entered s12")
              end,
        exit= function()
                print("exited s12")
              end,
      }
    },
    s2= {
      entry=function()
              print("entered s2")
            end,
      exit= function()
              print("exited s2")
            end,
      c    = 's1',
      f    = 's11',
      init = 's21',

      s21 = {
        entry=function()
                print("entered s21")
              end,
        exit= function()
                print("exited s21")
              end,
        init = 's211',
        b    = 's211',
        h    = function(context)
                if (context.foo==0) then
                  context.foo=1
                  return 's21'
                end
              end,
        s211 = {
          entry=function()
                  print("entered s211")
                end,
          exit= function()
                  print("exited s211")
                end,
          d = 's21',
          g = 's0',
        }
      }
    }
  }
}

watch= { time=0 }
send= { "a", "e", "e", "a", "h", "h", "g", "h", "i", "x" }
print("STARTING")
local hsm= hsm_statechart.new{ chart, context=watch }
for i =1, #send do
  ch= send[i]
  print("SENDING", ch )
  hsm:signal( ch )
end
print("DONE")

