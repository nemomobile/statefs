(provider "power" "/home/denis/src/mer/statefs/examples/src/libpower.so"
          (ns "battery"
              (prop "voltage" 3.8 :behavior continuous)
              (prop "current" 0 :behavior continuous)
              (prop "is_low" 0)))
