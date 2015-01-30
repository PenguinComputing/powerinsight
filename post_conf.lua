-- Post .conf file configuration
--

-- Global
Update = { } -- List of "update" sensors

-- Finalize the system configuration
do

-- Loop through configured sensors
--    Collect "update" sensors and update their values
  local k, v, s
  for k, s in ipairs( S ) do
    if s.update ~= nil then
      table.insert(Update,s)
      s:update( )
    end
  end

-- Default App function
  local function defaultApp (...)
    local args = { ... }
    if #args == 0 then
      -- No arguments, read and display every named sensor
      for k, s in ipairs( S ) do
        if s.name ~= nil then
          table.insert( args, s.name )
        end
      end
    end

    -- Loop through args, read each and print
    local start = pi.gettime( )
    io.write( string.format( "# Starting at %.6f sec\n", start ) )
    for k, v in ipairs( args ) do
      s = byName[v]
      if s == nil then
        io.write( string.format( "%-10s NOT FOUND\n", v ) )
      elseif s.temp ~= nil then
        io.write( string.format( "%-10s %7.2f degC\n", v, s:temp( ) ) )
      elseif s.volt ~= nil and s.amp ~= nil then
        io.write( string.format( "%-10s %8.3f Watts [ %7.3f Volts %7.3f Amps ]\n", v, s:power( ) ) )
      elseif s.volt ~= nil and s.amp == nil then
        io.write( string.format( "%-10s %8.3f Volts\n", v, s:volt( ) ) )
      else
        io.write( string.format( "%-10s UNREADABLE\n", v ) )
      end
    end
    io.write( string.format( "# Completed in %.6f sec\n", pi.gettime(start) ) )
  end

-- If user didn't provide an App global, export the default App
  if type(App) ~= "nil" and type(App) ~= "function" then
    error( "App global is type "..type(App)..", not a function" )
  end

  if type(App) == "nil" then
    _G.App = defaultApp
  end
end

-- ex: set sw=2 sta et : --
