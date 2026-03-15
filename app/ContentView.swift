import SwiftUI
import CoreAudio

struct ContentView: View {
    @State private var driverDetected = false
    @State private var hasChecked = false
    @State private var isProcessing = false
    @State private var errorMessage: String?
    @State private var showUninstallConfirm = false

    private let halPath = "/Library/Audio/Plug-Ins/HAL/ProxyAudio.driver"

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "speaker.wave.3")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)

            Text("ProxyAudio")
                .font(.title.bold())
            Text("Virtual Audio Driver")
                .font(.subheadline)
                .foregroundStyle(.secondary)

            Divider()
                .padding(.horizontal, 40)

            if !hasChecked || isProcessing {
                ProgressView()
                    .controlSize(.small)
            } else {
                statusView
            }

            Text("Version \(driverVersion)")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .padding(32)
        .frame(width: 400)
        .task { refreshStatus() }
        .alert("Error", isPresented: showErrorBinding) {
            Button("OK") { errorMessage = nil }
        } message: {
            Text(errorMessage ?? "")
        }
        .alert("Uninstall Driver?", isPresented: $showUninstallConfirm) {
            Button("Cancel", role: .cancel) {}
            Button("Uninstall", role: .destructive) { performUninstall() }
        } message: {
            Text("This will remove the ProxyAudio driver and restart the audio system.")
        }
    }

    @ViewBuilder
    private var statusView: some View {
        if driverDetected {
            VStack(spacing: 12) {
                Label("Driver Installed", systemImage: "checkmark.circle.fill")
                    .font(.headline)
                    .foregroundStyle(.green)

                Button("Uninstall Driver") { showUninstallConfirm = true }
                    .controlSize(.large)

                Button("Reinstall Driver") { performReinstall() }
                    .font(.caption)
                    .buttonStyle(.borderless)
                    .foregroundStyle(.secondary)
            }
        } else {
            VStack(spacing: 12) {
                Label("Driver Not Installed", systemImage: "xmark.circle.fill")
                    .font(.headline)
                    .foregroundStyle(.secondary)

                Button("Install Driver") { performInstall() }
                    .controlSize(.large)
                    .buttonStyle(.borderedProminent)
            }
        }
    }

    private var showErrorBinding: Binding<Bool> {
        Binding(get: { errorMessage != nil }, set: { if !$0 { errorMessage = nil } })
    }

    private var driverVersion: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "1.0.0"
    }

    private func refreshStatus() {
        let installed = FileManager.default.fileExists(atPath: halPath)
        let detected = checkForProxyAudioDevices()
        driverDetected = installed || detected
        hasChecked = true
    }

    private func performInstall() {
        guard let source = Bundle.main.path(forResource: "ProxyAudio", ofType: "driver") else {
            errorMessage = "Driver bundle not found in app resources."
            return
        }
        isProcessing = true
        let cmds = [
            "rm -rf '\(halPath)'",
            "cp -R '\(source)' '\(halPath)'",
            "chown -R root:wheel '\(halPath)'",
            "killall -9 coreaudiod 2>/dev/null || true"
        ].joined(separator: " && ")
        runPrivileged(cmds)
    }

    private func performUninstall() {
        isProcessing = true
        let cmds = [
            "rm -rf '\(halPath)'",
            "killall -9 coreaudiod 2>/dev/null || true"
        ].joined(separator: " && ")
        runPrivileged(cmds)
    }

    private func performReinstall() {
        performInstall()
    }

    private func runPrivileged(_ command: String) {
        let escaped = command.replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
        let source = "do shell script \"\(escaped)\" with administrator privileges"
        var error: NSDictionary?
        NSAppleScript(source: source)?.executeAndReturnError(&error)

        if let error = error {
            let msg = error[NSAppleScript.errorMessage] as? String ?? "Unknown error"
            if !msg.contains("canceled") && !msg.contains("cancelled") {
                errorMessage = msg
            }
        }

        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
            isProcessing = false
            refreshStatus()
        }
    }
}

private func checkForProxyAudioDevices() -> Bool {
    var propertyAddress = AudioObjectPropertyAddress(
        mSelector: kAudioHardwarePropertyDevices,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain
    )

    var dataSize: UInt32 = 0
    var status = AudioObjectGetPropertyDataSize(
        AudioObjectID(kAudioObjectSystemObject),
        &propertyAddress,
        0, nil,
        &dataSize
    )
    guard status == noErr, dataSize > 0 else { return false }

    let deviceCount = Int(dataSize) / MemoryLayout<AudioObjectID>.size
    var deviceIDs = [AudioObjectID](repeating: 0, count: deviceCount)
    status = AudioObjectGetPropertyData(
        AudioObjectID(kAudioObjectSystemObject),
        &propertyAddress,
        0, nil,
        &dataSize,
        &deviceIDs
    )
    guard status == noErr else { return false }

    let proxyDeviceSuffix = "_PA_Proxy"

    var uidAddress = AudioObjectPropertyAddress(
        mSelector: kAudioDevicePropertyDeviceUID,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain
    )

    for deviceID in deviceIDs {
        var cfUID: Unmanaged<CFString>?
        var uidSize = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
        let uidStatus = AudioObjectGetPropertyData(
            deviceID,
            &uidAddress,
            0, nil,
            &uidSize,
            &cfUID
        )
        if uidStatus == noErr, let uid = cfUID?.takeRetainedValue() as String? {
            if uid.hasSuffix(proxyDeviceSuffix) {
                return true
            }
        }
    }

    return false
}
