import SwiftUI
import CoreAudio

struct ContentView: View {
    @State private var driverDetected = false
    @State private var installedDriverVersion: String?
    @State private var liveDriverStatus: DriverLiveStatus?
    @State private var hasChecked = false
    @State private var isProcessing = false
    @State private var errorMessage: String?
    @State private var showUninstallConfirm = false

    private let halPath = "/Library/Audio/Plug-Ins/HAL/ProxyAudio.driver"
    private let driverBundleIdentifier = "com.TapTurtle.ProxyAudio"

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

            Text(driverVersionText)
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
                Label(driverStatusTitle, systemImage: "checkmark.circle.fill")
                    .font(.headline)
                    .foregroundStyle(.green)

                Text(driverStatusDetail)
                    .font(.caption)
                    .foregroundStyle(.secondary)

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

    private var driverStatusTitle: String {
        liveDriverStatus == nil ? "Driver Installed" : "Driver Active"
    }

    private var driverStatusDetail: String {
        guard let liveDriverStatus else {
            return "Installed, but CoreAudio has not activated it yet."
        }
        return "CoreAudio is responding (build \(liveDriverStatus.buildVersion))."
    }

    private var driverVersionText: String {
        if let liveDriverStatus {
            return "Driver version \(liveDriverStatus.driverVersion)"
        }
        if let installedDriverVersion {
            return "Installed version \(installedDriverVersion)"
        }
        return "Driver version —"
    }

    private func refreshStatus() {
        installedDriverVersion = installedProxyAudioDriverVersion(at: halPath)
        driverDetected = installedDriverVersion != nil
        liveDriverStatus = readLiveProxyAudioDriverStatus(
            bundleIdentifier: driverBundleIdentifier
        )
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

private struct DriverLiveStatus {
    let driverVersion: String
    let buildVersion: String
}

private let driverStatusProperty: AudioObjectPropertySelector = 0x5652534E // 'VRSN'
private let driverStatusSchemaVersion = 1
private let driverStatusProtocolVersion = 1

private func installedProxyAudioDriverVersion(at path: String) -> String? {
    guard let bundle = Bundle(path: path),
          bundle.bundleIdentifier == "com.TapTurtle.ProxyAudio",
          let version = bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String,
          !version.isEmpty else {
        return nil
    }
    return version
}

private func readLiveProxyAudioDriverStatus(bundleIdentifier: String) -> DriverLiveStatus? {
    guard let pluginID = audioPluginID(bundleIdentifier: bundleIdentifier) else {
        return nil
    }

    var propertyAddress = AudioObjectPropertyAddress(
        mSelector: driverStatusProperty,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain
    )

    var dictionary: Unmanaged<CFDictionary>?
    var dataSize = UInt32(MemoryLayout<Unmanaged<CFDictionary>?>.size)
    let status = AudioObjectGetPropertyData(
        pluginID,
        &propertyAddress,
        0, nil,
        &dataSize,
        &dictionary
    )
    guard status == noErr,
          let dictionary,
          let values = dictionary.takeRetainedValue() as NSDictionary as? [String: Any],
          let schemaVersion = values["schemaVersion"] as? NSNumber,
          schemaVersion.intValue == driverStatusSchemaVersion,
          let protocolVersion = values["protocolVersion"] as? NSNumber,
          protocolVersion.intValue == driverStatusProtocolVersion,
          values["bundleIdentifier"] as? String == bundleIdentifier,
          values["state"] as? String == "ready",
          values["live"] as? Bool == true,
          let driverVersion = values["driverVersion"] as? String,
          let buildVersion = values["buildVersion"] as? String,
          !driverVersion.isEmpty,
          !buildVersion.isEmpty else {
        return nil
    }

    return DriverLiveStatus(driverVersion: driverVersion, buildVersion: buildVersion)
}

private func audioPluginID(bundleIdentifier: String) -> AudioObjectID? {
    var propertyAddress = AudioObjectPropertyAddress(
        mSelector: kAudioHardwarePropertyPlugInForBundleID,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain
    )
    var inputBundleIdentifier = bundleIdentifier as CFString
    var pluginID = AudioObjectID(kAudioObjectUnknown)

    return withUnsafeMutablePointer(to: &inputBundleIdentifier) { inputPointer in
        withUnsafeMutablePointer(to: &pluginID) { outputPointer in
            var translation = AudioValueTranslation(
                mInputData: inputPointer,
                mInputDataSize: UInt32(MemoryLayout<CFString>.size),
                mOutputData: outputPointer,
                mOutputDataSize: UInt32(MemoryLayout<AudioObjectID>.size)
            )
            var dataSize = UInt32(MemoryLayout<AudioValueTranslation>.size)
            let status = AudioObjectGetPropertyData(
                AudioObjectID(kAudioObjectSystemObject),
                &propertyAddress,
                0, nil,
                &dataSize,
                &translation
            )
            if status == noErr, pluginID != AudioObjectID(kAudioObjectUnknown) {
                return pluginID
            }
            return nil
        }
    }
}
