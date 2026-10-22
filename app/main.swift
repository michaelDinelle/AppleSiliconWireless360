import AppKit
import SystemExtensions

/// Windowless installer: activates the embedded driver, then quits. Opening the app again after an
/// update installs the new driver version. Moving the app to the Trash removes the driver.
final class AppDelegate: NSObject, NSApplicationDelegate, OSSystemExtensionRequestDelegate {
    static let driverIdentifier = "com.michealdinelle.wireless360.driver"

    func applicationDidFinishLaunching(_ notification: Notification) {
        let request = OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: Self.driverIdentifier, queue: .main)
        request.delegate = self
        OSSystemExtensionManager.shared.submitRequest(request)
    }

    func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        .replace
    }

    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        // First install only: take the user straight to the switch they need to turn on.
        NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.LoginItems-Settings.extension")!)
        NSApp.terminate(nil)
    }

    func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        NSApp.terminate(nil)
    }

    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        let alert = NSAlert()
        alert.messageText = "Wireless360 couldn't install its driver"
        alert.informativeText = error.localizedDescription
        alert.runModal()
        NSApp.terminate(nil)
    }
}

let delegate = AppDelegate()
NSApplication.shared.delegate = delegate
NSApplication.shared.run()
