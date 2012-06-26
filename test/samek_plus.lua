function table_print (tt, indent, done)
  done = done or {}
  indent = indent or 0
  if type(tt) == "table" then
    for key, value in pairs (tt) do
      io.write(string.rep (" ", indent)) -- indent it
      if type (value) == "table" and not done [value] then
        done [value] = true
        io.write(string.format("[%s] => table\n", tostring (key)));
        io.write(string.rep (" ", indent+4)) -- indent it
        io.write("(\n");
        table_print (value, indent + 7, done)
        io.write(string.rep (" ", indent+4)) -- indent it
        io.write(")\n");
      else
        io.write(string.format("[%s] => %s\n",
            tostring (key), tostring(value)))
      end
    end
  else
    io.write(tt .. "\n")
  end
end


chart= {
  s0 = {
    entry=function(ctx)
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
        h = function(ctx)
              if (ctx.foo~=0) then
                ctx.foo=0
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
        h    = function(ctx)
                if (ctx.foo==0) then
                  ctx.foo=1
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
