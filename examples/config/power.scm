(plugin "power" "./examples/src/libpower.so"
        ; power interface
        (ns "battery"
            (prop "voltage" 3.8 :behavior continuous)
            (prop "percentage" 50)
            (prop "current" 0.4)
            (prop "is_low" false)
        ))
