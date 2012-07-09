function table_print (tt, indent)
  local indent = indent or 0
  if type(tt) ~= "table" then
    io.write( tt .. "\n" )
  else
    local a,d = {}, {}
    for k,v in next,tt do
      local str= tostring(k) -- lua cant sort string and numbers together :(
      a[#a+1]= str
      d[str]= k
    end
    table.sort(a)
    for _,akey in next, a do
      local key= d[akey]
      local value= tt[ key ]
      io.write(string.rep (" ", indent)) -- indent it
      if type (value) == "table" then -- and not done [value] then
        -- done [value] = true
        io.write(string.format('["%s"] = {\n', tostring (key)));
        table_print (value, indent + 2, done)
        io.write(string.rep (" ", indent)) -- indent it
        io.write("},\n");
      else
        io.write(string.format('["%s"] = %s,\n',
          tostring (key), tostring(value)))
      end
    end
  end
end



chart= {
  s0 = {
    entry=function()
              return { foo= 0 }
          end,
    -- exit =
    init = 's1',
    e    = 's211',
    i    = 's12',

    s1 = {
      init = 's11',
      a    = 's1',
      b    = 's11',
      c    = 's2',
      d    = 's0',
      f    = 's211',

      s11 = {
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
      }
    },
    s2= {
      c    = 's1',
      f    = 's11',
      init = 's21',

      s21 = {
        init = 's211',
        b    = 's211',
        h    = function(context)
                if (context.foo==0) then
                  context.foo=1
                  return 's21'
                end
              end,
        s211 = {
          d = 's21',
          g = 's0',
        }
      }
    }
  }
}

if arg then table_print( chart ) end

return chart
