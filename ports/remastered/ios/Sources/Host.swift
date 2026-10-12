// Host.swift: the iPhone host -- what the platform layer asks of the
// operating system (ports/brally/platform/host/host.h) that host/posix does
// not already give: the window (a Metal layer the view controller owns), the
// event queue, the pad (touch, TouchPad.swift), Core Audio out, the save
// folder and Boss Rally's disc.
//
// It is one host, as host/macos is on the Mac: the two games share it through
// the multiplexer (ports/remastered/platform/mux.c), which gives the one in
// front the events, the pad, the screen and its sound, and mixes the two.
//
// Each function is exported under its C name; the Swift names differ so they
// do not collide with host.h's declarations imported through Bridge.h.

import AVFoundation
import AudioToolbox
import QuartzCore
import UIKit

// ---- process -------------------------------------------------------------------

/// The app's folder name for its saves (main.c names it before host_init).
private var appDir = "Top Gear Rally Remastered"

@_cdecl("host_set_app_name")
func hostSetAppName(_ dir: UnsafePointer<CChar>?, _ title: UnsafePointer<CChar>?) {
    if let dir { appDir = String(cString: dir) }
}

@_cdecl("host_init")
func hostInit(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?) {}

/// The saves, in Application Support (Boss Rally's in a folder inside: mux.c)
private let saveDir: UnsafeMutablePointer<CChar> = {
    let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
    let dir = base.appendingPathComponent(appDir, isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    return strdup(dir.path)
}()
private let resourceDir: UnsafeMutablePointer<CChar> = strdup(Bundle.main.resourcePath ?? ".")
private let discDir: UnsafeMutablePointer<CChar> = strdup((Bundle.main.resourcePath ?? ".") + "/disc")

@_cdecl("host_save_dir")
func hostSaveDir() -> UnsafePointer<CChar>? { UnsafePointer(saveDir) }

/// Boss Rally's CD root and install: its data track, in the app
@_cdecl("host_cd_dir")
func hostCdDir() -> UnsafePointer<CChar>? { UnsafePointer(discDir) }

@_cdecl("host_game_dir")
func hostGameDir() -> UnsafePointer<CChar>? { UnsafePointer(discDir) }

@_cdecl("host_resource_dir")
func hostResourceDir() -> UnsafePointer<CChar>? { UnsafePointer(resourceDir) }

/// The CD's audio tracks are not in the app (the music under a race is Top Gear Rally's).
@_cdecl("host_music_dir")
func hostMusicDir() -> UnsafePointer<CChar>? { nil }

@_cdecl("host_stream_open")
func hostStreamOpen(_ path: UnsafePointer<CChar>?, _ rate: Int32) -> OpaquePointer? { nil }

@_cdecl("host_stream_read")
func hostStreamRead(_ s: OpaquePointer?, _ lr: UnsafeMutablePointer<Float>?, _ frames: Int32) -> Int32 { 0 }

@_cdecl("host_stream_close")
func hostStreamClose(_ s: OpaquePointer?) {}

@_cdecl("host_shutdown")
func hostShutdown() { hostAudioClose() }

@_cdecl("host_message_box")
func hostMessageBox(_ text: UnsafePointer<CChar>?, _ caption: UnsafePointer<CChar>?) {
    NSLog("%@: %@", caption.map { String(cString: $0) } ?? "", text.map { String(cString: $0) } ?? "")
}

// ---- the window: the view controller's Metal layer -------------------------------

/// The layer the renderers draw into, and its size in pixels: set on the main
/// thread when the view lays out, read on the games'.
enum Screen {
    static var layer: CAMetalLayer?
    private static let lock = NSLock()
    private static var w: Int32 = 0, h: Int32 = 0

    static func setPixels(_ pw: Int32, _ ph: Int32) {
        lock.lock(); w = pw; h = ph; lock.unlock()
    }
    static func pixels() -> (Int32, Int32) {
        lock.lock(); defer { lock.unlock() }
        return (w, h)
    }
}

@_cdecl("host_window_open")
func hostWindowOpen(_ w: Int32, _ h: Int32, _ title: UnsafePointer<CChar>?) -> Int32 {
    Screen.layer != nil ? 1 : 0
}

@_cdecl("host_window_close")
func hostWindowClose() {}

@_cdecl("host_window_visible")
func hostWindowVisible() -> Int32 { Screen.layer != nil ? 1 : 0 }

@_cdecl("host_window_lock_aspect")
func hostWindowLockAspect(_ lock: Int32) {}      // the screen's shape is fixed (16:9, landscape)

@_cdecl("host_present")
func hostPresent(_ argb: UnsafePointer<UInt32>?, _ w: Int32, _ h: Int32) {}   // Metal presents itself

@_cdecl("host_macos_metal_layer")
func hostMetalLayer() -> UnsafeMutableRawPointer? {
    Screen.layer.map { Unmanaged.passUnretained($0).toOpaque() }
}

@_cdecl("host_window_handle")
func hostWindowHandle() -> UnsafeMutableRawPointer? { hostMetalLayer() }

@_cdecl("host_macos_layer_fit")
func hostLayerFit(_ w: UnsafeMutablePointer<Int32>, _ h: UnsafeMutablePointer<Int32>) {
    (w.pointee, h.pointee) = Screen.pixels()
}

@_cdecl("host_window_pixels")
func hostWindowPixels(_ w: UnsafeMutablePointer<Int32>, _ h: UnsafeMutablePointer<Int32>) {
    (w.pointee, h.pointee) = Screen.pixels()
}

// ---- events: the app pushes, the multiplexer drains ----------------------------------

enum Events {
    private static let cond = NSCondition()
    private static var queue: [host_event] = []

    static func push(_ e: host_event) {
        cond.lock(); queue.append(e); cond.broadcast(); cond.unlock()
    }
    static func focus(_ on: Bool) {
        var e = host_event()
        e.type = Int32(HOST_EV_FOCUS)
        e.down = on ? 1 : 0
        push(e)
    }
    static func pop(_ ev: UnsafeMutablePointer<host_event>, _ waitMs: UInt32) -> Int32 {
        cond.lock(); defer { cond.unlock() }
        if queue.isEmpty && waitMs > 0 {
            let until = waitMs == 0xFFFF_FFFF ? Date.distantFuture
                                              : Date(timeIntervalSinceNow: Double(waitMs) / 1000)
            while queue.isEmpty && cond.wait(until: until) {}
        }
        if queue.isEmpty { return 0 }
        ev.pointee = queue.removeFirst()
        return 1
    }
}

@_cdecl("host_poll_event")
func hostPollEvent(_ ev: UnsafeMutablePointer<host_event>, _ waitMs: UInt32) -> Int32 {
    Events.pop(ev, waitMs)
}

// ---- the pad: touch (TouchPad.swift) ------------------------------------------------

@_cdecl("host_pad_read")
func hostPadRead(_ p: UnsafeMutablePointer<host_pad>) -> Int32 {
    var o = host_pad()
    o.pov = -1
    let s = TouchPad.shared.sample()
    o.x = s.x
    o.y = s.y
    o.buttons = s.buttons
    p.pointee = o
    return 1
}

// ---- audio out: a RemoteIO unit pulling the multiplexer's mix -------------------------

private var audioUnit: AudioComponentInstance?
private var audioFn: host_audio_fn?
private var audioUser: UnsafeMutableRawPointer?

private let audioRender: AURenderCallback = { _, _, _, _, frames, io in
    guard let io else { return noErr }
    let buf = UnsafeMutableAudioBufferListPointer(io)[0]
    guard let data = buf.mData else { return noErr }
    if let fn = audioFn {
        fn(data.assumingMemoryBound(to: Float.self), Int32(frames), audioUser)
    } else {
        memset(data, 0, Int(buf.mDataByteSize))
    }
    return noErr
}

@_cdecl("host_audio_open")
func hostAudioOpen(_ rate: Int32, _ fn: host_audio_fn?, _ user: UnsafeMutableRawPointer?) -> Int32 {
    if audioUnit != nil { return 1 }
    let session = AVAudioSession.sharedInstance()
    try? session.setCategory(.playback, mode: .default, options: [])
    try? session.setPreferredSampleRate(Double(rate))
    try? session.setPreferredIOBufferDuration(0.010)
    try? session.setActive(true)

    var desc = AudioComponentDescription(componentType: kAudioUnitType_Output,
                                         componentSubType: kAudioUnitSubType_RemoteIO,
                                         componentManufacturer: kAudioUnitManufacturer_Apple,
                                         componentFlags: 0, componentFlagsMask: 0)
    guard let comp = AudioComponentFindNext(nil, &desc) else { return 0 }
    var unit: AudioComponentInstance?
    guard AudioComponentInstanceNew(comp, &unit) == noErr, let unit else { return 0 }
    var fmt = AudioStreamBasicDescription(mSampleRate: Float64(rate), mFormatID: kAudioFormatLinearPCM,
                                          mFormatFlags: kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
                                          mBytesPerPacket: 8, mFramesPerPacket: 1, mBytesPerFrame: 8,
                                          mChannelsPerFrame: 2, mBitsPerChannel: 32, mReserved: 0)
    var cb = AURenderCallbackStruct(inputProc: audioRender, inputProcRefCon: nil)
    audioFn = fn
    audioUser = user
    if AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                            &fmt, UInt32(MemoryLayout.size(ofValue: fmt))) != noErr ||
        AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &cb, UInt32(MemoryLayout.size(ofValue: cb))) != noErr ||
        AudioUnitInitialize(unit) != noErr || AudioOutputUnitStart(unit) != noErr {
        AudioComponentInstanceDispose(unit)
        audioFn = nil
        return 0
    }
    audioUnit = unit
    return 1
}

@_cdecl("host_audio_close")
func hostAudioClose() {
    guard let unit = audioUnit else { return }
    AudioOutputUnitStop(unit)
    AudioUnitUninitialize(unit)
    AudioComponentInstanceDispose(unit)
    audioUnit = nil
    audioFn = nil
}
