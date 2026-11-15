# Run the reconnect regression

From the repository root, using CMake and a desktop C++ compiler:

```sh
cmake -S tests -B build/tests
cmake --build build/tests --config Release
ctest --test-dir build/tests -C Release --output-on-failure
```

The executable compiles the production touch implementation against a small fake of the AmebaBLE and Realtek profile APIs. It disconnects and reconnects the same host on slot 0 without calling a sketch API in between. It checks the first move, release and press after reconnection, the emitted reports, and interruptions during a swipe or between the two taps of a double tap. The observer must also be installed after an explicit end/begin cycle.

On hardware, hold a contact, disconnect the host without calling the library's `end()`, and reconnect the existing pairing. While offline, avoid polling `isConnected()`. On the new link, `moveTo()` and `release()` must both return false. A fresh `tap()` should reach the host.

The `paths` executable checks complete-path validation before pressing, initial/intermediate/final holds, report timestamps, continuous contact, reverse segments, Pixel shortcuts with rotation, legacy swipe equivalence, and disconnects during each gesture phase.
