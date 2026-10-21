// Prints what Apple's Game Controller framework sees, then input changes, for N seconds (default 15).
// Build: swiftc -O tools/gc_check.swift -o build/gc_check && build/gc_check 15
import Foundation
import GameController

setvbuf(stdout, nil, _IOLBF, 0)
let secs = CommandLine.arguments.count > 1 ? Double(CommandLine.arguments[1]) ?? 15 : 15
func attach(_ c: GCController) {
    print("controller: \(c.vendorName ?? "?") category=\(c.productCategory) extended=\(c.extendedGamepad != nil)")
    c.extendedGamepad?.valueChangedHandler = { _, element in
        if let b = element as? GCControllerButtonInput { print("  button \(b.localizedName ?? "?") = \(b.value)") }
        else if let d = element as? GCControllerDirectionPad { print("  stick/dpad \(d.localizedName ?? "?") = (\(d.xAxis.value), \(d.yAxis.value))") }
    }
}
NotificationCenter.default.addObserver(forName: .GCControllerDidConnect, object: nil, queue: .main) { n in
    if let c = n.object as? GCController { attach(c) }
}
GCController.shouldMonitorBackgroundEvents = true   // a command-line tool is never frontmost
GCController.controllers().forEach(attach)
RunLoop.main.run(until: Date(timeIntervalSinceNow: secs))
print("done")
