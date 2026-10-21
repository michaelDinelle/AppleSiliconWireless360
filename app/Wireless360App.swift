import SwiftUI
import SystemExtensions

@main
struct Wireless360App: App {
    var body: some Scene {
        WindowGroup("Wireless360") {
            ContentView().frame(minWidth: 460, minHeight: 200)
        }
        .windowResizability(.contentSize)
    }
}

struct ContentView: View {
    @StateObject private var manager = ExtensionManager()

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Wireless360").font(.title)
            Text("Driver for the Xbox 360 Wireless Receiver.")
            HStack {
                Button("Install driver") { manager.submit(activate: true) }
                Button("Uninstall driver") { manager.submit(activate: false) }
            }
            .disabled(manager.busy)
            Text(manager.status).font(.callout).textSelection(.enabled)
        }
        .padding(20)
    }
}

/// Activates or deactivates the embedded driver extension and reports the outcome.
@MainActor
final class ExtensionManager: NSObject, ObservableObject, OSSystemExtensionRequestDelegate {
    static let driverIdentifier = "com.michealdinelle.wireless360.driver"

    @Published var status = "Not installed in this session."
    @Published var busy = false

    func submit(activate: Bool) {
        let request = activate
            ? OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: Self.driverIdentifier, queue: .main)
            : OSSystemExtensionRequest.deactivationRequest(forExtensionWithIdentifier: Self.driverIdentifier, queue: .main)
        busy = true
        status = "Waiting for macOS..."
        request.delegate = self
        OSSystemExtensionManager.shared.submitRequest(request)
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        .replace
    }

    nonisolated func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        Task { @MainActor in
            status = "Approve it in System Settings > General > Login Items & Extensions > Driver Extensions."
        }
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        Task { @MainActor in
            busy = false
            status = result == .willCompleteAfterReboot
                ? "Done after the next restart."
                : "Done. Plug in the receiver and press the controller's guide button."
        }
    }

    nonisolated func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        Task { @MainActor in
            busy = false
            status = "Failed: \(error.localizedDescription)"
        }
    }
}
