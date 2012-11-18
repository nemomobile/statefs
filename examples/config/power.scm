(plugin "power" "./examples/src/libpower.so"
        ; power interface
        (ns "battery"
            (prop "voltage" 3.8 :type continuous)
            (prop "percentage" 50)
        ))
